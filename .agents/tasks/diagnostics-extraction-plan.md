# Implementation Plan: Extract developer diagnostics out of `debug_overlay_system`

Derived from the APPROVED design at `.agents/tasks/diagnostics-extraction-design.md`
(review verdict APPROVED, `.agents/tasks/diagnostics-extraction-review.json`). This is a
pure code-move / ownership refactor: lift the debug **dashboard**, the viewport-space
**basis editor**, and the shared **basis-target machinery** out of
`src/engine/system/debug_overlay_system.c` into a new `diagnostics` module, leaving the
facade as the toggle surface + F7–F10 shipping viewport-scale hotkeys + the three public
entry points that now delegate into the module.

## Hard constraints (apply to every step)

- **Behaviour-identical.** Same hotkeys, same dashboard layout and text, same basis editor,
  same toggles, same console prints, same frame-skip behaviour. No logic changes.
- **No compile-out guards, no `#ifdef`.** Clean module split only.
- **Git untouched.** Do NOT stage, commit, rebase, or alter git in any way. All edits stay
  uncommitted for the user. Do NOT create a worktree.
- **Public header `include/system/debug_overlay_system.h` is unchanged** — its three function
  signatures (`UpdateDebugOverlayHotkeys`, `DrawUniverseDebugOverlays`, `DrawGlobalDebugOverlays`)
  must stay byte-identical so `src/raylib_game.c` (lines ~394/~399) and
  `src/engine/world/universe_renderer.c` (line ~136) compile and behave without edits.
