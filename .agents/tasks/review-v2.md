# Textbox binding brought onto the symmetric core (HYBRID, v2)

The uncommitted working tree on `ui-overhaul` migrates textboxes onto the same binding abstraction that already drives buttons, so a single `Binding *binding` on `TextBoxData` carries both the per-frame display read (source) and the commit write (sink). The change extends the generic core in two symmetric ways — the query source now returns a full `BindingValue` instead of an INT, and a new `BIND_SINK_CALLBACK` mirrors the query source on the write side — then proves the two field shapes on live fields: a STABLE fixed-address field (rpanel gravity) and a DYNAMIC selection-driven field (state_manager damage). Migration is additive: unmigrated fields keep their legacy `binder`/`data_bind` path. The whole thing hinges on the generic core staying domain-free, and it does — all selection/debug/visibility policy lives in the panels.

Watch for: nothing blocking. The single-writer invariant holds for both migrated fields at both the per-frame array and the construction spec (confirmed by grep). The INT normalisation on the debug shim preserves the toggle display (confirmed). One informational note on the dynamic field's write-but-not-draw behaviour, which the design deliberately chose and documents.

**Verdict**: APPROVED

## High-level view

The core gained exactly two concepts, both type-level and both symmetric. `BindingQueryFn` went from `int (*)(int)` to `BindingValue (*)(int)` so a query source can be any type — the FLOAT damage read depends on this — and the `BIND_SRC_QUERY` branch became a straight pass-through, so the core never interprets the query's type. `BIND_SINK_CALLBACK` is the write-side mirror: a panel-supplied `write(key, value)` that returns true/false. Neither extension teaches the core anything about selection, debug, or visibility.

Domain policy stayed in the panels. The debug toggle shim normalises `IsDebugEnabled` to a canonical 0/1 INT so the existing `value.as.i != 0` test and ON/OFF composition are unchanged. The damage field's query and callback — selection lookup, clear-when-none, reject-when-unselected — live entirely in `state_manager_system.c`.

The carrier and both directions are wired end to end. `TextBoxData` gained `Binding *binding`, freed in `DisposeUIElement` alongside the existing binder and button-binding frees. `UIElement_RefreshBinding` gained a textbox branch that early-returns on no binding / `BIND_SRC_NONE`, early-returns while focused (preserving the invariant that used to live in `RefreshTextboxFields`), else pulls. `HandleTextCommit` prefers `binding` via `Binding_Commit`, falls back to the legacy `binder`, else reverts — so unmigrated fields are untouched.

Single-writer is enforced at both sites per migrated field. The rpanel gravity row lost its per-frame refresh row AND had its construction-spec `data_bind` NULLed so `InitUIFields` builds no second carrier; the damage row lost its `RefreshPhysView` refresh row and its construction spec already carried no `data_bind`. The lpanel edit-view (dead code) was correctly left untouched. Git is untouched.

<details>
<summary>Issues (1)</summary>

1. **Write-but-not-draw on non-projectile damage (informational, confirmed)** — the refresh walk ignores `is_enabled`, so the damage pull writes `object->damage` for any selected object even when `SetParentEnabledState(..., is_projectile)` has disabled the row. This is the design's explicit option (a) and matches pre-change behaviour; no action required.

</details>

<details>
<summary>Details</summary>

### Query source generalised to a full BindingValue

The typedef change (`binding.h`) from `int (*BindingQueryFn)(int)` to `BindingValue (*BindingQueryFn)(int)` and the `Binding_ReadSource` edit from the INT-stamping body to `return src->query(src->query_key);` move together with the sole consumer, `LPanel_QueryDebugEnabled` (confirmed: grep finds exactly one `BindingQueryFn` consumer and one `BIND_SRC_QUERY` producer branch in `binding.c`). Because all three sites moved in lockstep, a stale INT-returning shim would have failed the link — the recorded exit-0 build is evidence they did. The shim now returns `(BindingValue){ .type = BIND_INT, .as.i = IsDebugEnabled(...) ? 1 : 0 }`, and its source assignment still sets `value_type = BIND_INT` (confirmed at `lpanel_system.c` line 275). The button branch tests `value.as.i != 0`, so collapsing any non-zero to 1 leaves the ON/OFF composition byte-identical (confirmed).

### Symmetric callback sink

`BIND_SINK_CALLBACK` is added as the last `BindingSinkKind`, with `BindingSinkFn` (`bool (*)(int, BindingValue)`) and the `write`/`write_key` fields mirroring the source's `query`/`query_key`. `Binding_WriteSink` gained one branch: NULL `write` rejects (caller reverts), else returns `write(write_key, value)`. `Binding_Commit` is unchanged, so parse+validate run identically for a callback sink and only the final store differs — this is what makes the dynamic path's parse/validate identical to an address sink.

### Carrier, read branch, and the two corrected doc comments

`TextBoxData` carries `Binding *binding` alongside the retained `data_bind`/`binder`, and `DisposeUIElement` frees it between the textbox-binder free and the button-binding free, so a migrated textbox does not leak. The `UIElement_RefreshBinding` textbox branch returns on `!b || source.kind == BIND_SRC_NONE`, returns on `e->is_focused` before any write, then calls `Binding_RefreshText` — the focused-skip invariant that previously lived in `RefreshTextboxFields` is preserved at the new write site. Both stale doc comments were corrected in lockstep: the inline `NOTE` tail in `ui.c` (was "textboxes do not yet carry a Binding source … future work") and the `ui.h` declaration comment (was "(Textbox read-refresh still uses the RefreshTextboxFields path.)") now describe the pull-with-focused-skip behaviour.

