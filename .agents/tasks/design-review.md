# Design Review: Integrating Textboxes into the Symmetric Binding System (Hybrid Model)

Reviewed document: `.agents/tasks/textbox-binding-design.md`
Scope of verification: `binding.h`, `binding.c`, `ui.h`, `ui.c`, `ui_input.c`, `panel_system.c`/`.h`, `integration_system.c`, `lpanel_system.c`, `state_manager_system.c`, `ui_system.c`, `ui_constructors.h`/`.c`, `ui_loader.c`, `lpanel.xml`, `debug_overlay_system.h`.

The review was performed fresh against source. Every "verified" claim in the design was independently re-checked.

---

## Findings

### 1. HIGH — Section 7 targets dead (commented-out) code; the lpanel edit-view stable migration does not run in the live build

The design's section 7 ("lpanel STABLE migration") is written as if `InitLPanelEditView` and its `edit_specs` block are the live construction path for the lpanel edit fields. They are not.

- `InitLPanelEditView()` is **only ever called from commented-out dead code** (`lpanel_system.c` ~lines 342-343, inside the hardcoded fallback block). The live `InitLeftPanelSystem` path loads from XML (`UILoader_LoadFromFileWithResolvers("...lpanel.xml", ...)`) and returns before the fallback (`return;` at ~line 329).
- `lpanel.xml` has the **DRAW view commented out** — only `state_view` (`LPANEL_STATE_VIEW`) is live. There is no `LPANEL_DRAW_VIEW` in the active XML, and the `<TextField ... binding="physics.*">` rows for the edit fields are all inside the commented block.
- Consequently `G_UIState.edit_width_tbox` and siblings are **never assigned** in the live path: `BuildTextField` in `ui_loader.c` resolves `data_bind` but calls `CreateUILabeledFieldDefault(...)` without ever writing the created element back to a `G_UIState.edit_*_tbox` pointer, and `InitLPanelEditView` (the only place those pointers are set) never runs. They stay NULL.
- Therefore `RefreshEntityEditorFields` in `ui_system.c` currently iterates `edit_fields[]` whose textbox pointers are NULL — it is effectively a no-op today.

The design repeatedly asserts these sites as "verified" live code to edit ("set `data_bind = NULL` on the migrated `edit_specs` rows", "remove the migrated fields' rows from `RefreshEntityEditorFields`'s `edit_fields[]`", "the only edit at the init site is changing the `selector_callback` argument ... at `lpanel_system.c` ~line 308"). The `~line 308` call site is real and live, but what it drives (the edit view and its fields) is not. An implementer following section 7 literally would edit `edit_specs`/`InitLPanelEditView` and observe no behavioural change, because that function is not invoked, and would wire `LPanel_OnViewSelected` to attach bindings for `G_UIState.edit_*_tbox` pointers that are NULL — the `BindTextboxStable(G_UIState.edit_width_tbox, ...)` calls would early-return on NULL (`BindTextboxStable` is specified as a no-op on NULL textbox), silently doing nothing.

CONCRETE FIX — choose one and state it explicitly in section 7:
- (a) **Descope lpanel from this pass.** Make section 8 (state_manager `damage`, which IS live) the sole first proof, and demote lpanel to a documented-pattern-only note ("when the DRAW view / `InitLPanelEditView` is reinstated, apply this mechanism"). This is the smallest honest change and keeps the pass additive and verifiable.
- (b) **Reinstate the live path first.** If the lpanel edit view must be proven in this pass, the design must add an explicit prerequisite step: un-comment the `draw_view` in `lpanel.xml` (or the hardcoded `InitLPanelEditView` call), and specify how `G_UIState.edit_*_tbox` get populated in the chosen path (the XML loader does not currently assign them — there is no `id`->`G_UIState` registration in `ui_loader.c`). Without this, the attach callback has no non-NULL textboxes to bind.

