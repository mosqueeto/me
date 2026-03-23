/*
 * init.c -- user key table, ~/.me/init file reader, pipe-buffer mechanism.
 *
 * Provides:
 *   user_execute()      -- check user key table before built-in dispatch
 *   pipe_interactive()  -- C-X | : prompt for shell cmd, pipe buffer through it
 *   read_init_file()    -- read ~/.me/init at startup
 *
 * Init file syntax (one directive per line, # comments):
 *   set varname value
 *   bind KEYNAME built-in-name
 *   bind KEYNAME | shell command...
 *   macro macroname
 *
 * Key name syntax (no internal spaces):
 *   M-x      Meta + x
 *   C-x ^x   Control + x
 *   C-Xx ^Xx C-X followed by x   (run the prefix together with the key)
 *
 * Pipe destination: by default output replaces the current buffer.
 * Append @buffername to the command to send output to a named buffer instead:
 *   bind M-r | pandoc -f markdown -t plain @render
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "ed.h"

/* ------------------------------------------------------------------ */
/* User key table                                                       */
/* ------------------------------------------------------------------ */

USER_KEY user_keys[NUSER_KEYS];
int      n_user_keys = 0;

/* ------------------------------------------------------------------ */
/* Key name parser                                                      */
/* ------------------------------------------------------------------ */

/*
 * Parse a key name into a key code integer.
 * Returns -1 on failure.
 */
int
parse_keyname(const char *s)
{
    if (!s || !*s) return -1;

    /* M-x : Meta key */
    if ((s[0]=='M' || s[0]=='m') && s[1]=='-' && s[2])
        return META | (unsigned char)s[2];

    /* Detect C-X or ^X prefix (written without a space: C-Xx or ^Xx) */
    int is_ctlx = 0;
    const char *p = s;

    if ((p[0]=='C' || p[0]=='c') && p[1]=='-' && (p[2]=='X' || p[2]=='x')) {
        is_ctlx = 1; p += 3;
    } else if (p[0]=='^' && (p[1]=='X' || p[1]=='x')) {
        is_ctlx = 1; p += 2;
    }

    if (is_ctlx) {
        if (!*p) return CTLX;                           /* bare C-X */
        if ((p[0]=='C'||p[0]=='c') && p[1]=='-' && p[2])
            return CTLX | CNTRL | toupper((unsigned char)p[2]);
        if (p[0]=='^' && p[1])
            return CTLX | CNTRL | toupper((unsigned char)p[1]);
        return CTLX | (unsigned char)p[0];
    }

    /* C-x or ^x : plain control key */
    if ((s[0]=='C' || s[0]=='c') && s[1]=='-' && s[2])
        return CNTRL | toupper((unsigned char)s[2]);
    if (s[0]=='^' && s[1])
        return CNTRL | toupper((unsigned char)s[1]);

    return -1;
}

/* ------------------------------------------------------------------ */
/* Built-in name lookup                                                 */
/* ------------------------------------------------------------------ */