### Commit path: prefer binding, fall back to binder

`HandleTextCommit` keeps the outer non-textbox revert guard and the `IsEditableTextbox` guard, then branches: `binding` → `Binding_Commit` (revert on false), else `binder` → `Binder_ValidateAndWrite` (revert on false), else revert (matching the old binder-missing path), falling through to `ResetTextBuffers` on success (confirmed by reading `ui_input.c` lines 498-540). For the migrated gravity field — address sink, no validator — the design's byte-identity claim holds because `Binder_ValidateAndWrite` and `Binding_Commit` run the same parse+`Binding_WriteSink` sequence; the STRING 255-cap lives in the shared `Binding_WriteSink` and is unchanged. Unmigrated fields take the fallback branch, so behaviour there is unchanged.

### Live STABLE proof and single-writer (rpanel gravity)

Three edits landed together (confirmed by grep in `rpanel_system.c`): the construction `UIFieldSpec` Gravity row's `data_bind` is now `NULL` (line 126) so `InitUIFields` builds no legacy `Binder`; `BindTextboxStable(rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2)` is attached right after `InitUIFields` (line 138); and the per-frame `TextboxField create_fields[]` gravity row is commented out (line 230). The gravity textbox therefore appears in an attach but in no surviving `*_fields[]` row and no non-NULL construction `data_bind` — the sole carrier is the binding. The remaining create fields (spawn, resolution, basis u/v, objects) stay on the legacy per-frame path.

### Live DYNAMIC proof and the is_projectile decision (state_manager damage)

`StateManager_QueryDamage` returns `BIND_NONE` when nothing is selected (or id is `INVALID_ENTITY_ID`), else `{ BIND_FLOAT, object->damage }`; `StateManager_WriteDamage` rejects when nothing selected or the value is not FLOAT, else stores and returns true. Both are static in `state_manager_system.c`, so the selection/clear-when-none/reject policy is panel-owned. `BindTextboxDynamic(s_sm_ui.damage_tbox, BIND_FLOAT, 2, …, 0)` is attached after `InitUIFields` (line 524), the `gameplay_specs` Damage row carries no `data_bind` (line 516), and the `RefreshPhysView` `state_fields[]` damage row is commented out (line 817) — sole carrier is the binding, so post-migration `binder == NULL` and commit flows through the callback sink.

The is_projectile visibility call `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` is unchanged (line 680) — design option (a). Because `PanelSystem_RefreshBindings` walks the tree with no `is_enabled` guard, the pull writes `object->damage` for any selected object even a non-projectile whose row is parent-disabled; the value is written but not drawn. This matches the pre-change behaviour (the old `state_fields[]` row also wrote for any selected object) and is the design's explicit, documented choice, not a regression.

### lpanel edit-view untouched

The diff touches `lpanel_system.c` only for the `LPanel_QueryDebugEnabled` shim signature. No edits to `edit_specs`, `InitLPanelEditView`, or `RefreshEntityEditorFields` (confirmed — the dead-code edit view was correctly treated as a pattern note only, per plan step 7).

### Build and git

`verification-v2.md` records three configure+build cycles (after core extensions, after carrier/commit edits, after live migrations), all exit 0, `raylib-game.exe` linked, no `Permission denied`, exe not run. Per the task constraint this build was not re-run. The note also records git left unmutated; the working-tree diff against `ui-overhaul` HEAD confirms the edits are uncommitted and no branch/worktree change is present.

</details>

<details>
<summary>File map</summary>

- `include/system/systems.h` — includes `ui/binding.h`; declares `BindTextboxStable`/`BindTextboxDynamic`/`ClearTextboxBinding`.
- `include/ui/binding.h` — `BindingQueryFn` returns `BindingValue`; adds `BIND_SINK_CALLBACK`, `BindingSinkFn`, sink `write`/`write_key`; comment updates.
- `include/ui/ui.h` — adds `Binding *binding` to `TextBoxData`; corrects `UIElement_RefreshBinding` doc.
- `src/engine/system/integration_system.c` — defines the three attach/clear helpers next to `BindTextboxData`.
- `src/engine/system/ui/lpanel_system.c` — debug query shim returns a normalised `BindingValue` INT.
- `src/engine/system/ui/rpanel_system.c` — gravity migrated to `BindTextboxStable`; construction `data_bind` NULLed; per-frame row removed.
- `src/engine/system/ui/state_manager_system.c` — damage query+callback; `BindTextboxDynamic` attach; per-frame row removed; is_projectile call unchanged.
- `src/engine/ui/binding.c` — `BIND_SRC_QUERY` pass-through; `BIND_SINK_CALLBACK` branch.
- `src/engine/ui/ui.c` — textbox branch in `UIElement_RefreshBinding`; textbox binding free in `DisposeUIElement`; NOTE rewritten.
- `src/engine/ui/ui_input.c` — `HandleTextCommit` prefers binding, falls back to binder, else reverts.

Full diff: `git diff ui-overhaul`.

</details>