Either way, remove the claim that `edit_specs`/`RefreshEntityEditorFields` edits produce the described runtime effect in the current build.

### 2. HIGH — `LPanel_OnViewSelected` fires during `PanelSystem_FinaliseInit` for view index 0 (STATE), and the design's attach/clear logic depends on `G_UIState.edit_*_tbox` being initialised before that fire

`PanelSystem_FinaliseInit` calls `PanelSystem_SelectView(selector, 0)` (verified ~line 823-833), which invokes `selector->on_view_selected(selected_view)` for the **first** view. In lpanel the first view is STATE (`labels[] = {"STATE","DRAW"}`, index 0 = STATE). So the proposed `LPanel_OnViewSelected` runs during init with `selected->type == LPANEL_STATE_VIEW`, taking the `else` branch and calling `ClearTextboxBinding(G_UIState.edit_width_tbox)` etc.

In the live build (finding 1) those pointers are NULL, so `ClearTextboxBinding` must tolerate NULL. The design specifies `ClearTextboxBinding(UIElement *textbox)` but never states its NULL-handling contract. More importantly, even in a reinstated path, `PanelSystem_FinaliseInit` runs **after** the views are built but the design never pins down whether the edit textboxes exist at the moment of this first callback fire. If `on_view_selected` can fire before the edit textboxes are constructed/registered, the attach branch (DRAW active) would bind NULLs.

CONCRETE FIX — state both contracts explicitly in section 7:
- `ClearTextboxBinding(UIElement *textbox)` and `BindTextboxStable(...)` MUST be no-ops when `textbox == NULL` (the design says this for `BindTextboxStable`; say it for `ClearTextboxBinding` too).
- Add an ordering invariant: "the edit textboxes are constructed and their `G_UIState.edit_*_tbox` pointers assigned before `PanelSystem_FinaliseInit` runs, so the first `on_view_selected(STATE)` fire finds valid (clearable) pointers." If that ordering cannot be guaranteed in the chosen path, the attach/clear must be guarded accordingly.

### 3. MEDIUM — Section 7's single-writer claim ignores a live third writer: `RefreshEntityEditorFields` is still called every frame from `UpdateGlobalUIState`

The design says, after migration, "the only writers of these textboxes are (1) `InitEntityCreateDefaults`'s one-time seed and (2) the stable binding's pull" and instructs removing the migrated rows from `edit_fields[]`. But `RefreshEntityEditorFields(edit_view_active, params)` is called unconditionally every frame from `UpdateGlobalUIState` (verified `ui_system.c` ~line 547-549, `UpdateGlobalUIState` ~line 520). The design mentions possibly removing `RefreshEntityEditorFields` "along with its single call site in `UpdateGlobalUIState`" only in the fully-migrated case, and otherwise says "keep the function with only the remaining rows."

The gap: the design does not state what happens to the per-frame pull (`PanelSystem_RefreshBindings` via `UIElement_RefreshBinding`) versus `RefreshEntityEditorFields` while BOTH run for the same frame during a partial migration. For a field present in BOTH the stable binding AND a surviving `edit_fields[]` row, there would be two writers racing each frame. The design asserts "no field is driven by both paths at once" but provides no mechanism that enforces it beyond manual row removal — and if finding 1's dead-code reality holds, the removal has no live effect, masking the race only by accident.

CONCRETE FIX: state the enforced rule precisely — "a field migrated to a `binding` MUST have its `edit_fields[]` row removed in the same change; a reviewer/implementer confirms by grep that no migrated `G_UIState.edit_*_tbox` appears in both `edit_fields[]` and an attach call." And confirm the call-site reality: `RefreshEntityEditorFields` is live and per-frame today, so the removal is load-bearing (not optional) for every migrated field.

### 4. MEDIUM — `RefreshGameplaySection` disables `damage_tbox`'s parent when the selected object is not a projectile; the design's dynamic query does not account for the enable/visibility policy and the unconditional refresh walk

