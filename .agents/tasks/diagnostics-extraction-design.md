# Design: Extract developer diagnostics out of `debug_overlay_system`

## Overview

This is the final step (Option B) of the debug-overlay separation effort. The goal is a
pure code-move / ownership refactor that lifts the two genuine developer diagnostics — the
full-screen debug **dashboard** and the viewport-space **basis editor** — out of
`src/engine/system/debug_overlay_system.c` into a dedicated module, so that
`debug_overlay_system` is left as nothing but the toggle **facade** plus the shipping
viewport-scale hotkeys (F7–F10).

The dashboard and the basis editor share the "basis-target" machinery (the
`DebugBasisTargetOps` table and its frame-getter / apply / reset statics), so that machinery
moves with them and lives privately inside the new module, usable by both halves.

This refactor **preserves all behaviour exactly**: same hotkeys, same dashboard layout, same
basis editor, same toggles, same console prints. There are **no compile-out guards and no
`#ifdef`** — a clean module split only. **Git is left untouched**: nothing is staged,
committed, or rebased; all changes remain uncommitted for the user to commit manually.

The split mirrors two precedents already landed on `ui-overhaul`: the object-gizmos extraction
(`include/world/object_gizmos.h` + `src/engine/world/object_gizmos.c`, with the facade's
`DEBUG_OBJECT_*` cases delegating to `ToggleGizmo`/`IsGizmoEnabled`) and the world-system
re-home of the view flags. The direction of dependency is the same as the gizmo precedent:
the diagnostics module is a **consumer** of the facade (`IsDebugEnabled`), never the reverse.

## Technology stack (locked)

C11, raylib, CMake with `FetchContent`. No new dependencies, no new libraries. The new `.c`
file auto-registers because `src/CMakeLists.txt` uses
`file(GLOB_RECURSE ... CONFIGURE_DEPENDS *.c)` — **no CMake edit is required**, only a
reconfigure, which `CONFIGURE_DEPENDS` triggers automatically.

## New module

- **Header:** `include/system/diagnostics.h`
- **Source:** `src/engine/system/diagnostics.c`

Name and location chosen to match the existing engine-system layout
(`include/system/*.h` ↔ `src/engine/system/*.c`, e.g. `debug_overlay_system`,
`viewport_system`). `diagnostics` reflects that this module owns the genuine developer
diagnostics, as distinct from the shipping overlays that remain owned elsewhere. (The
object-gizmos precedent put world-layer gizmos under `world/`; these diagnostics belong to the
engine-system layer, hence `system/`.)

## Public API of `diagnostics.h`

The header exposes only the six entry points (five wrappers plus one predicate) the facade
needs to call. Everything else
(the basis-target machinery, the snapshot struct, every dashboard-draw helper, the basis-editor
state and its hotkey helpers) stays `static` inside `diagnostics.c`.

