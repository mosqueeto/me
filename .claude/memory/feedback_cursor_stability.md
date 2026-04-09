---
name: Cursor stability in wrap mode
description: Cursor must never move except under explicit user control — no auto-reflow that repositions cursor
type: feedback
originSessionId: d01f9da6-658c-4d5e-974f-465ae64b602c
---
Auto-wrap operations (on insert and delete) must never move the cursor to a different position than where the user placed it.

**Why:** Unexpected cursor jumps are very distracting. The user observed that other editors (e.g. browser input areas) only move text to the RIGHT of the cursor when wrapping — text before the cursor is never disturbed, and the cursor itself stays put.

**How to apply:** Any wrap/reflow triggered automatically (not by M-q/M-Q) must be cursor-anchored. wrap_insert() and wrap_delete() enforce this. fillpara must NOT be called from auto-wrap paths. Only call fillpara from explicit user commands (M-q, M-Q).