Section 8 migrates `damage` with a query returning the live value when an object is selected and `BIND_NONE` otherwise. But the live `damage_tbox` has a second policy layer: `RefreshGameplaySection` calls `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` (verified ~line 642), i.e. damage is only shown for projectile-type objects, not merely "something selected."

Because `PanelSystem_RefreshBindings` walks the whole tree ignoring `is_enabled` (verified — the design itself relies on this), the migrated pull will format and write `object->damage` into the textbox whenever ANY object is selected, even a non-projectile whose damage row the panel policy wants disabled/blank. `StateManager_QueryDamage` as written keys only on `UIState_GetSelectedObject() != NULL`, not on `is_projectile`. This diverges from current behaviour: today `RefreshTextboxFields` writes `object->damage` for any selected object too (the `state_fields` row is `object ? &object->damage : NULL`), BUT the field is parented-disabled for non-projectiles, so the stale/irrelevant value is not shown. Under the pull, the value is still written; whether it is drawn depends on the enabled parent. The net visible behaviour may match, but the design does not analyse this interaction and does not state the invariant.

CONCRETE FIX: state explicitly in section 8 that (a) the damage visibility policy (`is_projectile`) remains owned by `RefreshGameplaySection` via `SetParentEnabledState`, (b) the pull still writes the value into the (possibly disabled) textbox because the walk ignores `is_enabled`, and (c) this matches current behaviour because the old `RefreshTextboxFields` row also wrote unconditionally for a selected object. If instead the intent is for the query to return `BIND_NONE` for non-projectiles, encode `is_projectile` in `StateManager_QueryDamage` and say so — but then the query owns a second policy fact and the sample code must be updated.

### 5. NIT — `BindTextboxStable` signature uses `DataType`/`FLOAT`/`INT`, but the design never confirms `ResolveBindingType` is accessible from the helper's definition site

Section 7 says `BindTextboxStable` lives in `integration_system.c` and maps `DataType` -> `BindingValueType` "with the existing `ResolveBindingType`." Verified: `ResolveBindingType` is a `static` function in `integration_system.c` (`static BindingType ResolveBindingType(DataType type)` ~line 29), so a new helper in the same translation unit can call it. This is fine, but the design asserts reuse without noting the function is `static` (not exported) — correct only because the helper is co-located. 

CONCRETE FIX: add a half-sentence: "`ResolveBindingType` is `static` in `integration_system.c`; `BindTextboxStable` is placed in the same file so it can reuse it without changing linkage." Prevents an implementer from trying to call it from another TU.

### 6. NIT — Section 1 line references are accurate but one is off by one; harmless, worth correcting to avoid implementer confusion

The design cites `LPanel_QueryDebugEnabled` "at ~line 246" (verified: definition at line 246) and its assignment "at `lpanel_system.c` ~line 274" with `query_key` at "~line 275". Verified: `binding->source.query = LPanel_QueryDebugEnabled;` is at line 274 and `query_key = ...` at line 275. These are correct. No fix required beyond noting they were confirmed.

---

## Verdict inputs

HIGH findings: 2 (findings 1, 2)
MEDIUM findings: 2 (findings 3, 4)
NIT findings: 2 (findings 5, 6)

HIGH + MEDIUM = 4 > 0 => **CHANGES_REQUESTED**

---

## Required-confirmation checklist (from the task)

