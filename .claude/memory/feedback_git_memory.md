---
name: Memory files belong in git
description: .claude/memory/ files are tracked in git; always stage new ones when committing
type: feedback
originSessionId: 3ec58e3d-bc4f-463d-83bf-641f34e60041
---
The `.claude/memory/` files are tracked in the ME repo's git history. Always `git add` any new memory files created during a session when making commits.

**Why:** Memory files travel with the repo (useful on other machines) and document conventions/decisions. If the repo goes public later, they can be removed with `git rm -r --cached .claude/` at that point.

**How to apply:** When staging files for a commit, include any new or modified files under `.claude/memory/`. Don't let them accumulate as untracked files.