static int (*lookup_builtin(const char *name))(int, int)
{
    KEYTAB *ktp = keytab;
    while (ktp->k_code != 0) {
        if (ktp->name && strcmp(ktp->name, name) == 0)
            return ktp->k_fp;
        ++ktp;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* User key table management                                            */
/* ------------------------------------------------------------------ */

/*
 * Add or update a user key binding.
 * fp non-NULL: rebind to built-in.
 * cmd non-NULL: pipe through shell command.
 */
static int
user_bind(int keycode, int (*fp)(int, int), const char *cmd)
{
    int i;
    for (i = 0; i < n_user_keys; i++) {
        if (user_keys[i].k_code == keycode) {
            user_keys[i].k_fp = fp;
            if (user_keys[i].k_cmd) { free(user_keys[i].k_cmd); user_keys[i].k_cmd = NULL; }
            if (cmd) user_keys[i].k_cmd = strdup(cmd);
            return TRUE;
        }
    }
    if (n_user_keys >= NUSER_KEYS) {
        mlwrite("user key table full");
        return FALSE;
    }
    user_keys[n_user_keys].k_code = keycode;
    user_keys[n_user_keys].k_fp   = fp;
    user_keys[n_user_keys].k_cmd  = cmd ? strdup(cmd) : NULL;
    n_user_keys++;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Pipe mechanism                                                       */
/* ------------------------------------------------------------------ */

/*
 * Write a buffer's text to file descriptor fd.
 */
static void
write_buffer_to_fd(BUFFER *bp, int fd)
{
    LINE *lp = lforw(bp->lines);
    while (lp != bp->lines) {
        if (lp->used > 0)
            write(fd, lp->text, lp->used);
        if (lp->flags & L_NL)
            write(fd, "\n", 1);
        lp = lforw(lp);
    }
}

/*
 * Replace buffer bp's contents with the bytes in data[0..len-1].
 * Does not prompt for confirmation — caller is responsible.
 */
static void
load_bytes_into_buffer(BUFFER *bp, BYTE *data, long len)
{
    BYTE   *p = data;
    BYTE   *end = data + len;
    BYTE   *nl;
    LINE   *lp;
    WINDOW *wp;
    int     linelen;

    /* Force-clear without the "discard changes?" prompt */
    bp->flag &= ~BFCHG;
    while ((lp = lforw(bp->lines)) != bp->lines)
        lfree(lp);
    bp->dotp  = bp->lines;
    bp->doto  = 0;
    bp->markp = NULL;
    bp->marko = 0;

    while (p < end) {
        nl = (BYTE *)memchr(p, '\n', end - p);
        linelen = nl ? (int)(nl - p) : (int)(end - p);

        lp = ladd(bp->lines, linelen);
        if (!lp) break;
        if (linelen > 0)
            memcpy(lp->text, p, linelen);
        lp->used  = linelen;
        lp->flags = nl ? L_NL : 0;

        if (bp->dotp == bp->lines)
            bp->dotp = lp;

        p += linelen + (nl ? 1 : 0);
    }

    /* Reset all windows showing this buffer */
    for (wp = wheadp; wp != NULL; wp = wp->wndp) {
        if (wp->bufp == bp) {
            wp->topp  = lforw(bp->lines);
            wp->dotp  = lforw(bp->lines);
            wp->doto  = 0;
            wp->markp = NULL;
            wp->marko = 0;
            wp->flag |= WFHARD;
        }
    }
    bp->flag |= BFCHG;
}

/*
 * Parse optional @buffername suffix from cmd_buf.
 * If found, truncates cmd_buf at the @ (trimming trailing whitespace)
 * and returns a pointer to the name portion within cmd_buf.
 * Returns NULL if no @name suffix.
 */
static char *
extract_dest_buffer(char *cmd_buf)
{
    char *at = strrchr(cmd_buf, '@');
    if (!at) return NULL;

    char *name = at + 1;
    if (!*name) return NULL;

    /* @name must reach end of string (no trailing content) */
    char *p = name;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p != '\0') return NULL;

    /* Trim whitespace before @ and terminate */
    char *q = at;
    while (q > cmd_buf && isspace((unsigned char)*(q-1))) q--;
    *q = '\0';

    return name;
}

/*
 * Core pipe primitive.
 * Pipes src buffer through shell command cmd.
 * Result goes into dst buffer (may be same as src for in-place filter).
 * Returns TRUE on success.
 */
int
do_pipe(BUFFER *src, const char *cmd, BUFFER *dst)
{
    char    tmpfile[64];
    int     tmp_fd;
    int     out_pipe[2];
    pid_t   pid;
    BYTE   *out_buf;
    long    out_len = 0;
    long    out_cap = 8192;
    ssize_t nr;
    BYTE    chunk[4096];
    int     status;

    /* Write src to a temp file to avoid pipe deadlock */
    strcpy(tmpfile, "/tmp/me_pipe_XXXXXX");
    if ((tmp_fd = mkstemp(tmpfile)) < 0) {
        mlwrite("pipe: can't create temp file");
        return FALSE;
    }
    write_buffer_to_fd(src, tmp_fd);
    close(tmp_fd);

    if (pipe(out_pipe) < 0) {
        unlink(tmpfile);
        mlwrite("pipe: can't create pipe");
        return FALSE;
    }

    pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        unlink(tmpfile);
        mlwrite("pipe: fork failed");
        return FALSE;
    }

    if (pid == 0) {
        /* Child: stdin from temp file, stdout to pipe */
        int in_fd = open(tmpfile, O_RDONLY);
        if (in_fd < 0) _exit(1);
        dup2(in_fd, 0);
        dup2(out_pipe[1], 1);
        close(in_fd);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(1);
    }

    /* Parent: read all output */
    close(out_pipe[1]);
    out_buf = (BYTE *)malloc(out_cap);
    if (!out_buf) {
        close(out_pipe[0]);
        waitpid(pid, &status, 0);
        unlink(tmpfile);
        mlwrite("pipe: out of memory");
        return FALSE;
    }
    while ((nr = read(out_pipe[0], chunk, sizeof(chunk))) > 0) {
        if (out_len + nr >= out_cap) {
            out_cap = (out_cap + nr) * 2;
            BYTE *tmp = (BYTE *)realloc(out_buf, out_cap);
            if (!tmp) {
                free(out_buf);
                close(out_pipe[0]);
                waitpid(pid, &status, 0);
                unlink(tmpfile);
                mlwrite("pipe: out of memory");
                return FALSE;
            }
            out_buf = tmp;
        }
        memcpy(out_buf + out_len, chunk, nr);
        out_len += nr;
    }
    close(out_pipe[0]);
    waitpid(pid, &status, 0);
    unlink(tmpfile);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(out_buf);
        mlwrite("pipe: command exited %d", WEXITSTATUS(status));
        return FALSE;
    }

    load_bytes_into_buffer(dst, out_buf, out_len);
    free(out_buf);
    mlwrite("[pipe: %ld bytes]", out_len);
    return TRUE;
}