```c
/**********************************************************************************************
*
*   DIAGNOSTICS MODULE
*
*   Genuine developer diagnostics extracted from the debug-overlay facade: the full-screen
*   debug dashboard and the viewport-space basis editor (plus the shared basis-target
*   machinery they both use). State lives here; debug_overlay_system remains a thin toggle
*   facade and delegates its DEBUG_DASHBOARD case and its dashboard/basis-editor entry points
*   to this module, so existing command/UI/hotkey entry points keep working unchanged.
*
*   This module is a CONSUMER of the debug-overlay facade: it calls IsDebugEnabled() to render
*   the dashboard's ON/OFF rows. It never calls back into facade toggling.
*
**********************************************************************************************/
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>

// --- Dashboard toggle/query (DEBUG_DASHBOARD delegates here) ---

// Flip the debug dashboard's enabled state.
void Diagnostics_ToggleDashboard(void);

// Return true when the debug dashboard is enabled, false otherwise.
bool Diagnostics_IsDashboardEnabled(void);

// --- Dashboard rendering + snapshot ---

// Draw the full-screen debug dashboard. No-op responsibility stays with the caller: the
// facade only calls this when the dashboard is enabled.
void Diagnostics_DrawDashboard(void);

// Recompute the universe diagnostics snapshot for the current frame. When the dashboard is
// disabled the snapshot is simply invalidated (matches the previous early-out behaviour).
void Diagnostics_UpdateSnapshot(void);

// --- Basis editor ---

// Process basis-editor hotkeys (F5 toggle, TAB/U/V/I/J/K/L/O/P/BACKSPACE). Returns true when
// the active basis target changed this frame, so the caller can refresh dependent systems.
bool Diagnostics_HandleBasisEditorHotkeys(void);

// Return non-zero when the active basis-editor target is universe space, zero otherwise.
// The facade uses this to route the post-edit refresh: universe-space edits refresh the camera
// (UpdateCameraFull), any other target refreshes the viewport + UI (RefreshViewportForBasisEdit).
// Exposed as an int (a boolean 0/1 predicate) so DebugBasisTargetId stays private to
// diagnostics.c; the implementation is `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;`.
int Diagnostics_ActiveBasisTargetIsUniverseSpace(void);

#endif // !DIAGNOSTICS_H
```

### Why `Diagnostics_ActiveBasisTargetIsUniverseSpace`

`ApplyDebugHotkeyChanges` currently branches on
`basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE` to decide between
`UpdateCameraFull(&G_Universe.camera)` and `RefreshViewportForBasisEdit(...)`. `basis_editor_target`
and `DebugBasisTargetId` move into `diagnostics.c` and become private. Rather than leak the enum
through the header, the module exposes a single boolean predicate (returning a 0/1 `int`) that
answers the exact question the facade asks — "is the active target universe space?" — so the
facade's `if (Diagnostics_ActiveBasisTargetIsUniverseSpace())` branch is a direct, unambiguous
replacement for the current `basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE` test. It
deliberately does **not** return the target id as an int (that would misfire: several target ids
are truthy, so only `DEBUG_BASIS_TARGET_LPANEL_VIEWPORT == 0` would route to the else-branch).
This keeps the enum encapsulated while preserving the identical branch. (See the
`RefreshViewportForBasisEdit` placement decision below for the alternative that was considered and
rejected.)

## What MOVES vs what STAYS

### Group A — DIAGNOSTICS → MOVE to `diagnostics.c`

Basis-target machinery (shared by dashboard + basis editor):

| Symbol | Decision | Reason |
|---|---|---|
| `enum DebugBasisTargetId` | MOVE (private) | Only the diagnostics halves reference it. |
| `struct DebugBasisTargetOps` | MOVE (private) | Internal dispatch table type. |
| `debug_basis_target_ops[]` table | MOVE (private) | The shared target registry. |
| `GetLPanelViewportFrame` / `GetGameViewportFrame` / `GetRPanelViewportFrame` / `GetUniverseSpaceFrame` | MOVE (static) | Frame getters feeding the ops table. |
| `ApplyLPanelViewportBasis` / `ApplyGameViewportBasis` / `ApplyRPanelViewportBasis` / `ApplyUniverseSpaceBasis` / `ApplyLPanelSpaceBasis` / `ApplyRPanelSpaceBasis` | MOVE (static) | Basis-apply callbacks in the ops table. |
| `ResetLPanelViewportBasis` / `ResetGameViewportBasis` / `ResetRPanelViewportBasis` / `ResetUniverseSpaceBasis` | MOVE (static) | Reset callbacks in the ops table. |
| `GetDebugBasisTargetOps` / `GetDebugBasisTargetFrame` / `ApplyDebugBasisTarget` / `ResetDebugBasisTarget` / `GetDebugBasisTargetName` | MOVE (static) | Accessors over the ops table, used by both dashboard and basis editor. |

Dashboard:

