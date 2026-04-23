/*
 * crypt.c -- standalone ME file encrypt/decrypt utility.
 *
 * Uses the same Blowfish CBC format as the ME editor (magic #ME1.42$),
 * so files encrypted here can be opened directly in me, and vice versa.
 *
 * Usage: crypt [-d] [-p password] [infile [outfile]]
 *   -d          decrypt (default: encrypt)
 *   -p password supply password on command line (insecure; omit to be prompted)
 *   infile      input  file (default: stdin)
 *   outfile     output file (default: stdout)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ed.h"
#include "crypt.h"

extern BYTE *encrypt_buf(BYTE *, BYTE *, int *);
extern BYTE *decrypt_buf(BYTE *, BYTE *, long *);

static void
die(const char *s)
{
    fprintf(stderr, "crypt: %s\n", s);
    exit(1);
}

static void
usage(void)
{
    fprintf(stderr, "usage: crypt [-d] [-p password] [infile [outfile]]\n");
    exit(1);
}

static void
read_password(const char *prompt, char *buf, int size)
{
    struct termios old, noecho;
    int tty = isatty(fileno(stdin));
    int len;

    fprintf(stderr, "%s", prompt);
    fflush(stderr);

    if (tty) {
        tcgetattr(fileno(stdin), &old);
        noecho = old;
        noecho.c_lflag &= ~(tcflag_t)(ECHO | ECHONL);
        tcsetattr(fileno(stdin), TCSANOW, &noecho);
    }
    if (fgets(buf, size, stdin) == NULL)
        buf[0] = '\0';
    if (tty) {
        tcsetattr(fileno(stdin), TCSANOW, &old);
        fputc('\n', stderr);
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';
}

static BYTE *
slurp(int fd, long *size)
{
    struct stat st;
    BYTE *buf;
    long cap;
    ssize_t n;

    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        *size = st.st_size;
        buf = (BYTE *)malloc(*size + 1);
        if (!buf) die("out of memory");
        n = read(fd, buf, *size);
        if (n < 0) die("read error");
        *size = n;
        return buf;
    }
    /* pipe or special file: grow on demand */
    cap = 65536;
    *size = 0;
    buf = (BYTE *)malloc(cap);
    if (!buf) die("out of memory");
    while ((n = read(fd, buf + *size, cap - *size)) > 0) {
        *size += n;
        if (*size == cap) {
            BYTE *tmp;
            cap *= 2;
            tmp = (BYTE *)realloc(buf, cap);
            if (!tmp) die("out of memory");
            buf = tmp;
        }
    }
    if (n < 0) die("read error");
    return buf;
}

int
main(int argc, char *argv[])
{
    int    c, in, out, do_decrypt = 0;
    char   pw1[128], pw2[128];
    BYTE  *ibuf, *obuf;
    long   isz;
    int    osz;
    ssize_t written;

    pw1[0] = '\0';

    while ((c = getopt(argc, argv, "dp:")) != -1) {
        switch (c) {
        case 'd': do_decrypt = 1;                              break;
        case 'p': strncpy(pw1, optarg, sizeof(pw1) - 1);      break;
        default:  usage();
        }
    }
    argc -= optind;
    argv += optind;
    if (argc > 2) usage();

    if (!pw1[0]) {
        if (argc < 1)
            die("must supply -p when reading from stdin");
        read_password("  pw: ", pw1, sizeof(pw1));
        if (!do_decrypt) {
            read_password("again: ", pw2, sizeof(pw2));
            if (strcmp(pw1, pw2) != 0) die("passwords don't match");
            memset(pw2, 0, sizeof(pw2));
        }
    }

    in  = (argc >= 1) ? open(argv[0], O_RDONLY)                       : 0;
    out = (argc >= 2) ? open(argv[1], O_WRONLY|O_CREAT|O_TRUNC, 0600) : 1;
    if (in  < 0) { perror(argv[0]); exit(1); }
    if (out < 0) { perror(argv[1]); exit(1); }

    ibuf = slurp(in, &isz);
    if (in != 0) close(in);

    if (do_decrypt) {
        if (isz < 8 || memcmp(ibuf, ME_MAGIC, 8) != 0)
            die("not an ME encrypted file");
        obuf = decrypt_buf((BYTE *)pw1, ibuf, &isz);
        if (!obuf) die("decryption failed (wrong password or corrupt file)");
        written = write(out, obuf, (size_t)isz);
        if (written != (ssize_t)isz) die("write error");
        free(obuf);
    } else {
        osz = (int)isz;
        obuf = encrypt_buf((BYTE *)pw1, ibuf, &osz);
        if (!obuf) die("encryption failed");
        written = write(out, obuf, (size_t)osz);
        if (written != (ssize_t)osz) die("write error");
        free(obuf);
    }

    free(ibuf);
    memset(pw1, 0, sizeof(pw1));
    if (out != 1) close(out);
    return 0;
}