/*
 * Interactive pipe command, bound to C-X |.
 * Prompts for a shell command, pipes current buffer through it.
 * Output replaces current buffer, or goes to @buffername if specified.
 */
int
pipe_interactive(int f, int n)
{
    char    cmd[NLINE];
    char   *dest_name;
    BUFFER *dst;

    (void)defaultargs(f, n);
    if (mlreply("| ", (BYTE *)cmd, NLINE) != TRUE)
        return FALSE;

    dest_name = extract_dest_buffer(cmd);
    if (dest_name && *dest_name) {
        dst = bfind((BYTE *)dest_name, TRUE, BFTEMP);
        if (!dst) {
            mlwrite("pipe: can't create buffer %s", dest_name);
            return FALSE;
        }
    } else {
        dst = curbp;
    }

    return do_pipe(curbp, cmd, dst);
}

/* ------------------------------------------------------------------ */
/* User key dispatch                                                    */
/* ------------------------------------------------------------------ */

/*
 * Check the user key table for key c.
 * If found: dispatch and return the result (TRUE or FALSE).
 * If not found: return -1 so execute() falls through to built-ins.
 */
int
user_execute(int c, int f, int n)
{
    int     i;
    char    cmd[NLINE];
    char   *dest_name;
    BUFFER *dst;
    int     status;

    for (i = 0; i < n_user_keys; i++) {
        if (user_keys[i].k_code != c) continue;

        if (user_keys[i].k_fp) {
            thisflag = 0;
            status = (*user_keys[i].k_fp)(f, n);
            lastflag = thisflag;
            return status;
        }

        if (user_keys[i].k_cmd) {
            strncpy(cmd, user_keys[i].k_cmd, NLINE - 1);
            cmd[NLINE - 1] = '\0';
            dest_name = extract_dest_buffer(cmd);
            if (dest_name && *dest_name) {
                dst = bfind((BYTE *)dest_name, TRUE, BFTEMP);
                if (!dst) dst = curbp;
            } else {
                dst = curbp;
            }
            return do_pipe(curbp, cmd, dst);
        }
    }
    return -1;  /* not in user table */
}

