# Memory Index

## Feedback
- [Version number conventions](feedback_versioning.md) — always bump VERSION_NAME in main.c before committing; "X" suffix for informal changes
- [Cursor stability in wrap mode](feedback_cursor_stability.md) — auto-wrap must never move cursor; only explicit M-q/M-Q may reflow
- [ME codebase conventions](feedback_code_conventions.md) — mlwrite uses %D not %ld for long; BFTEMP makes buffers invisible/inaccessible
- [Memory files in git](feedback_git_memory.md) — .claude/memory/ is tracked; always git add new memory files when committing

## Project
- [Current state: ME2.08](project_current_state.md) — what was added in ME2.08, pending todo items (man page, local .me dir, git mode)
- [Lua embedding and pipe/config discussion](project_lua_and_pipe_discussion.md) — previous session: Lua=overkill, config file alternative, gpg encryption, buffer piping mechanism
- [Local wrap reflow system](project_wrap_reflow.md) — wrap_insert/wrap_delete/wrap_split replace fillpara for auto-wrap; ldelnewline and fillbuf fixes