| Symbol | Decision | Reason |
|---|---|---|
| `static bool dashboard_overlay_enabled` | MOVE (static) | Dashboard's own state; facade reaches it via `Diagnostics_Toggle/IsDashboard`. |
| `struct UniverseDebugSnapshot` + `static UniverseDebugSnapshot debug_snapshot` | MOVE (private + static) | Dashboard-only data. |
| `GetOnOffLabel` | MOVE (static) | Dashboard label helper. |
| `DrawDashboardLine` / `DrawDashboardRowf` | MOVE (static) | Dashboard text helpers (use `DrawTextCustom`/`FONT_BASIC`). |
| `struct DashboardBasisData` + `ResolveDashboardBasisData` | MOVE (private + static) | Dashboard reads basis frames via `GetDebugBasisTargetFrame`. |
| `DrawDashboardControlsSection` / `DrawDashboardUniverseSection` / `DrawDashboardViewportBasisSection` | MOVE (static) | Dashboard section renderers. |
| `DrawDebugDashboard` | MOVE → wrapped by `Diagnostics_DrawDashboard` | Dashboard composition root. |
| Snapshot-population body currently inside `DrawUniverseDebugOverlays` | MOVE → `Diagnostics_UpdateSnapshot` | Snapshot logic belongs with the dashboard; public entry point stays in facade and calls this. |

Basis editor:

| Symbol | Decision | Reason |
|---|---|---|
| `static bool basis_editor_enabled` | MOVE (static) | Basis-editor state. |
| `static bool basis_editor_editing_u` | MOVE (static) | Basis-editor state. |
| `static DebugBasisTargetId basis_editor_target` | MOVE (static) | Basis-editor state (type is now private here). |
| `HandleBasisVectorMutation` | MOVE (static) | Basis-editor nudge/scale helper. |
| `HandleBasisEditorHotkeys` | MOVE → wrapped by `Diagnostics_HandleBasisEditorHotkeys` | Basis-editor input; returns `basis_changed`. |

Note: `DrawDashboardControlsSection` prints `F5 Basis editor` / `F11 Dashboard` ON/OFF rows from
`basis_editor_enabled` / `dashboard_overlay_enabled`. Because the dashboard and the basis editor
now live in the **same** module, those statics remain directly readable by the dashboard renderer
— no extra accessor is needed for the controls rows.

### Group B — SHIPPING VIEWPORT CONTROLS → STAY in `debug_overlay_system.c`

| Symbol | Decision | Reason |
|---|---|---|
| `RefreshViewportBase` | STAY (static) | Core viewport refresh used by both the scale path and the basis-edit path; the scale path stays, so this stays. |
| `ClampViewportLogicalHeight` | STAY (static) | Used by `HandleViewportScaleHotkeys` (F7/F8), which stays. |
| `RefreshViewportAndDependentSystems` | STAY (static) | Driven by the F7–F10 scale change path. |
| `HandleViewportScaleHotkeys` | STAY (static) | F7–F10 shipping viewport-scale hotkeys. |
| `ApplyDebugHotkeyChanges` | STAY (static) | Orchestrates post-hotkey refresh; now consults the diagnostics module for the basis-change branch (see below). |
| `RefreshViewportForBasisEdit` | **STAY (static)** — see decision below | Its only caller is `ApplyDebugHotkeyChanges`, which stays. |

### Group C — FACADE + ENTRY POINTS → STAY in `debug_overlay_system.c`

| Symbol | Decision | Reason |
|---|---|---|
| `ToggleDebug` / `IsDebugEnabled` | STAY | The toggle facade. `DEBUG_DASHBOARD` case now delegates to the diagnostics module. |
| `HandleDebugToggleHotkeys` | STAY (static) | F1/F2/F3/F4/F6/F11/F12 + Ctrl+F1 ResetUI; F11 routes through `ToggleDebug(DEBUG_DASHBOARD)` → module. |
| `UpdateDebugOverlayHotkeys` | STAY (public, unchanged signature) | Calls `HandleDebugToggleHotkeys`, then `Diagnostics_HandleBasisEditorHotkeys`, then `HandleViewportScaleHotkeys`, then `ApplyDebugHotkeyChanges`. |
| `DrawGlobalDebugOverlays` | STAY (public, unchanged signature) | Calls `DrawViewportDebugGrid`, then `Diagnostics_DrawDashboard` when `Diagnostics_IsDashboardEnabled()`. |
| `DrawUniverseDebugOverlays` | STAY (public, unchanged signature) | Delegates its whole body to `Diagnostics_UpdateSnapshot()`. |

