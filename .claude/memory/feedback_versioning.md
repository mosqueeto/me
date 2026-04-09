---
name: Version number X suffix convention
description: When making changes without a formal version bump, append "X" to the current version string
type: feedback
---

When making changes that don't warrant a new version number, append "X" to `VERSION_NAME` in `main.c` (e.g. `"ME2.02"` → `"ME2.02X"`).

**Why:** Distinguishes modified builds from official releases without a full version increment.

**How to apply:** Any time code is changed and the version number is not being formally bumped, update `VERSION_NAME` to add the "X" suffix if not already present.
