#!/usr/bin/env python3
"""
Drive `me` headlessly through a pty for interactive testing.

`me` is a curses-style editor: it can't be tested by piping stdin/stdout,
and a naive pty (e.g. plain pty.fork()) leaves the terminal window size at
0x0, which makes vtinit() (display.c) malloc() a bogus-sized vscreen and
abort().  This sets TIOCSWINSZ and TERM before exec so the editor starts
normally, then feeds it a sequence of keystrokes and records everything it
writes back.

Usage:
    python3 pty_drive.py <target-file> <log-file> <keystroke> [<keystroke> ...]

Each <keystroke> is sent as raw bytes (use $'...' in bash for control chars,
e.g. $'\\x18\\x09' for C-X C-I, $'\\x1bW' for M-W).  Inspect <log-file> with
`cat -v` / `sed` to strip escape sequences and see what the editor displayed,
and check <target-file> for the saved result.

Example -- verify that inserting a small file mid-line splices it in place
rather than adding a separate line (C-X C-I, ME2.12 "as if typed" insert):

    printf 'abcXdef' > /tmp/t.txt
    printf 'INSERTED' > /tmp/snip.txt
    python3 pty_drive.py /tmp/t.txt /tmp/log.raw \\
        $'\\x06\\x06\\x06' $'\\x18\\x09' /tmp/snip.txt $'\\x0d' $'\\x18\\x13' $'\\x18\\x03'
    cat /tmp/t.txt   # -> abcINSERTEDXdef
"""
import os, sys, time, select, struct, fcntl, termios, signal

ME = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "me")


def drive(target, logpath, cmds, rows=30, cols=100, settle=0.3, exe=ME, extra_args=()):
    """Launch `exe` on `target` in a pty, send `cmds` (byte-strings, sent one
    at a time with a short pause for the screen to settle), record all output
    to `logpath`, and wait for the editor to exit (or kill it after a timeout).

    `extra_args` are inserted before `target` on the command line, e.g.
    extra_args=("-b",) to test the no-backup option.
    """
    import pty
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))

    pid = os.fork()
    if pid == 0:
        os.close(master)
        os.setsid()
        fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
        os.dup2(slave, 0)
        os.dup2(slave, 1)
        os.dup2(slave, 2)
        if slave > 2:
            os.close(slave)
        env = dict(os.environ)
        env["TERM"] = "xterm"
        env["COLUMNS"] = str(cols)
        env["LINES"] = str(rows)
        os.execvpe(exe, [exe, *extra_args, target], env)
        os._exit(1)

    os.close(slave)
    log = open(logpath, "wb")

    def drain(timeout):
        end = time.time() + timeout
        got = False
        while True:
            r, _, _ = select.select([master], [], [], max(0, end - time.time()))
            if not r:
                break
            try:
                d = os.read(master, 65536)
            except OSError:
                return got
            if not d:
                return got
            log.write(d)
            got = True
            end = time.time() + timeout
        return got

    try:
        time.sleep(0.6)
        drain(settle * 2)
        for c in cmds:
            data = c.encode() if isinstance(c, str) else c
            os.write(master, data)
            time.sleep(settle * 0.8)
            drain(settle)

        for _ in range(20):
            try:
                r, status = os.waitpid(pid, os.WNOHANG)
            except ChildProcessError:
                r, status = pid, 0
            if r == pid:
                break
            drain(0.2)
            time.sleep(0.1)
        else:
            os.kill(pid, signal.SIGTERM)
            os.waitpid(pid, 0)

        drain(0.3)
    finally:
        log.close()
        os.close(master)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    drive(sys.argv[1], sys.argv[2], sys.argv[3:])
