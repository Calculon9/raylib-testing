# Diagnostics extraction out of debug_overlay_system

The change lifts the developer debug **dashboard**, the viewport-space **basis editor**, and the shared **basis-target machinery** out of `src/engine/system/debug_overlay_system.c` into a new `diagnostics` module (`include/system/diagnostics.h` + `src/engine/system/diagnostics.c`). The facade keeps the toggle surface (`ToggleDebug`/`IsDebugEnabled`), the F7–F10 shipping viewport-scale hotkeys, the viewport-refresh orchestration, and the three public entry points — now delegating the dashboard/basis-editor work into the module through a six-function API. This is a pure code-move: no behavioural change, no `#ifdef`/compile-out guards, header signatures byte-identical. Dependency direction is facade ← diagnostics for the dashboard's ON/OFF rows (via `IsDebugEnabled`) and facade → diagnostics for the toggle/draw/snapshot/basis entry points, matching the existing object-gizmos precedent.

Watch for: nothing blocking. The working tree also carries unrelated textbox-binding changes (systems.h, binding.*, ui.*, lpanel/rpanel/state_manager, etc.) that are **out of scope** for this review — the user committed/landed that work separately (confirmed). This review covers only the diagnostics-extraction files. (confirmed)

**Verdict**: APPROVED

## High-level view

The move is faithful. Every Group-A symbol named in the plan (the `DebugBasisTargetId` enum, `DebugBasisTargetOps` table and its frame-getter/apply/reset statics, the accessors, the `UniverseDebugSnapshot` struct and `debug_snapshot` global, the four diagnostic statics, all dashboard helpers, `DrawDebugDashboard`, and `HandleBasisVectorMutation`) now lives `static`/private in `diagnostics.c`, and none of them linger in the facade. A grep for the moved symbols in the facade returns only the one expected delegating call. (confirmed)

The facade's `ToggleDebug`/`IsDebugEnabled` still switch over every `DebugOverlayId` case; `DEBUG_DASHBOARD` delegates to `Diagnostics_ToggleDashboard`/`Diagnostics_IsDashboardEnabled` exactly like the gizmo cases delegate to `object_gizmos`. All other cases are untouched. (confirmed)

The three public entry points keep their byte-identical signatures. `DrawGlobalDebugOverlays` still draws the viewport grid then the dashboard behind the enabled guard; `DrawUniverseDebugOverlays` is now a one-line delegate to `Diagnostics_UpdateSnapshot`; `UpdateDebugOverlayHotkeys` preserves the call order and routes the basis-editor call plus the universe-space branch through the module predicate. The post-edit refresh branch now tests `Diagnostics_ActiveBasisTargetIsUniverseSpace()` instead of the (now-private) enum, which is an exact behavioural substitute. (confirmed)

Hotkey behaviour is preserved: F1/Ctrl+F1, F2, F3, F4, F6, F11, F12/Shift+F12 stay in `HandleDebugToggleHotkeys`; F7–F10 stay in `HandleViewportScaleHotkeys`; F5 + TAB/U/V/I/J/K/L/O/P/BACKSPACE moved intact into `Diagnostics_HandleBasisEditorHotkeys` with identical print strings and the same `basis_changed` return path. (confirmed)

The public header `include/system/debug_overlay_system.h` and both callers (`src/raylib_game.c`, `src/engine/world/universe_renderer.c`) are unchanged in the working tree. Git is untouched — all edits are unstaged, nothing committed, branch up to date with origin. Build evidence records a clean reconfigure + build with EXIT=0 and `raylib-game.exe` linked, no compiler warnings. (confirmed)

<details>
<summary>Issues (0)</summary>

No blocking or non-blocking findings. Every acceptance criterion is satisfied.

</details>

<details>
<summary>Details</summary>

### Facade still owns all DebugOverlayId cases; dashboard delegates

`ToggleDebug` and `IsDebugEnabled` retain the full switch over `DEBUG_DASHBOARD`, `DEBUG_VIEWPORT_GRID`, `DEBUG_WORLD_GRID`, `DEBUG_WORLD_GRID_LABELS`, `DEBUG_UNIVERSE_GRID_LABELS`, `DEBUG_UI_BORDERS`, and `DEBUG_OBJECT_AXES/HULL/AABB`. `DEBUG_DASHBOARD` calls `Diagnostics_ToggleDashboard()` / `return Diagnostics_IsDashboardEnabled();`; the `dashboard_overlay_enabled` global that backed it has moved into the module and is no longer referenced from the facade. All other cases are byte-identical to before. (confirmed) (criterion 1)

### Entry-point wiring and the universe-space branch

