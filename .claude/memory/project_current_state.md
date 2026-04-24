---
name: ME project current state
description: Version, recent features added, and pending todo items as of ME2.08 + wrap fixes + layered init + named macros
type: project
originSessionId: 7783afe9-34e1-4314-b733-f8d89232a78d
---
Current version is **ME2.08** (uncommitted work on top). Working directory is /home/kent/db/git/me.

## Changes made in this session (not yet committed)

**Wrap mode fixes (word.c, random.c)**:
- `wrap_insert`: EOL case (cursor >= len) falls back to `wrapword`; cursor-past-rmarg with text ahead searches backward from rmarg for split, moves cursor to continuation line at correct offset; cascade loop tracks cursor and follows it if a split lands before it
- `wrap_split`: in MDWRAP mode, always prepends tail to existing next line if it has content (not just when L_SNL set); enables cascade through hard-newline paragraphs
- `backdel`: when backspace crosses a soft-newline boundary (cursor lands at end of L_SNL line), skips `ldelete` entirely — soft-newline is the "ghost space" consumed; `wrap_delete` then pulls words up normally

**Layered init (init.c, main.c)**:
- `parse_init_fp(FILE*, macro_dir)` extracted as inner loop; `read_init_file` and new `read_init_path` call it
- `edinit()` now reads `~/.me/init` then `./.me/init` (if it exists and has different inode from home)
- `-i <path>` flag: reads a file or directory as a third init layer (highest priority)
- Later `bind`/`set`/`def` definitions overwrite earlier ones automatically

**Named macros (init.c, ed.h, main.c)**:
- `def name | shell cmd` directive in init file defines a named macro
- `M-m` prompts "Macro: ", user types name + Enter, executes via `do_pipe`
- `M-M` now toggles mouse (was `M-m`)
- `NAMED_MACRO` struct, `named_macros[]` table (max 64), `named_bind()`, `named_macro()` added

## Pending todo items

- **Man page**: produce a man page. Ready to do on request — ~one session, 300-400 lines troff.
- **Git interface**: git pull before open, git commit+push on close ("git mode").
- **Question about bind M-g example in README**: needs clarification.
