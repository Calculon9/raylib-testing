# Design review: Extract developer diagnostics out of `debug_overlay_system`

Reviewed document: `.agents/tasks/diagnostics-extraction-design.md`
Scope: pure code-move / ownership refactor, no behaviour change. This review was done
fresh against the actual source, not against the design's own claims.

## Verdict

**APPROVED.** No HIGH or MEDIUM findings. The three NITs below are clarity/consistency
polish and do not block implementation; they are recorded only so the implementing engineer
is aware of them.

This revision of the design has resolved the three findings from the prior review round
(the `Diagnostics_ActiveBasisTargetIsUniverseSpace` contradictory contract, the "five entry
points" undercount, and the `DEBUG_UI_BORDERS` "local" mischaracterisation). I re-verified
each against source and confirm they are genuinely fixed.

---

## Gate checks (all PASS)

1. **Behaviour identical — same hotkeys, dashboard, basis editor, toggles.** PASS.
   Verified against `debug_overlay_system.c`: the toggle hotkeys
   (F1 axes / Ctrl+F1 ResetUI, F2 world-grid-labels, F3 UI-borders, F4 universe-grid-labels,
   F6 viewport-grid, F11 dashboard, F12 hull / Shift+F12 AABB) all live in
   `HandleDebugToggleHotkeys` which STAYS; the basis-editor hotkeys (F5 toggle, TAB/U/V/
   I/J/K/L/O/P/BACKSPACE) live in `HandleBasisEditorHotkeys` which moves as a unit and is
   re-exposed as `Diagnostics_HandleBasisEditorHotkeys`; F7–F10 live in
   `HandleViewportScaleHotkeys` which STAYS. The call order inside `UpdateDebugOverlayHotkeys`
   is preserved exactly (toggle → basis → scale → apply). No hotkey is dropped, rebound, or
   reordered. Dashboard layout/text and console prints move verbatim with their functions.

2. **Facade still works for ALL DebugOverlayId cases; DEBUG_DASHBOARD delegates.** PASS.
   The design keeps the full switch in `ToggleDebug`/`IsDebugEnabled` and only rewrites the
   `DEBUG_DASHBOARD` case to call `Diagnostics_ToggleDashboard()` /
   `Diagnostics_IsDashboardEnabled()`, mirroring the existing `DEBUG_OBJECT_*` → gizmo
   delegation (confirmed real: `include/world/object_gizmos.h` exposes
   `ToggleGizmo`/`IsGizmoEnabled`). All other cases (`DEBUG_VIEWPORT_GRID`, `DEBUG_WORLD_GRID`,
   `DEBUG_WORLD_GRID_LABELS`, `DEBUG_UNIVERSE_GRID_LABELS`, `DEBUG_UI_BORDERS`,
   `DEBUG_OBJECT_AXES/HULL/AABB`, `default`) are explicitly left unchanged.

3. **Public header signatures UNCHANGED.** PASS. `include/system/debug_overlay_system.h`
   declares `void DrawGlobalDebugOverlays(void)`,
   `void UpdateDebugOverlayHotkeys(int,int,int,float*,int*)`,
   `void DrawUniverseDebugOverlays(void)`, plus `ToggleDebug`/`IsDebugEnabled` and
   `extern bool ui_borders_enabled`. The design keeps all three entry points in the facade
   with identical signatures. Callers compile unchanged — verified:
   `raylib_game.c:394` (`DrawGlobalDebugOverlays`), `raylib_game.c:399`
   (`UpdateDebugOverlayHotkeys`), `universe_renderer.c:136` (`DrawUniverseDebugOverlays`).

4. **Dependency direction diagnostics → facade.** PASS. `diagnostics.c` includes
   `system/debug_overlay_system.h` to call `IsDebugEnabled()` for the dashboard ON/OFF rows
   (consumer). `debug_overlay_system.c` includes `system/diagnostics.h` to call the six
   delegation entry points (facade → module for the dashboard toggle). The two `.c` files
   depend on each other's headers; this is header-level, not a link cycle, and matches the
   gizmo precedent. The module never calls back into `ToggleDebug`. Correct.

5. **No new global duplicated; no dead/duplicate statics left behind.** PASS. Each moved
   static (`dashboard_overlay_enabled`, `basis_editor_enabled`, `basis_editor_editing_u`,
   `basis_editor_target`, `debug_snapshot`, `debug_basis_target_ops`) moves to the module and
   is deleted from the facade — the design's checklist step 3 explicitly says "delete the moved
   symbols". The facade reaches dashboard state only through the two delegation functions and
   the universe-space predicate, so there is no second copy. No global is promoted to extern.

6. **MOVE/STAY list complete over every symbol in groups A/B/C.** PASS. I enumerated every
   top-level symbol in `debug_overlay_system.c` and matched it to the list:
   - Group A (MOVE): `DebugBasisTargetId`, `DebugBasisTargetOps`, `debug_basis_target_ops[]`,
     the four frame getters, the six apply callbacks, the four reset callbacks, the five
     ops accessors (`GetDebugBasisTargetOps/Frame/Name`, `ApplyDebugBasisTarget`,
     `ResetDebugBasisTarget`), `dashboard_overlay_enabled`, `UniverseDebugSnapshot`+
     `debug_snapshot`, `GetOnOffLabel`, `DrawDashboardLine`, `DrawDashboardRowf`,
     `DashboardBasisData`+`ResolveDashboardBasisData`, the three dashboard section renderers,
     `DrawDebugDashboard`, the snapshot body from `DrawUniverseDebugOverlays`,
     `basis_editor_*` statics, `HandleBasisVectorMutation`, `HandleBasisEditorHotkeys`.
   - Group B (STAY): `RefreshViewportBase`, `ClampViewportLogicalHeight`,
     `RefreshViewportForBasisEdit`, `RefreshViewportAndDependentSystems`,
     `HandleViewportScaleHotkeys`, `ApplyDebugHotkeyChanges`.
   - Group C (STAY): `ToggleDebug`, `IsDebugEnabled`, `HandleDebugToggleHotkeys`,
     `UpdateDebugOverlayHotkeys`, `DrawGlobalDebugOverlays`, `DrawUniverseDebugOverlays`.
   Every symbol in the file is accounted for. The shared basis-target machinery (used by both
   the dashboard read-out and the basis-editor mutation) is MOVED as the design requires, and
   the shipping viewport controls (F7–F10 path) are KEPT.

7. **`RefreshViewportForBasisEdit` placement justified with actual callers.** PASS. I ran a
   whole-codebase search for `RefreshViewportForBasisEdit`: the definition is at
   `debug_overlay_system.c:205` and the only call site is inside `ApplyDebugHotkeyChanges`
   (the `basis_changed && !universe-space` branch, ~`debug_overlay_system.c:833`). No other
   reference anywhere. The design's "verified callers" section matches reality, and the STAY
   decision follows from it: the sole caller is a Group-B orchestrator that stays, and the
   function is built on `RefreshViewportBase` + `ResetUI` (facade-owned refresh plumbing).
   Keeping it in the facade avoids re-exposing that plumbing across the module boundary. The
   justification is grounded in the real call graph, not a guess.

8. **No compile-out guards / `#ifdef`.** PASS. The design states twice (Overview and Explicit
   Guarantees) that there are no `#ifdef`/compile-out guards and explicitly rejects the
   previously-floated release compile-out of the diagnostics bodies. Clean module split only.

9. **Git left untouched.** PASS. Both the Overview and the "Explicit guarantees → Git
   untouched" bullet state no staging, no commits, no rebase, no branch changes — all edits
   stay uncommitted for the user.

---

## Findings

### NIT 1 — Include partition for `diagnostics.c` is deliberately non-authoritative

Location: design section "Include partition", `diagnostics.c includes` list.

The design hedges several includes ("include only if…", "keep whichever of these are
declared here", "pulled transitively via `debug_overlay_system.h`") and states the
authoritative list is "whatever the moved code references". That is a reasonable delegation
to the implementing engineer and the compiler, and it does not change behaviour, so it does
not block. One concrete anchor to reduce guesswork: the moved code definitely needs
`system/viewport_system.h` (`game_viewport`/`lpanel_viewport`/`rpanel_viewport`,
`SetViewportSpaceBasis`, `ResetViewportSpaceBasis`, `VIEWPORT_SPACE_*`, `VectorMagnitude_2d`),
`system/ui/lpanel_system.h` + `system/ui/rpanel_system.h` (`GetLPanelSpaceFrame`/
`GetRPanelSpaceFrame` and the `Set*/Reset*` panel-space basis fns — confirmed declared in
those headers), `system/universe_system.h` (`Universe_FindWorldAt`,
`ResolvePixelToWorldFrame`, `ResolveGameViewportPixelCenter/LocalCenter`,
`SetUniverseCameraBasis`, `SyncUniverseCameraToViewport`), `world/*` for `G_Universe`,
`ui/cfont.h` (`FONT_BASIC`), `ui/text_region.h` (`DrawTextCustom`), and `<math.h>`/`<string.h>`/
`<stdio.h>`/`<stdarg.h>`. The design already lists all of these; the NIT is only that the
"omit if unused" phrasing on `camera/camera.h` is correct (the module reads
`G_Universe.camera.*` through `universe.h`, and `UpdateCameraFull` stays in the facade), so
`camera/camera.h` should simply be omitted from `diagnostics.c` rather than left as a
"maybe". Concrete fix: state plainly "diagnostics.c does NOT include `camera/camera.h`."

### NIT 2 — `DrawUniverseDebugOverlays` name no longer matches its reduced role

Location: Group C table and "public entry points" section 2.

After the move, `DrawUniverseDebugOverlays` draws nothing — it only calls
`Diagnostics_UpdateSnapshot()` to populate per-frame data. The name is now a misnomer. This
is behaviour-irrelevant and the public signature must stay (gate 3), so renaming the public
function is out of scope and must NOT be done. Recorded only so the implementer does not
"tidy" the name and break the two callers. Concrete fix: none required; keep the name as-is
and rely on the one-line delegate comment the design already specifies.

### NIT 3 — Line-number reference `debug_overlay_system.c:833` is approximate

Location: "`RefreshViewportForBasisEdit` placement decision → Verified callers".

The design cites the sole call site as `debug_overlay_system.c:833`. The call site is inside
`ApplyDebugHotkeyChanges` and is near that line but will shift as symbols are deleted during
the move. The claim is substantively correct (one call site, inside `ApplyDebugHotkeyChanges`);
only the absolute line number is brittle. Concrete fix: optionally phrase it as "the sole call
site, inside `ApplyDebugHotkeyChanges`" without the hard line number.

---

## Verified assumptions

- `include/system/debug_overlay_system.h` declares exactly the three public entry points named
  (`DrawGlobalDebugOverlays`, `UpdateDebugOverlayHotkeys`, `DrawUniverseDebugOverlays`) plus
  `ToggleDebug`/`IsDebugEnabled` and `extern bool ui_borders_enabled` — signatures match the
  design's "unchanged" claim.
- Public callers exist and are unchanged by the refactor: `raylib_game.c:394`/`:399`,
  `universe_renderer.c:136`.
- `RefreshViewportForBasisEdit` has exactly one call site (inside `ApplyDebugHotkeyChanges`,
  the non-universe-space basis branch); definition at `debug_overlay_system.c:205`. No other
  references in the repository. The placement justification is grounded in this real call graph.
- The object-gizmos precedent is real: `include/world/object_gizmos.h` exposes
  `ToggleGizmo`/`IsGizmoEnabled`, and the facade's `DEBUG_OBJECT_*` cases already delegate to
  it — the design's delegation pattern copies this exactly.
- `ui_borders_enabled` is a UI-owned extern: defined in `src/engine/system/ui_system.c:37`,
  declared `extern bool ui_borders_enabled;` in `include/system/debug_overlay_system.h:26`.
  The revised design describes it correctly (prior review's Finding 3 is fixed).
- `GetLPanelSpaceFrame`/`GetRPanelSpaceFrame` and the panel-space basis setters/resetters are
  declared in `include/system/ui/lpanel_system.h` / `rpanel_system.h`, backing the include
  partition for the LPANEL_SPACE/RPANEL_SPACE basis targets.
- `src/CMakeLists.txt` uses `file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)`, so the new
  `src/engine/system/diagnostics.c` auto-registers on reconfigure with no CMake edit — the
  "no CMake edit" claim holds.
- `ApplyDebugHotkeyChanges` currently branches on
  `basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE` to pick
  `UpdateCameraFull(&G_Universe.camera)` vs `RefreshViewportForBasisEdit(...)`. The predicate
  `Diagnostics_ActiveBasisTargetIsUniverseSpace()` is a faithful 1:1 replacement for that test,
  and the revised header doc now consistently describes it as a boolean 0/1 predicate pinned to
  `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;` (prior review's Finding 1
  is fixed).
- Every top-level symbol in `debug_overlay_system.c` appears in exactly one of the MOVE/STAY
  groups; the list is complete.

## Unverified / wrong assumptions

- None material. The only inaccuracy is cosmetic: the absolute line number
  `debug_overlay_system.c:833` for the `RefreshViewportForBasisEdit` call site is approximate
  (see NIT 3); the surrounding claim (one call site inside `ApplyDebugHotkeyChanges`) is
  correct.
- The include partition for `diagnostics.c` is intentionally left as "whatever the moved code
  references" rather than a pinned list (see NIT 1); this is a delegation to the compiler, not
  a wrong assumption.
