# Design Review — Textbox Binding Refactor v2 (`textbox-binding-design-v2.md`)

Reviewed fresh against the actual source on branch `ui-overhaul`. The build was NOT run and git was NOT touched, per instructions. Every load-bearing claim in the design was checked against the cited file.

## Verdict

**APPROVED** — zero HIGH, zero MEDIUM findings. All ten gate criteria pass. The remaining findings are NITs (line-number drift and one prose-only helper contract) that do not block implementation.

---

## Gate criteria results

1. **Generic core stays domain-free** — PASS. Extension A generalises `BindingQueryFn` to `(int key) -> BindingValue` (pure pass-through in the `BIND_SRC_QUERY` branch); extension B adds `BIND_SINK_CALLBACK` with `(int key, BindingValue) -> bool`. Neither carries a selection/debug/entity/visibility concept. The new textbox branch in `UIElement_RefreshBinding` only NULL-checks, focused-skips, and formats. All domain policy (which address now, clear-when-none, is_projectile visibility) lives in `rpanel_system.c` / `state_manager_system.c`. Verified the core files (`binding.h`, `binding.c`, `UIElement_RefreshBinding` in `ui.c`, `PanelSystem_RefreshBindings` in `panel_system.c`) contain no such concepts today and the extensions do not add any.

2. **Focused-skip invariant preserved** — PASS. Section 5's textbox branch returns on `e->is_focused` before any `Binding_RefreshText` write, matching the invariant currently enforced inside `RefreshTextboxFields` (`integration_system.c` line 222: `if (field->textbox->is_focused)`).

3. **Button toggle ON/OFF unchanged; validated commits byte-identical** — PASS. `UIElement_RefreshBinding`'s button branch (`ui.c` ~line 1120) computes `bool on = (value.as.i != 0)` and composes `"%s: %s"` ON/OFF from `button.text` into `button.display_text`. The new shim's `? 1 : 0` normalisation cannot change the `!= 0` outcome, so presentation is byte-identical. Byte-identity of commits is provable: `Binder_ValidateAndWrite` (binding.c) builds a `BIND_SINK_ADDRESS` sink and runs `Binding_ParseText` + `Binding_WriteSink`; `Binding_Commit` runs the identical sequence from `b->sink`. The STRING branch of `Binding_WriteSink` keeps the historic `strncpy(addr, s, 255); addr[255] = '\0';` 255-cap verbatim (verified).

4. **Additive migration** — PASS. `TextBoxData` keeps `data_bind` and `binder`; the new `Binding *binding` is added alongside (verified current fields: `text`, `data_type`, `data_bind`, `binder`, `font`, `cursor_position`). `HandleTextCommit`'s rewrite prefers `binding`, else falls back to `binder`, else reverts — the neither-carrier branch matches the old binder-missing revert.

5. **No leaks** — PASS. `DisposeUIElement` (`ui.c`) already frees the textbox `binder` (~lines 1063-1067) and the button `binding` (~lines 1071-1074); section 4 inserts the textbox `binding` free between them, mirroring both. `Binding_Destroy(Binding **)` NULLs the caller pointer (verified).

6. **Git untouched** — PASS. The design states explicitly, twice, that nothing is committed/staged/branched and no worktree is created; work is in-place on `ui-overhaul`.

7. **Every migrated field is a LIVE code path** — PASS. The STABLE proof is rpanel `rpanel_create_gravity_tbox`, a live `create_fields` row backed by `GetNextWorldGravityPtr()` and refreshed unconditionally every frame by `DrawRPanel`'s `RefreshTextboxFields(create_fields, ...)` (verified at `rpanel_system.c` line 226 — not view-gated). The DYNAMIC proof is state_manager `s_sm_ui.damage_tbox`, a live `state_fields` row (`state_manager_system.c` line 779) refreshed by `RefreshPhysView`. lpanel edit-view migration is explicitly OUT OF SCOPE (section 7); confirmed dead code — `InitLPanelEditView` is only called from a commented-out fallback (`lpanel_system.c` lines 342-343) and the live `InitLPanel` XML path returns before it.

