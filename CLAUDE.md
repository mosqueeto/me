# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**ME** is a terminal-based text editor written in C, derived from the public domain microEmacs by Dave G. Conroy. The editor binary is called `me` (symlinked as `v`), currently at version 2.01.

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
| `init.c` | User key table, `~/.me/init` file reader, pipe-buffer mechanism |
| `window.c` | Split/resize/switch windows |

**Control flow**: `main()` initialises the terminal and buffers, then runs the event loop: read key → look up in `keytab[]` → call bound function → redisplay.

**Encryption**: enabled at compile time via `#define CRYPT_S 1` in `ed.h`. Encrypted files begin with the magic prefix `#ME1.42$` followed by a secret hash.

**Regex**: PCRE is used for all search/replace (integrated in v1.82). The search state lives in `search.c`/`search.h`.

**Vi mode**: accessible via ESC-ESC toggle; alternative key bindings alongside the default Emacs-style bindings.

## v2.00 Design Decisions (init.c)

### The `~/.me/` ecosystem
All user configuration and state lives in `~/.me/`:
- `init` — startup config file (read by `read_init_file()` in `init.c`)
- `kbm_file` — persisted keystroke macro (single macro, saved/restored on exit/startup)
- `macros/` — named keystroke macros, one file per macro (same raw byte format as `kbm_file`)
- `README` — auto-generated on first creation; describes the structure and init syntax

`init_rc_dir()` creates the directory, `macros/` subdir, and README on first run. It is called from `edinit()` in `main.c`.

### User key table
A small dynamic table (`user_keys[]`, max `NUSER_KEYS=64`) in `init.c`, searched **before** the built-in `keytab[]` in `execute()`. Each entry holds either a function pointer (built-in rebind) or a shell command string (pipe binding). `user_execute()` returns -1 if the key is not in the user table, so `execute()` falls through to built-ins normally.

The `KEYTAB` typedef was moved from `main.c` into `ed.h` (v2.00) so `init.c` can reference it.

### Init file syntax
```
# comment
set varname value          # known vars: rmarg/rm, lmarg/lm, tabsize/t, softtabs/T
bind KEYNAME built-in      # rebind to existing command by name (names from keytab[].name)
bind KEYNAME | shell cmd   # bind key to pipe current buffer through shell command
macro macroname            # load ~/.me/macros/macroname as the keystroke macro at startup
```
Key name syntax (no internal spaces — run prefix and key together):
- `M-x` → Meta+x, `C-x` or `^x` → Control+x
- `C-Xx` or `^Xx` → C-X prefix then x, `C-X^x` → C-X then Control+x

### Pipe-buffer mechanism (`C-X |`)
`pipe_interactive()` prompts for a shell command and pipes the current buffer through it via fork/exec. The buffer is written to a temp file (avoids pipe deadlock), child stdout is captured and loaded back. Output replaces the current buffer by default; append `@buffername` to the command to send output to a named buffer instead.

`do_pipe(src, cmd, dst)` is the reusable primitive — also called by `user_execute()` for init-file pipe bindings.

### What was considered and set aside
- **Lua embedding** (`lua/` subdir): overkill; exposing ME's internals to a Lua runtime is a significant chore for modest gain.
- **GPG encryption**: would be more secure than the existing Blowfish implementation, but legacy files (magic prefix `#ME1.42$`) must remain readable. GPG via the pipe mechanism is a natural future addition — additive, not a replacement.

### Future work (not yet implemented)
- Named macro library: load/save by name to `~/.me/macros/`; currently only startup load via `macro` directive is wired up
- Pipe output to new buffer with window management (currently @buffername creates the buffer but doesn't split/switch windows automatically)
- Rendering via pandoc: `bind M-r | pandoc -f markdown -t plain @render`

## Notes / Todo

Development notes are in `Notes`; outstanding work in `todo`. The `old/` subdirectory holds deprecated implementations (old Blowfish i386 asm, legacy file I/O). `tests/` contains test data and a Perl integration test script (`intreg.pl`).
