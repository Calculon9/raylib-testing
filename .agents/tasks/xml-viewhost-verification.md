# Verification: XML-authoritative ViewHost + View selection (left panel)

Iteration: FIRST (no `.agents/tasks/xml-viewhost-review.json` present at start).
Toolchain: MSYS2 UCRT64 (gcc) + Ninja, via CMake presets only. No npm/yarn/make/MSVC.
Git: NOT mutated. No commit/stage/branch/rebase/reset/worktree. All edits left uncommitted on `ui-overhaul`.

## Commands run

### Configure
```
cmake --preset debug
```
Result: `Configuring done` / `Generating done`, **Exit Code: 0**.

### Build (authoritative exit code)
```
cmake --build --preset debug; "EXIT=$LASTEXITCODE"
```
Result: **EXIT=0**.

- `[110/111] Linking C executable raylib-game\raylib-game.exe` succeeded.
- All project translation units compiled, including the changed files:
  - `src/engine/system/ui/ui_loader.c`
  - `src/engine/system/ui/lpanel_system.c`
  - `src/engine/system/ui/view_host_system.c` (unchanged, recompiled)
- The `ninja: warning: premature end of file; recovering` prefix appeared and is
  NOT an error (per the task note).
- No EXE link-lock encountered: the final link to the real
  `build/Debug/raylib-game/raylib-game.exe` output completed with exit 0, so no
  `Permission denied` fallback / linkcheck procedure was needed.

## Intermediate failures fixed during this pass

1. `implicit declaration of 'UILoader_LogWarning'` — the new per-load side-table
   helpers (which use the `LOADER_WARNING` macro) were defined above the logging
   function definitions. Fixed by adding forward declarations for
   `UILoader_LogWarning` / `UILoader_LogError` / `UILoader_Log` immediately after
   the logging macros (before the side table). Rebuild then reached EXIT=0.

## Behavioural confirmations (code inspection — no unit-test harness exists)

- View type propagation: `BuildView` resolves `type=` via the app resolver and
  records `(container, ViewType, id)`; `UILoader_CollectViewContainers` now stamps
  `view->type` from that side table (replacing the hard-coded `LPANEL_STATE_VIEW`),
  so `draw_view` carries `LPANEL_DRAW_VIEW` and `state_view` carries
  `LPANEL_STATE_VIEW`.
- Selector wiring: `UILoader_BuildSelectorFromMarkup` resolves each Option's
  `view=` id to the matching view index (direct `UIElement*` container-pointer
  comparison against `host->views`), sets `view_indices[i]` to the RESOLVED index,
  and wires `on_click = HandleViewHostSelectorClick`,
  `user_data = &selector->view_indices[i]`, `data_bind = selector` — identical to
  `ViewHostSystem_CreateViewSelector`.
- DRAW selection sets `active_view`: selecting DRAW calls
  `ViewHostSystem_SelectView` -> `on_view_selected` =
  `ViewHostSystem_HandleViewSelected`, which sets
  `G_UIState.active_view = view->type` (= `LPANEL_DRAW_VIEW`). `geometry_editor.c`
  and `ui_system.c` compare against `LPANEL_DRAW_VIEW` and are unaffected by the
  rewiring.
- STATE preserved: `LPanel_AttachToggleSources(root)` still runs on the adopted
  XML tree; STATE action buttons keep their command-sink bindings and ON/OFF query
  sources; the `size-mode="content_fill"` Sections are untouched by the loader
  changes.
- Single root / coordinate space: `InitLPanel` now calls `ViewHostSystem_InitRoot`
  (builds the real `UI_ELEMENT_ROOT` + space + seed_box) then
  `AddElementToTree(root, lpanel->root)` to re-parent the XML `<ViewHost>`
  container under it. `lpanel->root = root` (the dangling-root bug) is NOT used.
- initialView: `UILoader_GetInitialViewId()` returns `"state_view"`, resolved to
  index 0 via `UILoader_ResolveViewIndexById`; `ViewHostSystem_SelectView` is the
  single styling + selection call. `ViewHostSystem_FinaliseInit` is NOT called.

## Out of scope (confirmed not done, by design)

- DRAW `<TextField>` controls render and the view is selectable, but their
  `G_UIState.edit_*_tbox` pointers remain NULL and are not bound to
  entity-create data. NULL-safe: `RefreshTextboxFields` skips NULL textbox rows.
- No `id -> G_UIState` registration mechanism was added.

## Files changed (all uncommitted)

- `include/system/ui/ui_loader.h`
- `src/engine/system/ui/ui_loader.c`
- `src/engine/system/ui/lpanel_system.c`
- `src/engine/ui/components/lpanel.xml` — no change (draw_view already live; confirmed).
