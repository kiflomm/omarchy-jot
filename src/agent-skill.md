---
name: jot
description: Manage local Jot notes and to-do tasks from the command line without a display.
---

Use `jot open` to launch the desktop window. All other commands run without a display.
Every command accepts `--json`, reports errors on stderr, and exits 0 on success or 1 on failure.

```sh
jot notes list --json
jot notes add --title 'Ideas' --body 'First thought' --json
jot notes update --id NOTE_ID --body 'Updated thought' --json
jot notes delete --id NOTE_ID --json
jot tasks list --json
jot tasks add --text 'Review the draft' --json
jot tasks update --id TASK_ID --text 'Review the final draft' --json
jot tasks complete --id TASK_ID --json
jot tasks reopen --id TASK_ID --json
jot tasks delete --id TASK_ID --json
```

Read the list first to obtain IDs. Preserve note fields the user did not ask to change;
`notes update` preserves any field whose flag is omitted. Quote arguments safely.
Ask before deleting content unless the user has already requested deletion.
Never edit the JSON data file directly. Jot serializes mutations under a lock and saves atomically.
The GUI notices CLI changes within 1.5 seconds. If a note has a conflicting unsaved edit,
it remains in the GUI and reports the conflict rather than overwriting either version.
The default data location is `$XDG_DATA_HOME/jot/jot.json` or `~/.local/share/jot/jot.json`.
Use `--data-dir DIRECTORY` for isolated testing. Undo history belongs to the current GUI session.