/* ------------------------------------------------------------------ */
/* Variable setter (for "set" directive)                               */
/* ------------------------------------------------------------------ */

static void
init_set(const char *var, const char *val)
{
    int ival = atoi(val);
    if      (strcmp(var, "rmarg")    == 0 || strcmp(var, "rm") == 0) rmarg    = ival;
    else if (strcmp(var, "lmarg")    == 0 || strcmp(var, "lm") == 0) lmarg    = ival;
    else if (strcmp(var, "tabsize")  == 0 || strcmp(var, "t")  == 0) tabsize  = ival;
    else if (strcmp(var, "softtabs") == 0 || strcmp(var, "T")  == 0) softtabs = ival;
    /* unknown variables silently ignored */
}

/* ------------------------------------------------------------------ */
/* rc directory initialisation                                          */
/* ------------------------------------------------------------------ */

static const char readme_text[] =
"~/.me/ -- ME editor state and configuration directory\n"
"\n"
"FILES\n"
"  init        startup configuration (read at launch)\n"
"  kbm_file    saved keystroke macro (persists across sessions)\n"
"  README      this file\n"
"\n"
"SUBDIRECTORIES\n"
"  macros/     named keystroke macros, one file per macro\n"
"              loaded with the 'macro' directive in init,\n"
"              or saved/restored manually\n"
"\n"
"INIT FILE SYNTAX\n"
"  One directive per line.  Lines beginning with # are comments.\n"
"\n"
"  set varname value\n"
"      Set an editor variable.  Known variables:\n"
"        rmarg  (or rm)  right margin column        default 77\n"
"        lmarg  (or lm)  left margin column         default 0\n"
"        tabsize (or t)  tab display width           default 4\n"
"        softtabs (or T) use spaces for tabs (1/0)  default 1\n"
"\n"
"  bind KEYNAME built-in-name\n"
"      Rebind a key to a built-in command, e.g.:\n"
"        bind M-r forwsearch\n"
"      Built-in names are listed in the M-h help screen.\n"
"\n"
"  bind KEYNAME | shell command...\n"
"      Bind a key to pipe the current buffer through a shell command.\n"
"      Output replaces the current buffer by default.  Append\n"
"      @buffername to send output to a named buffer instead:\n"
"        bind M-r | pandoc -f markdown -t plain @render\n"
"        bind M-f | fmt -72\n"
"        bind M-s | sort\n"
"        bind M-g | ~/.me/render-html @html\n"
"\n"
"  macro macroname\n"
"      Load a saved macro from ~/.me/macros/macroname at startup.\n"
"\n"
"KEY NAME SYNTAX (no internal spaces -- run prefix and key together)\n"
"  M-x      Meta (ESC) + x\n"
"  C-x      Control + x  (also written ^x)\n"
"  C-Xx     C-X prefix then x  (also written ^Xx)\n"
"  C-X^x    C-X prefix then Control+x\n"
"\n"
"INTERACTIVE PIPE COMMAND\n"
"  C-X |    prompts for a shell command, pipes current buffer through it.\n"
"           Append @buffername to output to a named buffer.\n"
"           Shell scripts in ~/.me/ can be invoked by path:\n"
"             | ~/.me/render-md @preview\n"
;

/*
 * Create ~/.me/ and its subdirectories if they don't exist.
 * Write a README on first creation.
 */