## How `ToggleDebug` / `IsDebugEnabled` DEBUG_DASHBOARD delegates

`dashboard_overlay_enabled` moves into `diagnostics.c`, so the two facade cases delegate exactly
like the gizmo cases already do:

```c
// in ToggleDebug():
case DEBUG_DASHBOARD:
    Diagnostics_ToggleDashboard(); // delegate to the diagnostics owner
    break;

// in IsDebugEnabled():
case DEBUG_DASHBOARD:
    return Diagnostics_IsDashboardEnabled();
```

All other facade cases are unchanged: `DEBUG_VIEWPORT_GRID` → viewport system,
`DEBUG_WORLD_GRID`/`DEBUG_WORLD_GRID_LABELS` → world-owned flags,
`DEBUG_UNIVERSE_GRID_LABELS` → universe-owned flag, `DEBUG_UI_BORDERS` → `ui_borders_enabled`
(a UI-owned extern defined in `ui_system.c` and declared in `debug_overlay_system.h`, toggled
in place by the facade), `DEBUG_OBJECT_*` → `object_gizmos`.

`debug_overlay_system.c` adds `#include "system/diagnostics.h"` so the facade can call the
delegation functions. Dependency direction stays facade → module for the dashboard toggle, and
module → facade for the dashboard's ON/OFF rows (via `IsDebugEnabled`); there is no cycle because
`diagnostics.c` includes `debug_overlay_system.h` for `IsDebugEnabled`/`DebugOverlayId`, while
`debug_overlay_system.c` includes `diagnostics.h` for the six entry points — the two `.c` files
depend on each other's headers, which is fine (the gizmo split does the same).

## How the three public entry points call into the module (identical signatures/behaviour)

1. **`DrawGlobalDebugOverlays(void)`** — unchanged signature. Body stays:
   ```c
   DrawViewportDebugGrid();
   if (Diagnostics_IsDashboardEnabled())
   {
       Diagnostics_DrawDashboard();
   }
   ```
   Behaviour is identical to the current `dashboard_overlay_enabled` guard + `DrawDebugDashboard()`.

2. **`DrawUniverseDebugOverlays(void)`** — unchanged signature. The whole current body (invalidate
   snapshot, early-out when the dashboard is off, otherwise populate `debug_snapshot`) moves into
   `Diagnostics_UpdateSnapshot`. The facade entry point becomes a one-line delegate:
   ```c
   void DrawUniverseDebugOverlays(void)
   {
       Diagnostics_UpdateSnapshot(); // populate the dashboard's universe snapshot for this frame
   }
   ```
   The early-out `if (!dashboard_overlay_enabled) return;` moves inside `Diagnostics_UpdateSnapshot`
   (reading the now-local `dashboard_overlay_enabled`), so the frame-skip behaviour is preserved.

