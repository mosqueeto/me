---
name: ME project current state
description: Version, recent features added, and pending todo items as of ME2.08
type: project
originSessionId: 7783afe9-34e1-4314-b733-f8d89232a78d
---
Current version is **ME2.08**, committed and pushed to GitHub (mosqueeto/me.git). The working directory is /home/kent/db/git/me.

## What was added in ME2.08 (this session)

**Pipe/@buffer fixes (init.c)**:
- `%ld` → `%D` in mlwrite (mlwrite uses %D for long, not standard %ld)
- @buffername dest was created as BFTEMP — invisible in buffer list, rejected on re-use; fixed to use flag=0
- Child stderr redirected to /dev/null so failed commands don't corrupt the terminal display
- Silent fallback to curbp removed — now errors out properly

**setvar (M-s) improvements (random.c)**:
- Empty Var: prompt → shows all current variable values on message line
- Name-only entry → shows current value without changing it
- Fixed atoi("")=0 silent-zero bug on empty Val: prompt
- `init_vars()` extracted and called at startup so variable table always ready

**Init file override warning (init.c, main.c)**:
- If init file binds a key that shadows a built-in, `init_warning[]` global is set
- Displayed in message line after startup (visible until first keypress)

**Crypt utility rewrite (crypt.c, crypt_buf.c)**:
- Standalone `crypt` now uses same Blowfish CBC format as editor — files are interchangeable
- Proper echo suppression via termios
- IV now from /dev/urandom (was time()-based)
- MD5 verification moved inside decrypt_buf(); returns NULL on wrong password
- pad_size validated before md5string call (prevents segfault on bad password)
- Makefile: crypt links crypt_buf.o and md5.o

**asciify command (M-8) (random.c, main.c)**:
- Converts 8-bit chars to 7-bit ASCII via table
- Covers Windows-1252 0x80-0x9F (smart quotes→'", em dash→--, ellipsis→..., trademark→(tm), etc.)
- Latin-1 punctuation 0xA0-0xBF (NBSP→space, copyright→(c), etc.)
- Accented letters 0xC0-0xFF stripped to base ASCII
- Reports "[asciify: N replacements]"

**8-bit display fix (display.c)**:
- Fixed > 0x80 off-by-one to >= 0x80
- 0x80-0x9F: shown as highlighted '?' (can't render safely in UTF-8 terminal)
- 0xA0-0xFF: shown as 7-bit approximation highlighted (also correct for selection display)
- Removed broken | 0x20 transformation

## Pending todo items (from todo file)

- **Local .me directory**: have me check for a .me directory in the current working directory; settings there should overwrite/supplement ~/.me/init. Allows per-project environments.
- **Man page**: produce a man page documenting all keystroke commands. Kent asked for an estimate — answer was "one session, ~30-45 min, 300-400 lines of troff, 4-5 pages". Ready to do on request.
- **Git interface**: a "git mode" that does git pull before opening a file and git commit + push on close.
- **Question about bind M-g example in README**: needs clarification/investigation.

**Why:** These items came up in discussion and are the natural next steps.
**How to apply:** Pick up from here on the desktop after `git pull`.
