---
name: ME codebase conventions
description: Key non-obvious conventions in the ME codebase that affect correctness
type: feedback
originSessionId: 7783afe9-34e1-4314-b733-f8d89232a78d
---
**mlwrite uses %D (capital) for long integers, not standard %ld.**
Using %ld prints "ld" literally — the 'l' modifier is not recognized. Use %d for int, %D for long.

**Why:** mlwrite is a hand-rolled formatter in display.c, not printf. Discovered when "[pipe: ld bytes]" appeared instead of a byte count.

**How to apply:** Any time writing an mlwrite format string with a long value, use %D.

---

**bfind() with BFTEMP flag makes a buffer invisible and inaccessible.**
Buffers created via bfind(..., TRUE, BFTEMP) don't appear in the buffer list and are rejected with "Cannot select builtin buffer" on subsequent bfind calls. User-facing destination buffers (e.g. @buffername in pipe commands) must use flag=0.

**Why:** The pipe @buffername feature was broken because dest buffers were created as BFTEMP.

**How to apply:** Only use BFTEMP for internal editor buffers like [List].
