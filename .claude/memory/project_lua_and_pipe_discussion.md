---
name: Lua embedding and pipe/config discussion
description: Summary of interrupted session discussing lua embedding, config files, encryption via gpg, and buffer piping
type: project
---

User experimented with embedding Lua into the editor; remnants are in the `lua/` subdirectory.

Claude's assessment: Lua embedding is overkill and would be a chore to meaningfully expose `me`'s internal data structures and operations to the Lua runtime.

**Alternative discussed — startup config file:**
A config file read at startup could handle a lot of what Lua would be used for. At one point user experimented with saving macro definitions across sessions (current status unknown — worth checking).

**Encryption discussion:**
Alternative to the current Blowfish (`crypt_buf.c`) approach: pipe the buffer through `gpg` to the output file. Main complication: how to get the key/passphrase to gpg cleanly.

**Buffer piping (the main thread):**
Discussion of a general mechanism for piping buffers — either to a file or to another buffer. Possibly expressing pipe/shell-script syntax in the initialization/config file. This was considered interesting and promising.

**Markdown/asciidoc rendering:**
Pipe buffer through `pandoc` (or similar) to render to a scratch buffer — a natural application of the pipe mechanism. No special-casing needed if pipe-buffer-through-command is general enough.

**GPG coexistence:**
GPG would be more secure than the ad-hoc Blowfish implementation, but legacy encrypted files (magic prefix `#ME1.42$`) exist and must remain readable. GPG encryption would need to coexist with the Blowfish path for some time — not a replacement, an addition.

**`.me/` as script directory:**
`~/.me/` already holds state (kbm_file for macro persistence). It could also hold user shell scripts that the pipe mechanism invokes by name — so the config/pipe/script system all lives in one place.

**Named/indexed macro library:**
Currently only one macro survives across sessions (`~/.me/kbm_file`). Old idea: save multiple macros to `~/.me/`, retrievable by index or name. The macro format is already just a byte array (`kbdm[]`). A macro library could be a directory `~/.me/macros/` with files named by the user, or a single file with a simple index. Retrieval could be a prompt: `load-macro [name]`. This is orthogonal to the pipe/init-file work but fits naturally in the `~/.me/` ecosystem.

**Why:** User found the discussion valuable and wants to continue it. Session was interrupted by a system reboot before conclusions were reached.
**How to apply:** Pick up these threads when the user wants to continue. Don't re-litigate the Lua question — that's settled. GPG is additive, not a Blowfish replacement.
