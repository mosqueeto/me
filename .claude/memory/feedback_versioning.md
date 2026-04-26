---
name: Version number conventions
description: Always bump VERSION_NAME in main.c when committing; use "X" suffix for informal changes
type: feedback
originSessionId: 3ec58e3d-bc4f-463d-83bf-641f34e60041
---
Always update `VERSION_NAME` in `main.c` as part of any commit. Two cases:

- **Formal version bump**: increment the number (e.g. `ME2.08` → `ME2.09`) when committing a named release.
- **Informal change**: append "X" suffix (e.g. `ME2.09` → `ME2.09X`) when making changes that don't warrant a new version number.

**Why:** Kent noticed ME2.09 shipped still showing 2.08 — the bump was forgotten. Version string is user-visible on the mode line.

**How to apply:** Version bump is part of the commit checklist — do it before committing, not as a follow-up.