8. **Legacy writer removed in lockstep (no double-writer)** — PASS. For gravity: the design removes the per-frame `TextboxField create_fields[]` gravity row AND NULLs the construction-spec `data_bind` so `InitUIFields` (`ui_constructors.c` lines 389-392: `if (specs[i].data_bind) BindTextboxData(...)`) builds no legacy `Binder`. This closes the second-carrier gap correctly: without the NULL, the construction spec's non-NULL `data_bind` (`GetNextWorldGravityPtr()`) would build a binder on the gravity textbox at construction, leaving two carriers. For damage: the construction `gameplay_specs` row already carries `data_bind == NULL` (verified line 484), so only the per-frame `state_fields[]` row must be removed — which also removes the sole site (`RefreshTextboxFields` -> `BindTextboxData` at `integration_system.c` line 219) that builds the dynamic binder today. The design's reasoning on this distinction is accurate.

9. **query->BindingValue signature change enumerates three lockstep edits + 0/1 shim** — PASS. The three sites are correctly identified and complete: (a) typedef in `binding.h` line 91; (b) the `BIND_SRC_QUERY` branch in `binding.c`; (c) the sole shim `LPanel_QueryDebugEnabled` plus its assignment in `lpanel_system.c`. Verified by grep that `LPanel_QueryDebugEnabled` is the ONLY `BindingQueryFn` in the tree and the `BIND_SRC_QUERY` branch is the only consumer. The shim's `IsDebugEnabled(...) ? 1 : 0` normalisation is correctly described as behaviour-preserving.

10. **is_projectile visibility decided and STATED with matching code** — PASS. Section 9 chooses option (a): visibility stays in `RefreshGameplaySection` via the unchanged `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` (verified ~line 641); the pull writes `object->damage` for any selected object. This matches today's behaviour exactly — the live `state_fields` row writes `object ? &object->damage : NULL` for any selected object regardless of role (verified line 779), and the parent-disable merely hides it for non-projectiles. The sample `StateManager_QueryDamage` keys only on selection (not on `is_projectile`), matching the decision.

---

## Findings

1. **NIT — Residual line-number drift across several citations.** The design (as a v2 revision) claims line references were re-verified, but a few are still off by one to two lines:
   - `LPanel_QueryDebugEnabled` definition: design says line 246, actual `static int LPanel_QueryDebugEnabled` is at line 247.
   - `query_key` assignment: design says line 276, actual is line 275.
   - rpanel construction `UIFieldSpec create_fields[]` gravity row: design says line 124, actual is line 126 (line 124 is the "Objects" row).
   - `InitUIFields` `data_bind` block: design says lines 388-392, actual is 389-392.
   - state_manager `gameplay_specs` damage row: design says ~483, actual is line 484.
   - `DisposeUIElement` binder/binding frees: design says 1064-1069 / 1072-1076, actual ~1063-1067 / 1071-1074.
   
   None of these obstruct the edit because every cited row is also quoted verbatim and the anchor text matches source exactly, so each edit remains unambiguous. CONCRETE FIX: either (a) drop the exact line numbers and rely on the verbatim anchors (preferred, since the file is on an active branch and will drift), or (b) re-sync the numbers one final time. This does not affect correctness; severity NIT.

2. **NIT — `BindTextboxStable` / `BindTextboxDynamic` "update-in-place if one already exists" is prose-only and must not leak.** Section 8 says the stable helper "stores it in `textbox->data.textbox.binding` (updating in place if one already exists, mirroring `BindTextboxData`'s binder reuse)." But `Binding_Create` (verified) always `AllocateBytes` a fresh heap `Binding`; if the helper calls `Binding_Create` while `textbox->data.textbox.binding != NULL` without first destroying the old one, it leaks the previous allocation. Both live proofs attach exactly once, so this never triggers in this pass, but the stated reuse contract is a trap for a future second-attach. CONCRETE FIX: specify the helper body as — if `textbox->data.textbox.binding` is non-NULL, overwrite its fields in place (`*textbox->data.textbox.binding = built;`) rather than calling `Binding_Create` again; only `Binding_Create` when the slot is NULL. State this explicitly so the implementer does not `Binding_Create` over a live pointer. Severity NIT (no live path hits it).

---

## Verified assumptions

