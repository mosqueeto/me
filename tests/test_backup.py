#!/usr/bin/env python3
"""
End-to-end tests for the -b option and the non-fatal backup-creation
fix (ME2.13).

Drives `me` through a pty (see pty_drive.py) for three scenarios:

  1. default: saving an existing file creates a ",,filename" backup
     holding the pre-edit content.
  2. -b: saving with -b creates no backup file.
  3. unwritable directory: if the ",,filename" backup can't be created
     (e.g. the containing directory isn't writable), the save still
     succeeds and a warning is shown -- it no longer blocks the write.

Usage: python3 test_backup.py
"""
import os, sys, shutil, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pty_drive as pd

ORIG = "line one\nline two\n"
EDIT_PREFIX = "iEDIT-"  # typed literally as self-insert characters
KEYS = [EDIT_PREFIX, b'\x18\x13', b'\x18\x03']  # type prefix, ^X^S, ^X^C

failures = []


def check(name, cond):
    print(f"[{'PASS' if cond else 'FAIL'}] {name}")
    if not cond:
        failures.append(name)


def read(path):
    with open(path) as fh:
        return fh.read()


tmp = tempfile.mkdtemp(prefix="me_backup_test_")
try:
    # --- Scenario 1: default backup creation ---
    f1 = os.path.join(tmp, "default.txt")
    with open(f1, "w") as fh:
        fh.write(ORIG)
    pd.drive(f1, os.path.join(tmp, "default.log"), KEYS)

    bak1 = os.path.join(tmp, ",,default.txt")
    check("default: save writes edited content", read(f1) == EDIT_PREFIX + ORIG)
    check("default: ,,backup created with pre-edit content",
          os.path.exists(bak1) and read(bak1) == ORIG)

    # --- Scenario 2: -b suppresses backup ---
    f2 = os.path.join(tmp, "nobackup.txt")
    with open(f2, "w") as fh:
        fh.write(ORIG)
    pd.drive(f2, os.path.join(tmp, "nobackup.log"), KEYS, extra_args=("-b",))

    bak2 = os.path.join(tmp, ",,nobackup.txt")
    check("-b: save writes edited content", read(f2) == EDIT_PREFIX + ORIG)
    check("-b: no backup file created", not os.path.exists(bak2))

    # --- Scenario 3: unwritable directory -- backup failure is non-fatal ---
    d3 = os.path.join(tmp, "nowrite")
    os.mkdir(d3)
    f3 = os.path.join(d3, "locked.txt")
    with open(f3, "w") as fh:
        fh.write(ORIG)
    log3 = os.path.join(tmp, "locked.log")  # outside d3 -- d3 won't be writable

    os.chmod(d3, 0o555)
    try:
        pd.drive(f3, log3, KEYS)
    finally:
        os.chmod(d3, 0o755)

    bak3 = os.path.join(d3, ",,locked.txt")
    check("unwritable dir: save still succeeds", read(f3) == EDIT_PREFIX + ORIG)
    check("unwritable dir: no backup file created", not os.path.exists(bak3))
    with open(log3, "rb") as fh:
        check("unwritable dir: backup-failure warning shown",
              b"couldn't create backup file" in fh.read())

finally:
    shutil.rmtree(tmp, ignore_errors=True)

if failures:
    print(f"\n{len(failures)} check(s) failed: {', '.join(failures)}")
    sys.exit(1)

print("\nAll checks passed.")