- Generic core stays domain-free (binding.h / binding.c / ui.c refresh / panel_system.c walk): **CONFIRMED.** The query/callback generalisation carries only `(int key) -> BindingValue` and `(int key, BindingValue) -> bool`; no selection/debug/entity concept enters the core. `ui.c`'s refresh and `panel_system.c`'s walk are domain-free in both current source and the proposed branch.
- Focused-skip invariant (`e->is_focused` never overwritten): **CONFIRMED in the design** (section 5 branch returns before any write when `e->is_focused`). Note it is only exercised on the live path for state_manager's `damage` (section 8); the lpanel proof is dead-code (finding 1).
- Existing button toggle ON/OFF display and validated commits unchanged: **CONFIRMED.** The `? 1 : 0` normalisation is behaviour-preserving under the verified `value.as.i != 0` test in `ui.c`; the button branch composes `"<text>: ON/OFF"` unchanged.
- Migration is additive (legacy Binder / RefreshTextboxFields retained while callers remain): **CONFIRMED in principle** — `HandleTextCommit` prefers `binding` then falls back to `binder`; `RefreshTextboxFields` is not deleted. Caveat: finding 3 (the live per-frame `RefreshEntityEditorFields` call) must be acknowledged so the additive story holds without a double-writer.
- No memory leaks (textbox binding freed in DisposeUIElement): **CONFIRMED.** Section 4's dispose addition mirrors the verified button-binding and textbox-binder frees; `Binding_Destroy(Binding**)` NULLs the pointer; pool zeroing guarantees NULL for untouched elements.
- Git untouched (no commits/staging/branch/worktree changes): **CONFIRMED.** The design states this explicitly in the Overview and makes no git-affecting instruction. This review likewise performs no git operations.
- Byte-identical commit guarantee (validated INT/FLOAT and STRING 255-cap identical): **CONFIRMED against source.** `Binder_ValidateAndWrite` and `Binding_Commit` run the identical `Binding_ParseText` + `Binding_WriteSink` sequence with the same `value_type`/`address`/`validator`/`ctx`; `Binding_WriteSink`'s STRING case does `strncpy(addr, s, 255); ((char*)addr)[255]='\0';` (verified byte-for-byte in `binding.c`).
- `query -> BindingValue` signature change with `LPanel_QueryDebugEnabled` shim concretely specified: **CONFIRMED concretely.** The three lockstep sites are correct (typedef in `binding.h`; `BIND_SRC_QUERY` branch in `binding.c`; single shim definition + single assignment in `lpanel_system.c`). `IsDebugEnabled` is `int IsDebugEnabled(DebugOverlayId)` (verified in `debug_overlay_system.h`), so the shim and normalisation type-check.

---

## Verified Assumptions

