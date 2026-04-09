---
name: Local wrap reflow system
description: New wrap_insert/wrap_delete/wrap_split replace fillpara for auto-wrap in MDWRAP mode
type: project
originSessionId: d01f9da6-658c-4d5e-974f-465ae64b602c
---
Replaced fillpara-based auto-wrap with a local reflow system that guarantees the cursor never moves unexpectedly.

**Why:** fillpara destroys and reconstructs the whole paragraph, causing distracting cursor jumps — especially when typing spaces mid-line collapsed runs of spaces, or when deletion reflowed text the user didn't touch.

**How to apply:** The design requirement is firm: only explicit keystrokes move the cursor. Auto-wrap on insert and delete must be cursor-stable.

Key functions added to word.c:
- `wrap_split(lp, brk)` — splits lp at position brk (a space): trims lp to [0..brk-1], sets L_SNL, and either prepends the tail to the existing continuation (if lp had L_SNL) or creates a new continuation line. Saves/restores cursor.
- `wrap_insert()` — called from main.c on space-triggered overflow; finds rightmost space at or after cursor within rmarg, calls wrap_split, cascades into continuation lines.
- `wrap_delete()` — called from random.c after forwdel/backdel; walks continuation chain pulling words up as space permits, cascades through all following lines.

fillpara (M-q) and fillbuf (M-Q) are unchanged and still useful for explicit full-paragraph reflow.

Also fixed in this session:
- `ldelnewline` in line.c: merged line now inherits L_SNL from lp2 (the second line) instead of losing flags. Blank-second-line case also fixed.
- `fillbuf` in word.c: was saving a raw line pointer (save_dotp) that became dangling after fillpara freed lines. Now counts non-ws chars from BOB and restores by walking forward — same technique fillpara uses within a paragraph.
- Removed two-spaces-after-sentence-termination from fillpara (dotflag logic).
- Removed double-space guard from wrap_reflow_after_del (no longer needed).