void
init_rc_dir(BYTE *rc_dir)
{
    char path[NFILEN];
    int  is_new = 0;
    FILE *fp;

    if (access((char *)rc_dir, R_OK|W_OK|X_OK) != 0) {
        if (mkdir((char *)rc_dir, 0700) != 0) {
            printf("Note: couldn't create state directory %s\r\n",
                   (char *)rc_dir);
            sleep(2);
            return;
        }
        printf("Note: initializing state directory %s\r\n", (char *)rc_dir);
        sleep(2);
        is_new = 1;
    }

    /* Create macros/ subdirectory if missing */
    snprintf(path, sizeof(path), "%s/macros", (char *)rc_dir);
    if (access(path, R_OK|W_OK|X_OK) != 0)
        mkdir(path, 0700);

    /* Write README on first creation (don't overwrite if it exists) */
    snprintf(path, sizeof(path), "%s/README", (char *)rc_dir);
    if (is_new || access(path, F_OK) != 0) {
        fp = fopen(path, "w");
        if (fp) {
            fputs(readme_text, fp);
            fclose(fp);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Init file reader                                                     */
/* ------------------------------------------------------------------ */

/*
 * Read and process ~/.me/init.
 * Called from edinit() after the macro file is restored.
 *
 * Supported directives:
 *   set varname value
 *   bind KEYNAME built-in-name
 *   bind KEYNAME | shell command string...
 *   macro macroname
 */
void
read_init_file(BYTE *rc_dir)
{
    char  path[NFILEN];
    char  line[NLINE];
    char *p, *keyword, *rest;
    FILE *fp;
    int   keycode;
    int (*builtin)(int, int);

    snprintf(path, sizeof(path), "%s/init", (char *)rc_dir);
    fp = fopen(path, "r");
    if (!fp) return;    /* no init file is fine */

    while (fgets(line, sizeof(line), fp)) {
        /* strip trailing newline */
        p = line + strlen(line) - 1;
        while (p >= line && (*p == '\n' || *p == '\r' || *p == ' ')) *p-- = '\0';

        /* skip blank lines and comments */
        p = line;
        while (isspace((unsigned char)*p)) p++;
        if (!*p || *p == '#') continue;

        /* first token: directive keyword */
        keyword = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) { *p++ = '\0'; while (isspace((unsigned char)*p)) p++; }
        rest = p;

        /* ---- set varname value ---- */
        if (strcmp(keyword, "set") == 0) {
            char *var = rest;
            while (*rest && !isspace((unsigned char)*rest)) rest++;
            if (*rest) { *rest++ = '\0'; while (isspace((unsigned char)*rest)) rest++; }
            if (*var && *rest)
                init_set(var, rest);

        /* ---- bind KEYNAME ... ---- */
        } else if (strcmp(keyword, "bind") == 0) {
            /* next token: key name (no internal spaces — use C-Xx not "C-X x") */
            char *keystr = rest;
            while (*rest && !isspace((unsigned char)*rest)) rest++;
            if (*rest) { *rest++ = '\0'; while (isspace((unsigned char)*rest)) rest++; }
            if (!*keystr || !*rest) continue;

            keycode = parse_keyname(keystr);
            if (keycode < 0) continue;  /* unrecognised key name */

            if (rest[0] == '|') {
                /* pipe command */
                rest++;
                while (isspace((unsigned char)*rest)) rest++;
                if (*rest)
                    user_bind(keycode, NULL, rest);
            } else {
                /* built-in function name */
                builtin = lookup_builtin(rest);
                if (builtin)
                    user_bind(keycode, builtin, NULL);
            }

        /* ---- macro macroname ---- */
        } else if (strcmp(keyword, "macro") == 0) {
            if (*rest) {
                char mpath[512];
                snprintf(mpath, sizeof(mpath), "%s/macros/%s",
                         (char *)rc_dir, rest);
                rest_kbdm((BYTE *)mpath);
            }
        }
        /* unknown directives silently ignored */
    }

    fclose(fp);
}