1. `binding.h`: `typedef int (*BindingQueryFn)(int key);`, `BIND_SRC_QUERY` hardcodes INT in `Binding_ReadSource` — **verified**, matches section 1 "current state."
2. `binding.c` `Binding_ReadSource` `BIND_SRC_QUERY` branch sets `value.as.i = src->query(src->query_key); value.type = BIND_INT;` — **verified exactly.**
3. `Binding_WriteSink` command branch never reads `value_type` (so "command sink: ignored" is accurate) — **verified.**
4. `Binding_WriteSink` STRING address case: `strncpy(addr, s, 255); addr[255]='\0';` — **verified.**
5. `Binder_ValidateAndWrite` is a thin adapter building a `BIND_SINK_ADDRESS` sink with `value_type=b->type`, `address=b->target`, `validator`/`validator_ctx` — **verified.**
6. `Binding_Commit` parses with `sink.value_type`/`sink.validator`/`sink.validator_ctx` then calls `Binding_WriteSink` — **verified.**
7. `Binding_RefreshText` returns false on `BIND_NONE` before `Binding_FormatValue`, leaving the buffer untouched — **verified.**
8. `Binding_FormatValue` STRING does a bounded `strncpy` into `out_bytes-1` with explicit NUL — **verified.**
9. `ui.c` `UIElement_RefreshBinding` has a button branch then a trailing NOTE comment; no textbox branch exists today — **verified.**
10. `ui.h` doc comment on `UIElement_RefreshBinding` ends "(Textbox read-refresh still uses the RefreshTextboxFields path.)" — **verified.**
11. `panel_system.c` `PanelSystem_RefreshBindings` recurses the whole tree with no `is_enabled` guard — **verified.**
12. `ui.c` `DisposeUIElement` frees textbox `binder` and button `binding`; no textbox `binding` free yet — **verified.**
13. `TextBoxData` currently has `text`, `data_type`, `data_bind`, `binder`, `font`, `cursor_position` (so adding `binding` is additive) — **verified.**
14. `ui_input.c` `HandleTextCommit` currently requires `element->data.textbox.binder` and calls `Binder_ValidateAndWrite`, reverting on failure — **verified.**
15. `lpanel_system.c` `LPanel_QueryDebugEnabled` (line 246) is the sole `BindingQueryFn`; assignment at line 274, `query_key` at line 275 inside `LPanel_AttachToggleSources` — **verified** (sole query confirmed by grep across the tree).
16. `IsDebugEnabled` is declared `int IsDebugEnabled(DebugOverlayId)` — **verified** in `debug_overlay_system.h`.
17. `ViewSelector` has a single `on_view_selected` callback field; populated from `PanelSystem_CreateStandard`'s `selector_callback`; lpanel passes `PanelSystem_HandleViewSelected` at ~line 308 — **verified.**
18. `PanelSystem_HandleViewSelected` sets `G_UIState.active_panel_view = view->type;` — **verified.**
19. `PanelSystem_FinaliseInit` only selects the initial view (index 0) and lays out; it neither creates the selector nor registers a callback — **verified.**
20. `UIFieldSpec` fields: `label`, `type`(UIElementType), `size`, `data_type`, `target`(UIElement**), `text_target`(String64**), `data_bind` — no validator field — **verified.**
21. `integration_system.c` `BindTextboxData` with `data_bind == NULL` calls `BindTextbox(textbox, NULL)` and attaches nothing; `ResolveBindingType` is `static` in the same file — **verified.**
22. state_manager `damage_tbox` built from a `gameplay_specs` row with `data_bind == NULL` (no legacy `Binder`); `RefreshPhysView` binds `object ? &object->damage : NULL`; `UIState_GetSelectedObject()` returns `Newtonoid2d *`; `object->damage` is a plain FLOAT — **verified.**
23. state_manager UI is built via hardcoded `InitUIFields` (not XML), so section 8's live-code premise holds — **verified.**

## Unverified / Wrong Assumptions

1. **WRONG (finding 1):** The design treats `InitLPanelEditView`/`edit_specs` as live construction code. `InitLPanelEditView` is only referenced from commented-out dead code; the live lpanel path is XML (`lpanel.xml`) whose DRAW view is commented out. The edit fields do not exist in the running build and `G_UIState.edit_*_tbox` are never populated.
2. **WRONG/UNSTATED (finding 1):** "the only edit at the init site is changing the `selector_callback` argument ... at `lpanel_system.c` ~line 308" — the call site is live, but it drives views whose edit fields are not constructed, so the described downstream effect (stable bindings refreshing physics fields) does not occur in the current build.
3. **UNVERIFIED (finding 2):** The design assumes `G_UIState.edit_*_tbox` are valid at the moment `on_view_selected` first fires during `PanelSystem_FinaliseInit`. In the live build they are NULL; in a reinstated build the construction-vs-callback ordering is not pinned down. `ClearTextboxBinding`'s NULL contract is unstated.
4. **INCOMPLETE (finding 3):** The claim "the only writers ... are the one-time seed and the stable binding's pull" omits that `RefreshEntityEditorFields` is still called every frame from `UpdateGlobalUIState`; row removal is load-bearing, not optional, and (per finding 1) currently has no live effect.
5. **INCOMPLETE (finding 4):** `StateManager_QueryDamage` keys only on object selection and ignores the live `is_projectile` visibility policy enforced by `RefreshGameplaySection` via `SetParentEnabledState`. The interaction with the unconditional refresh walk is not analysed.