- `binding.h` line 91 `typedef int (*BindingQueryFn)(int key);` — the sole typedef. VERIFIED.
- `binding.c` `BIND_SRC_QUERY` branch hardcodes `value.as.i = src->query(...); value.type = BIND_INT;` — the sole branch, exactly as quoted. VERIFIED.
- `LPanel_QueryDebugEnabled` is the ONLY `BindingQueryFn` assigned anywhere; assignment + `query_key` in `LPanel_AttachToggleSources` only. VERIFIED by grep.
- `IsDebugEnabled` result consumed only via `value.as.i != 0` in the button branch — `? 1 : 0` is behaviour-preserving. VERIFIED.
- `BindingSink` currently has `kind`, `value_type`, `address`, `validator`, `validator_ctx`, `command`, `command_code` — the two new callback fields are additive. VERIFIED.
- `Binding_WriteSink` command branch never reads `value_type` (so "command sink: ignored" stays accurate). VERIFIED.
- `Binder_ValidateAndWrite` and `Binding_Commit` run the identical parse+write sequence; STRING 255-cap preserved verbatim in `Binding_WriteSink`. VERIFIED.
- `Binding_RefreshText` returns false on `BIND_NONE` without touching the output buffer (clear-when-none mechanism). VERIFIED.
- `TextBoxData` fields are `text`, `data_type`, `data_bind`, `binder`, `font`, `cursor_position` — no existing `binding`. VERIFIED.
- `DisposeUIElement` frees textbox `binder` and button `binding`; `Binding_Destroy` NULLs the pointer; pool-zeroing defaults untouched slots to NULL. VERIFIED.
- `UIElement_RefreshBinding` is button-only today with the stale NOTE at the tail; `ui.h` doc comment ends "(Textbox read-refresh still uses the RefreshTextboxFields path.)". VERIFIED.
- `PanelSystem_RefreshBindings` recurses `panel->root` via `ForEachChild` with NO `is_enabled` guard (load-bearing for sections 8/9). VERIFIED (panel_system.c ~line 661).
- `UIFieldSpec` has NO validator field (`label`, `type`, `size`, `data_type`, `target`, `text_target`, `data_bind`). VERIFIED.
- `BindTextboxData` builds its binder with `validator = NULL` (so migrated rpanel create fields are validator-free; address-sink commit is byte-identical). VERIFIED.
- `InitUIFields` builds a legacy `Binder` from any spec row with non-NULL `data_bind` (`ui_constructors.c`). VERIFIED — the exact reason the construction-spec `data_bind` must be NULLed for gravity.
- rpanel construction gravity row currently passes `GetNextWorldGravityPtr()` as `data_bind` (non-NULL). VERIFIED — the second-carrier risk is real and the design's NULLing fix is necessary and correct.
- rpanel per-frame `TextboxField create_fields[]` in `DrawRPanel` runs unconditionally (not view-gated), so migrating gravity to the always-walked pull preserves "refreshed every frame regardless of view". VERIFIED.
- state_manager `damage_tbox` `gameplay_specs` row has `data_bind == NULL`; `state_fields` damage row is `object ? &object->damage : NULL` for any selected object; `SetParentEnabledState(damage_tbox, is_projectile)` present and unchanged. VERIFIED — option (a) matches current behaviour exactly.
- `UIState_GetSelectedObject()` returns `Newtonoid2d *` and validates before exposing; the `object->id == INVALID_ENTITY_ID` guard pattern used by the proposed query/callback matches existing code (e.g. `HandleComponentToggleClick`). VERIFIED.
- `ResolveBindingType` is `static` in `integration_system.c` (co-location rationale for the helpers is sound). VERIFIED.
- `InitLPanelEditView` is dead (only a commented-out fallback calls it); lpanel out-of-scope is justified. VERIFIED.
- `HandleTextCommit` current structure (outer IsTextbox guard, then `!IsEditableTextbox || !binder` revert, then `Binder_ValidateAndWrite`) matches the design's section 6 current-state snippet; the proposed rewrite preserves revert-on-failure in every branch. VERIFIED.

## Unverified / wrong assumptions

- None material. The only inaccuracies found are the minor line-number offsets in Finding 1; every quoted anchor text matches source verbatim, so no claim is substantively wrong. No design assertion about behaviour, data flow, or structure was contradicted by the source.