`UpdateDebugOverlayHotkeys` keeps the NULL guard on its out-params and the ordering `HandleDebugToggleHotkeys()` → `Diagnostics_HandleBasisEditorHotkeys()` → `HandleViewportScaleHotkeys(...)` → `ApplyDebugHotkeyChanges(...)`. Inside `ApplyDebugHotkeyChanges`, the `basis_changed` branch tests `Diagnostics_ActiveBasisTargetIsUniverseSpace()`, whose implementation is `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;` — a direct replacement for the old inline enum test, so universe-space edits still refresh the camera via `UpdateCameraFull` and every other target still calls `RefreshViewportForBasisEdit`. The predicate returns a 0/1 int rather than the raw id, which correctly avoids the else-branch misfire that returning the id would cause. (confirmed) (criteria 2, 5)

### Basis editor moved intact

`Diagnostics_HandleBasisEditorHotkeys` carries the F5 toggle, TAB target-cycle, U/V vector select, BACKSPACE reset, and I/J/K/L/O/P mutation via `HandleBasisVectorMutation`, with the apply-failure rollback (`basis_frame->basis = previous_basis;`) and all `[Viewport Basis]` / `[Basis Editor]` print strings reproduced verbatim. It returns `basis_changed` exactly as the original did. (confirmed) (criterion 2)

### Snapshot early-out preserved

`Diagnostics_UpdateSnapshot` keeps `debug_snapshot.valid = false;` followed by the `if (!dashboard_overlay_enabled) return;` early-out before populating, so the frame-skip behaviour when the dashboard is off is identical. The `target_world_index` bounds check against `G_Universe.world_count` is retained. (confirmed) (criterion 2)

### Module boundary and includes

`diagnostics.c` includes exactly the font/text/panel/world/viewport headers the moved code needs and makes `<stdarg.h>` explicit for `DrawDashboardRowf`; it does not pull `camera/camera.h` (it reaches `G_Universe.camera.*` through `world/universe.h`). The facade drops `ui/cfont.h`, `ui/text_region.h`, the panel-system headers, `<math.h>`, and `<string.h>` that only the moved code used, adds `system/diagnostics.h`, and keeps `camera/camera.h` for `UpdateCameraFull`. The build evidence reports no unused-include / unused-function / implicit-declaration warnings, confirming the prune on both sides left no dangling or duplicate statics. (confirmed) (criteria 5, 6)

### Header, callers, and git

`include/system/debug_overlay_system.h` is unchanged (`DebugOverlayId`, the `ui_borders_enabled` extern, and the five function declarations are intact), and the three facade definitions match those signatures byte-for-byte. `git diff --name-only` lists neither the header nor `raylib_game.c` nor `universe_renderer.c`, so the callers compile without edits. `git status` shows every change unstaged with the branch up to date with `origin/ui-overhaul` — no commit, stage, branch, or worktree change. The new module files are untracked, as expected for an uncommitted working-tree change. (confirmed) (criteria 3, 7)

### Build evidence

`.agents/tasks/diagnostics-extraction-verification.md` records `cmake --preset debug` (EXIT 0) picking up the new `.c` via `CONFIGURE_DEPENDS`, then `cmake --build --preset debug` with `EXIT=0`, both `debug_overlay_system.c.obj` and `diagnostics.c.obj` compiled, and `raylib-game.exe` linked at step `[110/111]`. No exe-lock was hit. The `ninja: warning: premature end of file; recovering` line is the known benign log-recovery notice. Per the task instruction I did not re-run the build; the evidence satisfies criterion 8 (EXIT=0, executable linked). (confirmed) (criterion 8)

</details>

<details>
<summary>File map</summary>

- `include/system/diagnostics.h` — new module header: six-function API + banner comment.
- `src/engine/system/diagnostics.c` — new module: all moved basis-target machinery, dashboard, snapshot, and basis-editor code, plus the six public wrappers.
- `src/engine/system/debug_overlay_system.c` — reduced to facade + F7–F10 viewport hotkeys + viewport-refresh orchestration + three delegating entry points; `#include "system/diagnostics.h"` added, diagnostics-only includes pruned.
- `include/system/debug_overlay_system.h` — unchanged.
- Out of scope (unrelated textbox-binding work in the same working tree): `include/system/systems.h`, `include/system/command_system.h`, `include/ui/binding.h`, `include/ui/ui.h`, `include/world/universe.h`, `include/world/world.h`, `src/engine/system/command_system.c`, `src/engine/system/integration_system.c`, `src/engine/system/ui/*.c`, `src/engine/system/world_system.c`, `src/engine/ui/binding.c`, `src/engine/ui/ui.c`, `src/engine/ui/ui_input.c`, `src/engine/world/world_renderer.c`, and the new `object_gizmos.*`.

Full diff: `git diff -- src/engine/system/debug_overlay_system.c` and `git status` for the new untracked `diagnostics.*` files.

</details>
