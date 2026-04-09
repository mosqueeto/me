# Memory Index

## Feedback
- [Version number X suffix convention](feedback_versioning.md) — append "X" to VERSION_NAME in main.c when changing code without a formal version bump
- [Cursor stability in wrap mode](feedback_cursor_stability.md) — auto-wrap must never move cursor; only explicit M-q/M-Q may reflow

## Project
- [Lua embedding and pipe/config discussion](project_lua_and_pipe_discussion.md) — previous session: Lua=overkill, config file alternative, gpg encryption, buffer piping mechanism
- [Local wrap reflow system](project_wrap_reflow.md) — wrap_insert/wrap_delete/wrap_split replace fillpara for auto-wrap; ldelnewline and fillbuf fixes
