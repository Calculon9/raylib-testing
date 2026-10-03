# Verification note: Unified Symmetric UI Data Binding

## What was built

The symmetric binding core (source x sink x format/parse) was added to the existing
`binding.h` / `binding.c` pair, and the legacy textbox-only mechanisms were re-expressed as
thin wrappers over it. Additive migration: no caller was forced to change.

### Files changed
- `include/ui/binding.h` — added `#include "math/cvectors.h"`; added canonical enum
  `BindingValueType { BIND_NONE..BIND_STRING }` with back-compat aliases
  (`typedef BindingValueType BindingType;` + `#define BINDING_* BIND_*`, identical values 0..4);
  added `BindingValue`, `BindingSource`, `BindingSink`, `Binding`, `BindingQueryFn`,
  `BindingCommandFn`, and prototypes for `Binding_FormatValue/ParseText/ReadSource/WriteSink/
  Commit/RefreshText/Destroy`. No debug/command subsystem includes.
- `src/engine/ui/binding.c` — implemented the six `Binding_*` core operations + `Binding_Destroy`;
  re-expressed `Binder_ValidateAndWrite` as an adapter that builds a transient address sink and
  routes through `Binding_ParseText` + `Binding_WriteSink`. `ParseVector2d` and the three
  validators are unchanged. `Binding_FormatValue` dispatches to the leaf `Pipeline*` primitives
  (one home per format string).
- `src/engine/system/integration_system.c` — deleted the commented-out `PipelineTextTo*` stubs
  (logic now lives in `Binding_ParseText`); reimplemented `RefreshTextboxFields`' per-field format
  step over a local `Binding` (BIND_SRC_ADDRESS source, BIND_SINK_NONE), preserving the exact
  control flow and the `if (field->textbox->is_focused) continue;` focused-skip invariant.
  `Pipeline*`, `WriteTextbox*`, `ResolveBindingType`, `BindTextbox*`, `Clear*` are unchanged.
- `include/ui/ui.h` — added `Binding *binding;` as the last member of `ButtonData`
  (no `sizeof(UIElement)` growth: `HoverItemData` remains the largest union arm).
- `src/engine/ui/ui.c` — added a dispose hook in `DisposeUIElement`:
  `if (IsBtn(e) && e->data.button.binding) Binding_Destroy(&e->data.button.binding);`
  mirroring the existing textbox `Binder_Destroy` block.

### Expressibility (item 7, no file change)
The toggle-button query-source x command-sink pair is expressible via the shipped types:
`source = { BIND_SRC_QUERY, value_type=BIND_INT, query, query_key }` and
`sink = { BIND_SINK_COMMAND, command, command_code }`. No button is wired this pass (trigger is
out of scope); no scratch compile unit was added or left behind.

## How it was verified

Run from `c:\Projects\raylib-testing\.worktrees\ui-binding` in PowerShell (presets only,
UCRT64 + Ninja):

```
cmake --preset debug
cmake --build --preset debug; "EXIT=$LASTEXITCODE"
```

Result: configure succeeded; the build linked `raylib-game.exe`; final command reported
`EXIT=0`. The leading `ninja: warning: premature end of file; recovering` line (when present)
is benign. The produced exe was NOT run.

All six mandated caller files compiled unchanged and link into the executable:
`ui_constructors.c`, `ui_input.c`, `ui_system.c`, `lpanel_system.c`, `rpanel_system.c`,
`state_manager_system.c`.

## Preserved behaviour / carried hazards
- Focused-textbox skip preserved verbatim in the read path.
- STRING sink keeps the hardcoded 255-cap copy with `dst[255]='\0'` (pre-existing latent
  over-write for sub-256 targets preserved to honour "behave identically"; flagged as the first
  follow-on hardening candidate).
- `CMakePresets.json` was copied into the worktree root as local tooling and is deliberately
  NOT committed (matches how it is untracked in the parent checkout).