3. **`UpdateDebugOverlayHotkeys(...)`** — unchanged signature. The call order is preserved; only the
   basis-editor call is now the module function, and the basis-change branch now queries the module:
   ```c
   HandleDebugToggleHotkeys();
   basis_changed = Diagnostics_HandleBasisEditorHotkeys();
   HandleViewportScaleHotkeys(..., &viewport_scale_changed, &ui_scale_changed);
   ApplyDebugHotkeyChanges(..., basis_changed, viewport_scale_changed, ui_scale_changed);
   ```
   Inside `ApplyDebugHotkeyChanges`, the universe-space test switches from the (now-private) global
   to the predicate:
   ```c
   if (basis_changed)
   {
       if (Diagnostics_ActiveBasisTargetIsUniverseSpace())
       {
           UpdateCameraFull(&G_Universe.camera);
       }
       else
       {
           RefreshViewportForBasisEdit(screen_width, screen_height, screen_resolution_scalar,
                                       viewport_target_game_logical_height,
                                       viewport_ui_pixels_per_unit_override);
       }
   }
   ```
   This reproduces the current branch exactly.

## `RefreshViewportForBasisEdit` placement decision

**Verified callers (whole-codebase search for `RefreshViewportForBasisEdit`):**
- Definition: `debug_overlay_system.c:205`.
- Sole call site: `debug_overlay_system.c:833`, inside `ApplyDebugHotkeyChanges` (the
  `basis_changed && !universe-space` branch).

There are no other references anywhere in the repository.

**Decision: `RefreshViewportForBasisEdit` STAYS in `debug_overlay_system.c`.**

Reasoning: although the function exists *for* the basis editor, it is invoked only from
`ApplyDebugHotkeyChanges`, which is a Group-B viewport-control orchestrator that must stay (it also
drives the F7–F10 scale path via `RefreshViewportAndDependentSystems`). `RefreshViewportForBasisEdit`
is built on `RefreshViewportBase` + `ResetUI`, both of which belong to the viewport/UI refresh
concern that remains in the facade. Moving it into `diagnostics.c` would require re-exposing
`RefreshViewportBase`, `InitViewportLayout`, `SetViewportTargetLogicalHeight`,
`SetViewportUIScaleScalar`, `SyncUniverseCameraToViewport`, and `ResetUI` plumbing to the module and
threading the `screen_width/height/scalar/logical-height/ppu-override` parameters across the module
boundary — more surface area and coupling for no behavioural gain. Keeping the apply/refresh
orchestration whole in the facade is the simpler, lower-coupling split, and the module only needs to
answer "is the active target universe space?" — which `Diagnostics_ActiveBasisTargetIsUniverseSpace`
provides. This also keeps the F7–F10 and basis-edit refresh paths co-located, as they are today.

## Include partition

**`diagnostics.c` includes** (the subset the diagnostics actually use):
- `system/diagnostics.h` (own header)
- `system/debug_overlay_system.h` — for `IsDebugEnabled` / `DebugOverlayId` (dashboard ON/OFF rows)
- `<math.h>` — `floorf` (snapshot cell index)
- `<string.h>` — `memset` (snapshot)
- `<stdio.h>` — `printf` (basis-editor console prints), `snprintf`/`vsnprintf`
- `<stdarg.h>` — `va_list`/`va_start`/`va_end` in `DrawDashboardRowf` (currently relying on a
  transitive include; make it explicit in the new file)
