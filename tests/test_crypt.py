#!/usr/bin/env python3
"""
Regression tests for ME's file encryption (crypt_buf.c).

Covers the #ME2.00$ format (AES-256-GCM + scrypt) and, crucially, backward
compatibility: legacy #ME1.42$ files (Blowfish-CBC) must keep decrypting.

Drives the real `me` binary through a pty via pty_drive.py.  Run from anywhere:

    python3 tests/test_crypt.py

Requires the repo's legacy fixture `enc-file-pw-fiddle` (password: fiddle),
which decrypts to  b'This is a test encrypted file...\\n'.
"""
import sys, os, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import pty_drive

FIDDLE_SRC = os.path.join(REPO, "enc-file-pw-fiddle")
FIDDLE_PLAIN = b'This is a test encrypted file...\n'

# keystrokes
def MENC(pw, name):  # M-E encrypt: passwd, again, write-file
    return [b'\x1bE', pw + b'\r', pw + b'\r', name + b'\r', b'\x18\x03']
def MDEC(name):      # M-D decrypt-to-plaintext: write-file
    return [b'\x1bD', name + b'\r', b'\x18\x03']
QUIT = b'\x18\x03'

def main():
    if not os.path.exists(FIDDLE_SRC):
        print("SKIP: missing fixture", FIDDLE_SRC); return 0
    work = tempfile.mkdtemp(prefix="me_crypt_")
    home = os.path.join(work, "home"); os.makedirs(home)
    os.environ["HOME"] = home           # sandbox ~/.me
    os.chdir(work)
    shutil.copy(FIDDLE_SRC, "fiddle.enc")

    def rd(p):
        with open(p, "rb") as f: return f.read()
    R = {}

    # 1. backward compat: legacy #ME1.42$ decrypts on the current binary
    pty_drive.drive("fiddle.enc", "l1", [b'fiddle\r'] + MDEC(b'fiddle.out'))
    R['legacy #ME1.42$ still decrypts'] = os.path.exists("fiddle.out") and rd("fiddle.out") == FIDDLE_PLAIN

    # 2. new-format round trip + magic + confidentiality
    with open("p.txt", "w") as f: f.write("hello v2 world\nsecond line\n")
    pty_drive.drive("p.txt", "l2", MENC(b'hunter2', b'v2.enc'))
    R['new save is #ME2.00$'] = os.path.exists("v2.enc") and rd("v2.enc")[:8] == b'#ME2.00$'
    R['ciphertext hides plaintext'] = os.path.exists("v2.enc") and b'hello v2' not in rd("v2.enc")
    pty_drive.drive("v2.enc", "l2b", [b'hunter2\r'] + MDEC(b'v2.out'))
    R['new format round-trips'] = os.path.exists("v2.out") and rd("v2.out") == b'hello v2 world\nsecond line\n'

    # 3. upgrade-on-save: legacy -> #ME2.00$, decrypts with the typed password
    pty_drive.drive("fiddle.enc", "l3", [b'fiddle\r'] + MENC(b'newpw', b'up.enc'))
    R['legacy upgrades to #ME2.00$ on save'] = os.path.exists("up.enc") and rd("up.enc")[:8] == b'#ME2.00$'
    pty_drive.drive("up.enc", "l3b", [b'newpw\r'] + MDEC(b'up.out'))
    R['upgraded file decrypts with typed pw'] = os.path.exists("up.out") and rd("up.out") == FIDDLE_PLAIN

    # 4. wrong password rejected by the GCM tag
    pty_drive.drive("v2.enc", "l4", [b'WRONGpw\r', QUIT])
    R['wrong password rejected'] = b'cannot be decrypted' in rd("l4")

    # 5. per-file salt: identical plaintext+password -> different blob each time
    with open("s.txt", "w") as f: f.write("identical content\n")
    pty_drive.drive("s.txt", "l5a", MENC(b'pw', b'a.enc'))
    pty_drive.drive("s.txt", "l5b", MENC(b'pw', b'b.enc'))
    A, B = rd("a.enc"), rd("b.enc")
    R['per-file salt differs'] = A[8:24] != B[8:24]
    R['ciphertext differs per encryption'] = A != B

    ok = True
    for k in sorted(R):
        print(("PASS" if R[k] else "FAIL"), "-", k); ok = ok and R[k]
    shutil.rmtree(work, ignore_errors=True)
    print("=== ALL PASS ===" if ok else "=== FAILURES ===")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