- **UK English** spelling in all new prose/comments. Note: existing identifiers use US spelling
  (`color`, `ColourRgba` is the project's own type) — do NOT rename existing symbols; only new
  comments/prose use UK English.
- **Descriptive comments**, per `AGENTS.md`: every new function gets a one-line description
  comment above it; non-obvious logic gets an inline comment. The banner comment on the new
  header is specified verbatim in the design.
- Do NOT run the produced `raylib-game.exe`. Build/verify only.

## Exact MOVE / STAY mapping (confirmed against the current source)

The current `src/engine/system/debug_overlay_system.c` is ~910 lines. The symbols below are
confirmed present. **MOVE** = cut from `debug_overlay_system.c` and paste into `diagnostics.c`
(keeping `static` where it was `static`). **STAY** = leave untouched in `debug_overlay_system.c`.

### MOVE → `diagnostics.c` (Group A)

Basis-target machinery (all `static`/file-private, keep private):
- `typedef enum DebugBasisTargetId { … }` (6 ids + `DEBUG_BASIS_TARGET_COUNT`)
- `typedef struct DebugBasisTargetOps { … }`
- `GetLPanelViewportFrame`, `GetGameViewportFrame`, `GetRPanelViewportFrame`, `GetUniverseSpaceFrame`
- `ApplyLPanelViewportBasis`, `ApplyGameViewportBasis`, `ApplyRPanelViewportBasis`,
  `ApplyUniverseSpaceBasis`, `ApplyLPanelSpaceBasis`, `ApplyRPanelSpaceBasis`
- `ResetLPanelViewportBasis`, `ResetGameViewportBasis`, `ResetRPanelViewportBasis`, `ResetUniverseSpaceBasis`
- `static const DebugBasisTargetOps debug_basis_target_ops[DEBUG_BASIS_TARGET_COUNT]` (the table)
- `GetDebugBasisTargetOps`, `GetDebugBasisTargetFrame`, `ApplyDebugBasisTarget`,
  `ResetDebugBasisTarget`, `GetDebugBasisTargetName`

Dashboard:
- `typedef struct UniverseDebugSnapshot { … }` and `static UniverseDebugSnapshot debug_snapshot = {0};`
- `static bool dashboard_overlay_enabled = false;`
- `GetOnOffLabel`
- `DrawDashboardLine`, `DrawDashboardRowf` (the latter uses `va_list` → needs `<stdarg.h>`)
- `typedef struct DashboardBasisData { … }` and `ResolveDashboardBasisData`
- `DrawDashboardControlsSection`, `DrawDashboardUniverseSection`, `DrawDashboardViewportBasisSection`
- `DrawDebugDashboard` (dashboard composition root; wrapped by `Diagnostics_DrawDashboard`)
- The entire body currently inside `DrawUniverseDebugOverlays` (snapshot invalidate + early-out
  `if (!dashboard_overlay_enabled) return;` + snapshot population) → becomes `Diagnostics_UpdateSnapshot`

Basis editor:
- `static bool basis_editor_enabled = false;`
- `static bool basis_editor_editing_u = true;`
- `static DebugBasisTargetId basis_editor_target = DEBUG_BASIS_TARGET_LPANEL_VIEWPORT;`
- `HandleBasisVectorMutation`
- `HandleBasisEditorHotkeys` body (incl. the trailing `if (basis_changed)` print block) →
  wrapped by `Diagnostics_HandleBasisEditorHotkeys`

Keep the two existing explanatory comments that sit beside the moved statics ONLY if they still
describe moved code; the comment block about world-grid / object-gizmo delegation sits beside the
facade statics and relates to the facade `ToggleDebug` cases — that comment STAYS with the facade.

### STAY in `debug_overlay_system.c` (Groups B + C)

- `RefreshViewportBase` (static)
- `ClampViewportLogicalHeight` (static)
- `RefreshViewportForBasisEdit` (static) — **STAYS**; see decision below
- `RefreshViewportAndDependentSystems` (static)
- `HandleViewportScaleHotkeys` (static, F7–F10)
- `ApplyDebugHotkeyChanges` (static) — edited to call the module predicate (see step 4)
- `HandleDebugToggleHotkeys` (static, F1/F2/F3/F4/F6/F11/F12 + Ctrl+F1)
- `ToggleDebug`, `IsDebugEnabled` (public) — `DEBUG_DASHBOARD` cases now delegate to the module
- `UpdateDebugOverlayHotkeys`, `DrawGlobalDebugOverlays`, `DrawUniverseDebugOverlays` (public entry
  points) — rewired to call the module

### `RefreshViewportForBasisEdit` placement decision (from the design)

`RefreshViewportForBasisEdit` **STAYS** in `debug_overlay_system.c`. Its sole call site is inside
`ApplyDebugHotkeyChanges` (the `basis_changed && !universe-space` branch), which is a Group-B
viewport orchestrator that stays. It is built on `RefreshViewportBase` + `ResetUI`, which belong to
the viewport/UI refresh concern owned by the facade. Moving it would force re-exposing
`RefreshViewportBase`/`InitViewportLayout`/`SetViewportTargetLogicalHeight`/`SetViewportUIScaleScalar`/
`SyncUniverseCameraToViewport`/`ResetUI` plus threading the screen/scale params across the module
boundary — more coupling for no behavioural gain. The module only needs to answer "is the active
target universe space?", provided by `Diagnostics_ActiveBasisTargetIsUniverseSpace()`.

## Include partition

**`src/engine/system/diagnostics.c` includes** (exactly the subset the moved code references):
- `"system/diagnostics.h"` (own header)
- `"system/debug_overlay_system.h"` — `IsDebugEnabled`/`DebugOverlayId` for the dashboard ON/OFF rows
  (also transitively provides `math/cvectors.h` → `Vector2d`/`Basis2d`/`Frame2d`/`ZERO_VECTOR_2D`/
  `IDENTITY_BASIS_2D`/`VectorScale_2d`/`ColourRgba` and `viewport_system.h`)
- `"raylib.h"` — `GetScreenWidth/Height`, `GetMouseX/Y`, `IsKeyPressed/Down`, `DrawRectangle*`, `Color`
- `"system/universe_system.h"` — `Universe_FindWorldAt`, `ResolvePixelToWorldFrame`,
  `ResolveGameViewportPixelCenter`, `ResolveGameViewportLocalCenter`, `SetUniverseCameraBasis`,
  `SyncUniverseCameraToViewport` (keep whichever this header actually declares)
- `"world/world.h"`, `"world/world_internal.h"`, `"world/universe.h"` — `G_Universe`, `World2d`,
  grid-space fields used by the snapshot and the universe-space basis
- `"system/viewport_system.h"` — `game_viewport`/`lpanel_viewport`/`rpanel_viewport`,
  `ViewportRegion_ContainsPixel`, `SetViewportSpaceBasis`, `ResetViewportSpaceBasis`,
  `VIEWPORT_SPACE_*`, `VectorMagnitude_2d` (may already come via the facade header; keep explicit)
- `"system/ui/lpanel_system.h"`, `"system/ui/rpanel_system.h"` — `GetLPanelSpaceFrame`,
  `GetRPanelSpaceFrame`, `SetLPanelSpaceBasis`, `SetRPanelSpaceBasis`, `ResetLPanelSpaceBasis`,
  `ResetRPanelSpaceBasis`
- `"ui/cfont.h"` — `FONT_BASIC`
- `"ui/text_region.h"` — `DrawTextCustom`
- `<math.h>` — `floorf`
- `<string.h>` — `memset`
- `<stdio.h>` — `printf`/`snprintf`/`vsnprintf`
- `<stdarg.h>` — `va_list`/`va_start`/`va_end` (make explicit; previously transitive)

**`diagnostics.c` does NOT include `camera/camera.h`.** It reaches `G_Universe.camera.*` via
`world/universe.h`; `UpdateCameraFull` stays in the facade. (Review finding 1.)

**`src/engine/system/debug_overlay_system.c` keeps only** what the facade + F7–F10 path still need:
- `"system/debug_overlay_system.h"` (own header) **+ new `"system/diagnostics.h"`**
- `"raylib.h"` — `IsKeyPressed/Down`
- `"camera/camera.h"` — `UpdateCameraFull(&G_Universe.camera)` in `ApplyDebugHotkeyChanges`
- `"world/universe.h"`, `"world/world.h"`, `"world/world_internal.h"`, `"world/object_gizmos.h"` —
  still used by the remaining `ToggleDebug`/`IsDebugEnabled` cases (gizmos, world/universe view flags)
  and the camera refresh
- `"system/viewport_system.h"` — `ToggleViewportDebugGrid`/`IsViewportDebugGridEnabled`/
  `DrawViewportDebugGrid`, `SetViewportTargetLogicalHeight`, `SetViewportUIScaleScalar`,
  `InitViewportLayout`, `SyncUniverseCameraToViewport`
- `"system/ui_system.h"` — `ResetUI`, `GetCurrentMemoryAllocated`
- `"system/universe_system.h"` — `InitWorldSystem` / `SyncUniverseCameraToViewport` plumbing (keep
  whichever header actually declares `InitWorldSystem`)
- **Dropped from the facade** (moved out with the diagnostics): `"ui/cfont.h"`, `"ui/text_region.h"`,
  `"system/ui/lpanel_system.h"`, `"system/ui/rpanel_system.h"`, `<math.h>`, `<string.h>`.
  (`<stdarg.h>` was never explicitly included; it was transitive for the moved `DrawDashboardRowf`.)
  `<stdio.h>` STAYS (facade still prints).

The authoritative rule for both files: keep an include only if a symbol it provides is still
referenced from that file; let the compiler + `-Wunused`-class diagnostics be the final arbiter.
Prune include fallout iteratively during the build step, not speculatively.

---

# Ordered steps

- [ ] 1. Create the new module header `include/system/diagnostics.h`.
      Add the banner comment and the six-function public API exactly as specified in the design's
      "Public API of `diagnostics.h`" block: `Diagnostics_ToggleDashboard`,
      `Diagnostics_IsDashboardEnabled`, `Diagnostics_DrawDashboard`, `Diagnostics_UpdateSnapshot`,
      `Diagnostics_HandleBasisEditorHotkeys`, and `Diagnostics_ActiveBasisTargetIsUniverseSpace`
      (returns `int`). Include guard `DIAGNOSTICS_H`; `#include <stdbool.h>`. Keep the per-function
      doc comments from the design verbatim (UK English). Nothing else is exposed.
      Files: `include/system/diagnostics.h` (new)
      Verify: header-only; no standalone build. Correctness is proven by step 5's full build.

- [ ] 2. Create `src/engine/system/diagnostics.c` and move every Group-A symbol into it.
      Paste the moved symbols in dependency order so the file compiles top-to-bottom:
      (1) the `DebugBasisTargetId` enum + `DebugBasisTargetOps` struct; (2) the frame-getters,
      apply callbacks, reset callbacks; (3) the `debug_basis_target_ops[]` table; (4) the
      `GetDebugBasisTargetOps`/`Frame`/`ApplyDebugBasisTarget`/`ResetDebugBasisTarget`/`...Name`
      accessors; (5) `UniverseDebugSnapshot` + `debug_snapshot`; (6) the three basis-editor statics
      (`basis_editor_enabled`, `basis_editor_editing_u`, `basis_editor_target`) and
      `dashboard_overlay_enabled`; (7) `GetOnOffLabel`, `DrawDashboardLine`, `DrawDashboardRowf`;
      (8) `DashboardBasisData` + `ResolveDashboardBasisData`; (9) the three `DrawDashboard*Section`
      helpers; (10) `DrawDebugDashboard`; (11) `HandleBasisVectorMutation`.
      Then add the six public wrappers at the bottom:
      - `Diagnostics_ToggleDashboard` → `dashboard_overlay_enabled = !dashboard_overlay_enabled;`
      - `Diagnostics_IsDashboardEnabled` → `return dashboard_overlay_enabled;`
      - `Diagnostics_DrawDashboard` → `DrawDebugDashboard();`
      - `Diagnostics_UpdateSnapshot` → the entire moved body of the old `DrawUniverseDebugOverlays`
        (snapshot invalidate, `if (!dashboard_overlay_enabled) return;` early-out, snapshot population)
      - `Diagnostics_HandleBasisEditorHotkeys` → the moved body of the old `HandleBasisEditorHotkeys`
        (returns `basis_changed`)
      - `Diagnostics_ActiveBasisTargetIsUniverseSpace` →
        `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;`
      Apply the `diagnostics.c` include partition above (no `camera/camera.h`). Add a one-line
      description comment above each wrapper. Do NOT alter any moved logic, text, or print strings.
      Files: `src/engine/system/diagnostics.c` (new)
      Verify: compiles as part of step 5's full build (new `.c` auto-registers via GLOB on reconfigure).

- [ ] 3. Trim `src/engine/system/debug_overlay_system.c`: delete every Group-A symbol now living in
      `diagnostics.c` (all symbols listed under "MOVE" above — enum, struct, table, getters/apply/
      reset, accessors, snapshot struct+global, the four diagnostic statics, all dashboard helpers,
      `DrawDebugDashboard`, `HandleBasisVectorMutation`, and the old `HandleBasisEditorHotkeys`
      function). Add `#include "system/diagnostics.h"`. Point both facade `DEBUG_DASHBOARD` cases at
      the module: in `ToggleDebug` → `Diagnostics_ToggleDashboard();`; in `IsDebugEnabled` →
      `return Diagnostics_IsDashboardEnabled();`. Leave all other `ToggleDebug`/`IsDebugEnabled`
      cases and the world-grid/gizmo delegation comment block untouched.
      Files: `src/engine/system/debug_overlay_system.c`
      Verify: compiles as part of step 5's full build.

- [ ] 4. Rewire the three public entry points + `ApplyDebugHotkeyChanges` in
      `debug_overlay_system.c` to delegate into the module (identical behaviour):
      - `DrawGlobalDebugOverlays`: keep `DrawViewportDebugGrid();` then
        `if (Diagnostics_IsDashboardEnabled()) { Diagnostics_DrawDashboard(); }`.
      - `DrawUniverseDebugOverlays`: replace the whole body with a one-line delegate
        `Diagnostics_UpdateSnapshot();` plus a comment. (Do NOT rename the function — the two callers
        depend on the name/signature; review finding 2.)
      - `UpdateDebugOverlayHotkeys`: keep the call order; the basis-editor call becomes
        `basis_changed = Diagnostics_HandleBasisEditorHotkeys();`. `HandleDebugToggleHotkeys`,
        `HandleViewportScaleHotkeys`, and `ApplyDebugHotkeyChanges` calls stay.
      - `ApplyDebugHotkeyChanges`: replace the branch test
        `if (basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE)` with
        `if (Diagnostics_ActiveBasisTargetIsUniverseSpace())`. The `UpdateCameraFull(&G_Universe.camera)`
        and `RefreshViewportForBasisEdit(...)` branches are unchanged.
      Then prune now-unused includes from `debug_overlay_system.c` per the facade partition above
      (drop `ui/cfont.h`, `ui/text_region.h`, `system/ui/lpanel_system.h`, `system/ui/rpanel_system.h`,
      `<math.h>`, `<string.h>`); keep `<stdio.h>` and the rest. Let the step-5 build confirm the prune.
      Files: `src/engine/system/debug_overlay_system.c`
      Verify: compiles as part of step 5's full build; the three public signatures are byte-identical
      to the unchanged `include/system/debug_overlay_system.h`.

- [ ] 5. Reconfigure (REQUIRED — a new `.c` was added) and build with the UCRT64 + Ninja preset, then
      fix any include/symbol fallout (unresolved symbol → a referenced include was dropped; unused
      include → prune it). Iterate until the build is clean. Do NOT edit `src/CMakeLists.txt` — the
      `file(GLOB_RECURSE ... CONFIGURE_DEPENDS *.c)` picks up the new file on reconfigure.
      Files: none (build only; possibly minor include adjustments in the two `.c` files)
      Verify (Windows PowerShell, from repo root `c:\Projects\raylib-testing`):
      ```powershell
      cmake --preset debug
      cmake --build --preset debug; "EXIT=$LASTEXITCODE"
      ```
      Expected: configure succeeds; `diagnostics.c` appears in the build; `raylib-game.exe` links and
      `EXIT=0`. The `ninja: warning: premature end of file; recovering` prefix is NOT an error.
      If every `.c` compiles and ONLY the final link fails with
      `ld.exe: cannot open output file raylib-game\raylib-game.exe: Permission denied`, the game is
      currently running (env lock), NOT a code error: run `Get-Process -Name raylib-game` to confirm,
      report that the user must close the running game, do NOT kill the process, and do NOT run the exe.

- [ ] 6. Final consistency pass (no behavioural change). Confirm: no `#ifdef`/compile-out guards were
      introduced; all moved print strings, dashboard text, hotkeys, and the frame-skip early-out are
      byte-identical to the originals; `include/system/debug_overlay_system.h` is unchanged; new
      functions each have a description comment; UK English in new prose. Confirm git was NOT touched
      (changes remain uncommitted — do not stage/commit).
      Files: none (review only)
      Verify: `git status` shows only the expected modified/new files (two new under `diagnostics`,
      one modified `.c`) and nothing staged/committed; the step-5 build is green.

## Verification summary (stop contract)

The implement-and-review loop stops when
`.agents/tasks/diagnostics-extraction-impl-review.json` has jsonPath `verdict` equal to `APPROVED`.
Primary acceptance = a clean `cmake --build --preset debug` with `EXIT=0` after a
`cmake --preset debug` reconfigure, with behaviour preserved exactly per the hard constraints above.
```