- `raylib.h` — `GetScreenWidth/Height`, `GetMouseX/Y`, `IsKeyPressed/Down`, `DrawRectangle*`, `Color`
- `camera/camera.h` — `UpdateCameraFull` referenced? (No — that stays in the facade. Camera type is
  reached via `universe.h`'s `G_Universe.camera`.) Include only if the snapshot/basis code needs a
  camera symbol directly; otherwise omit. (Dashboard reads `G_Universe.camera.*` through `universe.h`.)
- `system/universe_system.h` — `Universe_FindWorldAt`, `ResolvePixelToWorldFrame`,
  `ResolveGameViewportPixelCenter/LocalCenter`, `SetUniverseCameraBasis`, `SyncUniverseCameraToViewport`
  dependencies used by snapshot/basis apply (keep whichever of these are declared here).
- `world/world.h`, `world/world_internal.h`, `world/universe.h` — `G_Universe`, `World2d`,
  `ResolvePixelToWorldFrame`, grid-space fields used by the snapshot and the universe-space basis.
- `system/viewport_system.h` — `game_viewport`/`lpanel_viewport`/`rpanel_viewport`,
  `ViewportRegion_ContainsPixel`, `SetViewportSpaceBasis`, `ResetViewportSpaceBasis`,
  `VIEWPORT_SPACE_*`, `VectorMagnitude_2d` usage in the dashboard.
- `system/ui/lpanel_system.h`, `system/ui/rpanel_system.h` — `GetLPanelSpaceFrame`,
  `GetRPanelSpaceFrame`, `SetLPanelSpaceBasis`, `SetRPanelSpaceBasis`, `ResetLPanelSpaceBasis`,
  `ResetRPanelSpaceBasis` for the LPANEL_SPACE / RPANEL_SPACE basis targets.
- `ui/cfont.h` — `FONT_BASIC`.
- `ui/text_region.h` — `DrawTextCustom`.
- `math/cvectors.h` — pulled transitively via `debug_overlay_system.h`; `Vector2d`, `Basis2d`,
  `Frame2d`, `ZERO_VECTOR_2D`, `IDENTITY_BASIS_2D`, `VectorScale_2d`, `ColourRgba`.

The authoritative list is "whatever the moved code references"; the moving engineer copies the
exact set of symbols used by the moved functions and prunes any include that no longer resolves a
used symbol. The point of this section is the partition **intent**: diagnostics owns fonts/text,
snapshot/universe/world, viewport, and panel-space includes.

**`debug_overlay_system.c` keeps only what the facade + viewport-scale hotkeys still need:**
- `system/debug_overlay_system.h` (own header), **plus new** `system/diagnostics.h`.
- `<stdio.h>` — `printf` in `HandleDebugToggleHotkeys` / `RefreshViewportAndDependentSystems`.
- `raylib.h` — `IsKeyPressed/Down` for the toggle + scale hotkeys.
- `camera/camera.h` — `UpdateCameraFull(&G_Universe.camera)` in `ApplyDebugHotkeyChanges`.
- `world/universe.h` (for `G_Universe` in that same camera refresh) and `world/world.h`,
  `world/world_internal.h`, `world/object_gizmos.h`, `world/universe.h` as still required by the
  remaining facade `ToggleDebug`/`IsDebugEnabled` cases (gizmos, world/universe view flags).
- `system/viewport_system.h` — `ToggleViewportDebugGrid`/`IsViewportDebugGridEnabled`/
  `DrawViewportDebugGrid`, `SetViewportTargetLogicalHeight`, `SetViewportUIScaleScalar`,
  `InitViewportLayout`, `SyncUniverseCameraToViewport`.
- `system/ui_system.h` — `ResetUI`, `GetCurrentMemoryAllocated`.
- `system/universe_system.h` — `SyncUniverseCameraToViewport`/`InitWorldSystem` plumbing if declared
  there (keep whichever header actually declares `InitWorldSystem`).
- **Dropped from the facade:** `ui/cfont.h`, `ui/text_region.h`, `system/ui/lpanel_system.h`,
  `system/ui/rpanel_system.h`, `<math.h>`, `<string.h>` — these were only used by the diagnostics
  code that moved out. (`<stdarg.h>` was only needed by `DrawDashboardRowf`, which moves.) The
  moving engineer removes any `#include` whose symbols are no longer referenced from the facade and,
  conversely, keeps any that still resolve a used symbol — the compiler/`-Wunused` is the final check.

## Error handling (behaviour-preserving — no new semantics)

This is a code move, so error handling is reproduced verbatim; no handling strategy changes.

- **`GetDebugBasisTargetOps` / `GetDebugBasisTargetFrame` / `ApplyDebugBasisTarget` /
  `ResetDebugBasisTarget`:** keep their existing bounds and NULL guards. Out-of-range target id →
  `NULL` ops → getters return `NULL`, apply returns `false`, reset is a no-op. Non-fatal; the
  caller already copes (basis editor skips mutation when the frame is `NULL`).
- **Basis apply failure in the editor** (`ApplyDebugBasisTarget` returns `false`): the code restores
  `basis_frame->basis = previous_basis` and prints `"[Basis Editor] Failed to apply basis for %s"`.
  Recoverable; the frame is rolled back, `basis_changed` stays unset for that mutation. Logged to
  stdout at the module level exactly as today.
- **`DrawDashboardRowf` guard:** keeps the `if (!row_y || !line || !fmt) return;` early-out.
  Defensive, non-fatal.
- **`Diagnostics_UpdateSnapshot`:** preserves the current `debug_snapshot.valid = false` invalidate,
  the `if (!dashboard_overlay_enabled) return;` early-out, and the `target_world_index` bounds check
  before indexing `G_Universe.worlds`. No new failure modes.
- **Facade delegation functions:** `Diagnostics_ToggleDashboard`/`IsDashboardEnabled`/`DrawDashboard`
  cannot fail; `Diagnostics_HandleBasisEditorHotkeys` returns the `basis_changed` bool as before.

### Input validation

The only external inputs are keyboard events (raylib `IsKeyPressed/Down`) and mouse coordinates
(`GetMouseX/Y`), both already handled by the moved code. Mouse pixel → world resolution and the
cell-index computation keep their existing range checks (`child_local` within world resolution).
No new external input is introduced, so no new validation rules are added. `Diagnostics_*` entry
points take no caller-supplied pointers except via the unchanged `UpdateDebugOverlayHotkeys` out
params, which the facade already NULL-checks before delegating.

## Invariant ownership

- **Dashboard enabled-state** (`dashboard_overlay_enabled`) is now owned solely by `diagnostics.c`;
  the facade may only flip/read it through `Diagnostics_ToggleDashboard`/`IsDashboardEnabled`. This
  mirrors how gizmo state is owned by `object_gizmos.c`. Single-owner enforcement lives in the module
  because the state and all its mutators now live there.
- **Basis-editor state** (`basis_editor_enabled`, `basis_editor_editing_u`, `basis_editor_target`)
  is owned by `diagnostics.c`; the facade never touches it directly, only observes "is universe
  space active?" via the predicate. Owned by the module because it is purely diagnostic state.
- **Basis-target registry** (`debug_basis_target_ops`) is owned by `diagnostics.c` as the single
  source of truth for both the dashboard read-out and the basis-editor mutation. Enforced in the
  module because both consumers now live there.
- **Viewport/UI refresh orchestration** (`RefreshViewport*`, `ApplyDebugHotkeyChanges`) stays owned
  by the facade, which is the layer that holds the screen/scale parameters.

## Testability

- **Unit-testable in isolation:** the basis-target ops accessors (`GetDebugBasisTargetOps`,
  `GetDebugBasisTargetName` and the range guards) are pure table lookups — testable with a thin
  harness if the project later adds one, though they are `static` so would need either a test hook
  or test inclusion of the `.c`. Behaviour is unchanged, so existing manual verification still holds.
- **Integration-tested (manual, as today):** the dashboard rendering, basis-editor hotkeys, and the
  viewport refresh are raylib-frame-driven and verified by running the app and exercising F1–F12,
  F5 + TAB/U/V/I/J/K/L/O/P/BACKSPACE, and F7–F10. Because this is a pure move with identical hotkey
  routing, the acceptance test is "every hotkey and the dashboard/basis editor behave exactly as
  before the split."
- The split does not make anything harder to test; it narrows `debug_overlay_system.c` to the facade
  surface and isolates the diagnostics, which is marginally better for reasoning about each half.

## Explicit guarantees

- **Git untouched:** no staging, no commits, no rebase, no branch changes. All edits remain
  uncommitted for the user.
- **Behaviour-identical:** same hotkeys, same dashboard layout and text, same basis editor, same
  toggles, same console prints, same frame-skip behaviour. Public header signatures in
  `debug_overlay_system.h` are unchanged, so `raylib_game.c` (`DrawGlobalDebugOverlays` ~394,
  `UpdateDebugOverlayHotkeys` ~399) and `universe_renderer.c` (`DrawUniverseDebugOverlays` ~136)
  compile and behave without edits.
- **No compile-out guards, no `#ifdef`:** this is a clean module split only. (The previously
  rejected release compile-out guard is explicitly not part of this design.)
- **No CMake edit:** `file(GLOB_RECURSE ... CONFIGURE_DEPENDS *.c)` auto-registers
  `src/engine/system/diagnostics.c` on reconfigure.

## Implementation checklist (for the implementing step)

1. Create `include/system/diagnostics.h` with the six-function API above + banner comment.
2. Create `src/engine/system/diagnostics.c`; move every Group-A symbol into it (keeping statics
   static), add the five public wrappers (`Diagnostics_ToggleDashboard`,
   `Diagnostics_IsDashboardEnabled`, `Diagnostics_DrawDashboard`, `Diagnostics_UpdateSnapshot`,
   `Diagnostics_HandleBasisEditorHotkeys`) plus `Diagnostics_ActiveBasisTargetIsUniverseSpace`.
3. In `debug_overlay_system.c`: delete the moved symbols; add `#include "system/diagnostics.h"`;
   point the `DEBUG_DASHBOARD` facade cases at the module; rewrite `DrawGlobalDebugOverlays`,
   `DrawUniverseDebugOverlays`, `UpdateDebugOverlayHotkeys`, and `ApplyDebugHotkeyChanges` to call the
   module; prune now-unused includes.
4. Keep `RefreshViewportBase`/`ClampViewportLogicalHeight`/`RefreshViewportForBasisEdit`/
   `RefreshViewportAndDependentSystems`/`HandleViewportScaleHotkeys`/`ApplyDebugHotkeyChanges`/
   `HandleDebugToggleHotkeys` in the facade.
5. Reconfigure + build with the UCRT64 + Ninja preset; fix any include/symbol fallout; confirm no
   behavioural change by exercising the hotkeys.
6. Add function/description comments and UK English spelling throughout, per project conventions.

## Responses to design review (`diagnostics-extraction-review.json`, verdict CHANGES_REQUESTED)

All three findings are **addressed** in this revision. None are backlogged or ignored; each
change aligns with the original requirement of a behaviour-identical, pure code-move split.

- **Finding 1 (MEDIUM) — `Diagnostics_ActiveBasisTargetIsUniverseSpace` contradictory contract.**
  ADDRESSED. The header doc comment is rewritten to describe a boolean 0/1 predicate ("return
  non-zero when the active basis-editor target is universe space") and pins the implementation
  to `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;`. The stale "return the
  target id as an int" wording is removed, and the "Why" rationale now explicitly rejects the
  id-as-int reading (several target ids are truthy, so it would misfire the universe-space
  branch). Only the predicate interpretation remains, which matches the function name and the
  sole `if (...)` call site, preserving the current branch exactly.

- **Finding 2 (NIT) — "five entry points" undercounts the six-function API.** ADDRESSED. The
  Public API intro now reads "six entry points (five wrappers plus one predicate)", and the
  cross-reference in the no-cycle paragraph was updated from "five entry points" to "six entry
  points". The implementation-checklist step 1 header-count was updated to "six-function API".
  No design change — wording only.

- **Finding 3 (NIT) — `DEBUG_UI_BORDERS` mischaracterised as "local".** ADDRESSED. The
  delegation section now describes `ui_borders_enabled` as a UI-owned extern defined in
  `ui_system.c` and declared in `debug_overlay_system.h`, toggled in place by the facade. No
  refactor impact: `DEBUG_UI_BORDERS` still STAYS in the facade unchanged.

The nine gate checks in the review all PASS and are unchanged by this revision; the edits above
resolve only the clarity defects the review flagged.
