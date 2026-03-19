# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**ME** is a terminal-based text editor written in C, derived from the public domain microEmacs by Dave G. Conroy. The editor binary is called `me` (symlinked as `v`), currently at version 1.96.

## Build Commands

```sh
make me        # Build the editor
make clean     # Remove object files and backups (,,* files)
make crypt     # Build standalone encryption utility
make efme      # Build with Electric Fence for memory debugging
make pecos     # Install to /home/kent/bin/ (local machine)
make songbird  # Install to /usr/local/bin/
make tar       # Create me.tar distribution archive
```

**Dependencies** (install if missing):
```sh
sudo apt install libncurses5-dev libpcre3-dev
```

**Compiler flags**: `-g -Wno-pedantic -fPIC -Wno-implicit`
**Libraries**: `-lc -ltermcap -lpcre`

All `.c` files depend on `ed.h`, `search.h`, and `crypt.h` — changing any header triggers a full rebuild.

## Architecture

The editor uses a linked-list line model. The core data structures are defined in `ed.h`:

- `LINE` — doubly-linked circular list node holding a line's text
- `BUFFER` — named text buffer; holds a `LINE` ring, file metadata, and mode flags
- `WINDOW` — display viewport into a buffer; holds cursor position
- `REGION` — a selected range (start line/offset + byte count)
- `TERM` — terminal abstraction with function pointers (move, eeol, open, etc.)

**Key files and responsibilities**:

| File | Role |
|------|------|
| `main.c` | Entry point, key binding table (`keytab[]`), version history |
| `ed.h` | All struct definitions and global declarations; `CRYPT_S 1` enables encryption |
| `display.c` | Screen redisplay engine (virtual/real screen diffing) |
| `tcap.c` / `termio.c` | Terminal capability queries and raw-mode I/O |
| `buffer.c` / `line.c` | Buffer/line allocation and manipulation primitives |
| `file.c` | File read/write, backup files (prefix `,,`), external-change detection |
| `crypt_buf.c` | Blowfish-encrypted file format; magic prefix `#ME1.42$` |
| `bf.c` / `md5.c` | Blowfish cipher and MD5 (used for key derivation) |
| `search.c` / `pcre.c` | Search/replace; wraps PCRE for regex |
| `keys.c` | Key sequence parsing and dispatch |
| `random.c` | Miscellaneous commands (set-mark, exchange-point-mark, etc.) |
| `region.c` | Kill/yank (cut/copy/paste) on regions |
| `word.c` | Word-level movement and operations |
| `spawn.c` | Shell command execution from within the editor |
| `window.c` | Split/resize/switch windows |

**Control flow**: `main()` initialises the terminal and buffers, then runs the event loop: read key → look up in `keytab[]` → call bound function → redisplay.

**Encryption**: enabled at compile time via `#define CRYPT_S 1` in `ed.h`. Encrypted files begin with the magic prefix `#ME1.42$` followed by a secret hash.

**Regex**: PCRE is used for all search/replace (integrated in v1.82). The search state lives in `search.c`/`search.h`.

**Vi mode**: accessible via ESC-ESC toggle; alternative key bindings alongside the default Emacs-style bindings.

## Notes / Todo

Development notes are in `Notes`; outstanding work in `todo`. The `old/` subdirectory holds deprecated implementations (old Blowfish i386 asm, legacy file I/O). `tests/` contains test data and a Perl integration test script (`intreg.pl`).
