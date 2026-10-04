# Implementation Plan — Textbox Binding Refactor (HYBRID, v2)

Derived from the APPROVED design `.agents/tasks/textbox-binding-design-v2.md` (review `.agents/tasks/design-review-v2.md`). This plan sequences that design into ordered, independently-verifiable edits. It does not re-decide the architecture.

## HARD CONSTRAINTS (apply to EVERY step below)

- Work IN-PLACE in the main worktree at `c:\Projects\raylib-testing` on branch `ui-overhaul`. DO NOT create a worktree.
- DO NOT commit, stage, rebase, or alter git state in ANY way. Leave all edits uncommitted. The user commits themselves.
- Build ONLY with, from the repo root (UCRT64 + Ninja):
  ```powershell
  cmake --preset debug
  cmake --build --preset debug
  ```
- The `ninja: warning: premature end of file; recovering` prefix is NOT an error — ignore it.
- Success = `raylib-game.exe` links and the build exits with code 0.
- If every `.c` compiled and ONLY the final link failed on `Permission denied`, the `.exe` is locked by a running game: treat compile as PASSING, report that the user must close the game, DO NOT kill the process, DO NOT run the `.exe`.
- Per AGENTS.md: UK English in comments (colour, behaviour, unrecognised); comment the logic of each new function; match existing style.
- Verification note: the build is a WHOLE-PROJECT link (one target `raylib-game.exe`), so every step's build verification is the same two commands. Steps are ordered so the tree stays buildable after each one; run the build at the step boundaries indicated. Grep checks noted below are review aids, NOT a substitute for the build.

## Approach notes (decisions already fixed by the approved design, restated so the implementer does not re-derive them)

- The generic binding core (`binding.h`, `binding.c`, `UIElement_RefreshBinding` in `ui.c`, `PanelSystem_RefreshBindings` in `panel_system.c`) carries NO domain knowledge. All selection / clear-when-none / visibility policy lives in `rpanel_system.c` and `state_manager_system.c`.
- Migration is ADDITIVE: `data_bind`, `binder`, and the new `binding` coexist on `TextBoxData`; unmigrated fields keep the legacy path.
- STABLE fields (fixed address) use `BIND_SRC_ADDRESS` + `BIND_SINK_ADDRESS`. DYNAMIC/selection fields use a panel-supplied query (read) + callback (write).
- SINGLE-WRITER rule: a field migrated to a `binding` MUST, in the SAME step, lose its per-frame `RefreshTextboxFields` `*_fields[]` row AND (if its construction `UIFieldSpec` row carried a non-NULL `data_bind`) have that `data_bind` NULLed so `InitUIFields` builds no legacy `Binder`.
- lpanel edit-view is OUT OF SCOPE (dead code) — documentation pattern note only, no runtime edits.

Verified anchor drift (use the quoted verbatim anchor text to locate each edit; line numbers are approximate): rpanel gravity construction row is at ~line 125 (NOT 124 — line 124 is the "Objects" row); lpanel shim `static int LPanel_QueryDebugEnabled` is at ~line 246-247; `query_key` assignment ~line 275; state_manager `gameplay_specs` Damage row ~line 483, `state_fields[]` damage row ~line 779 inside `RefreshPhysView`, `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` ~line 642.

Header-ordering fact (load-bearing for step 4): `include/ui/ui.h` includes `system/systems.h` BEFORE `ui/binding.h`. `systems.h` does NOT currently include `binding.h`. So any new helper declaration in `systems.h` that names `BindingValueType` / `BindingQueryFn` / `BindingSinkFn` requires `systems.h` to `#include "ui/binding.h"` itself (binding.h is include-guarded — safe).

---

# Implementation Plan

- [ ] 1. Generic core extension A — generalise the query source to return a `BindingValue` (three lockstep sites).
      What: (a) In `binding.h`, change the typedef `typedef int (*BindingQueryFn)(int key);` to `typedef BindingValue (*BindingQueryFn)(int key);`, and update the `BindingSource.value_type` comment from `// how to interpret the address / query result` to note it is advisory for a query source (the query owns the returned type) and meaningful for an address source. Note `BindingValue` is declared ABOVE `BindingQueryFn` in the header, so the typedef compiles. (b) In `binding.c`, replace the `BIND_SRC_QUERY` branch body of `Binding_ReadSource` (currently `value.as.i = src->query(src->query_key); value.type = BIND_INT; return value;`) with a straight pass-through `return src->query(src->query_key);` (keep the `if (!src->query) return value;` NULL guard). (c) In `lpanel_system.c`, change the shim `static int LPanel_QueryDebugEnabled(int overlay_key)` to `static BindingValue LPanel_QueryDebugEnabled(int overlay_key)` returning `(BindingValue){ .type = BIND_INT, .as.i = IsDebugEnabled((DebugOverlayId)overlay_key) ? 1 : 0 };` and refresh its doc comment. Leave the `binding->source.query = LPanel_QueryDebugEnabled;` and `query_key` assignment lines textually unchanged.
      Why: INT-only query blocks the FLOAT dynamic damage path (step 8). The `? 1 : 0` normalisation is behaviour-preserving — the button branch tests `value.as.i != 0`.
      Files: `include/ui/binding.h`, `src/engine/ui/binding.c`, `src/engine/system/ui/lpanel_system.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, `raylib-game.exe` links. (All three sites move together; a stale `int`-returning shim would fail the link.) Review-grep: confirm `LPanel_QueryDebugEnabled` is still the ONLY `BindingQueryFn` in the tree and the `BIND_SRC_QUERY` branch is its only consumer.

- [ ] 2. Generic core extension B — add the symmetric `BIND_SINK_CALLBACK` sink.
      What: In `binding.h`: add `BIND_SINK_CALLBACK` as the last enumerator of `BindingSinkKind`; add the typedef `typedef bool (*BindingSinkFn)(int key, BindingValue value);` next to `BindingCommandFn`; add two fields to `BindingSink` — `BindingSinkFn write;` and `int write_key;` — and append `callback sink: parse type` to the existing `value_type` comment (keep `command sink: ignored` verbatim). In `binding.c`: add one `case BIND_SINK_CALLBACK:` branch to `Binding_WriteSink` — `if (!sink->write) return false; return sink->write(sink->write_key, value);`. Leave `Binding_Commit` unchanged (it already parses with `sink.value_type`/`validator`/`validator_ctx` then calls `Binding_WriteSink`).
      Why: the write-side mirror of the query source; lets a dynamic field (damage) commit without the core knowing about selection. Additive — no existing sink kind changes.
      Files: `include/ui/binding.h`, `src/engine/ui/binding.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links.

- [ ] 3. Add the `Binding *binding` field to `TextBoxData` and free it in `DisposeUIElement`.
      What: In `include/ui/ui.h`, add `Binding *binding;` to the `TextBoxData` struct (after `Binder *binder;`), with a comment noting it is the single bidirectional carrier (source = read, sink = write) and that it coexists with the legacy `data_bind`/`binder`. In `src/engine/ui/ui.c`, inside `DisposeUIElement`, between the existing textbox `binder` free and the button `binding` free, add: `if (IsTextbox(e) && e->data.textbox.binding) { Binding_Destroy(&e->data.textbox.binding); }` with a comment mirroring the adjacent frees (`Binding_Destroy` NULLs the pointer; pool-zeroing defaults untouched slots to NULL so this is a no-op there).
      Why: `binding` is the carrier used by steps 5, 6, 7, 8; the dispose free prevents a leak on migrated textboxes.
      Files: `include/ui/ui.h`, `src/engine/ui/ui.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. (No behavioural change yet; every `binding` is NULL until a later step attaches one.)

- [ ] 4. Add the three binding-helper declarations and their definitions in `integration_system.c`.
      What: Declare in `include/system/systems.h` (and add `#include "ui/binding.h"` near the top of `systems.h` so `BindingValueType`/`BindingQueryFn`/`BindingSinkFn` are visible there — see the header-ordering note above):
      - `void BindTextboxStable(UIElement *textbox, DataType type, void *address, int precision);`
      - `void BindTextboxDynamic(UIElement *textbox, BindingValueType type, int precision, BindingQueryFn query, BindingSinkFn write, int key);`
      - `void ClearTextboxBinding(UIElement *textbox);`
      Define all three in `src/engine/system/integration_system.c`, placed next to `BindTextboxData` so they can reuse the existing `static BindingType ResolveBindingType(DataType)` without changing its linkage.
      - `BindTextboxStable`: no-op on NULL textbox; map `type` via `ResolveBindingType` and no-op if it resolves to `BINDING_NONE`; build a `Binding` with `source.kind = BIND_SRC_ADDRESS`, `sink.kind = BIND_SINK_ADDRESS`, both `value_type` from the map, both `address` the same pointer, `precision` as given, no validator. Store into `textbox->data.textbox.binding`: if the slot is already non-NULL, overwrite in place (`*textbox->data.textbox.binding = built;`) — do NOT call `Binding_Create` over a live pointer (that leaks); only call `Binding_Create(built)` when the slot is NULL. (This closes design-review-v2 NIT 2.)
      - `BindTextboxDynamic`: no-op on NULL textbox; build `source = { .kind = BIND_SRC_QUERY, .value_type = type, .query = query, .query_key = key }`, `sink = { .kind = BIND_SINK_CALLBACK, .value_type = type, .write = write, .write_key = key }`, `precision`; store with the same create-or-overwrite-in-place rule as above.
      - `ClearTextboxBinding`: no-op on NULL textbox or NULL binding; otherwise `Binding_Destroy(&textbox->data.textbox.binding)`.
      Why: these are the attach helpers used by steps 8 and 9; `ClearTextboxBinding` is for completeness (neither live proof needs detach, both attach once).
      Files: `include/system/systems.h`, `src/engine/system/integration_system.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. (Helpers compile and are callable; not yet called.)

- [ ] 5. Add the textbox read branch to `UIElement_RefreshBinding` and update BOTH stale doc comments in lockstep.
      What: In `src/engine/ui/ui.c`, after the existing button branch of `UIElement_RefreshBinding` and BEFORE the tail `NOTE`, add a textbox branch: `if (IsTextbox(e)) { const Binding *b = e->data.textbox.binding; if (!b || b->source.kind == BIND_SRC_NONE) return; if (e->is_focused) return; Binding_RefreshText(b, e->data.textbox.text.string, sizeof(e->data.textbox.text.string)); return; }` with comments covering: unbound/source-less textboxes stay on the legacy `RefreshTextboxFields` path; the focused-skip invariant (never overwrite text the user is editing) that used to live in `RefreshTextboxFields`; and that a `BIND_NONE` read leaves the last-good display untouched. Then REPLACE the stale tail `NOTE` (currently "textbox read-refresh still flows through ... Textboxes do not yet carry a Binding source ... deliberate future work") with a note that migrated textboxes refresh here via the pull (skipped while focused) while unmigrated ones remain on the explicit `RefreshTextboxFields` path. In `include/ui/ui.h`, update the `UIElement_RefreshBinding` declaration doc comment: replace the trailing `(Textbox read-refresh still uses the RefreshTextboxFields path.)` with wording that a textbox carrying a readable binding source refreshes via this pull (skipped while focused), and textboxes without a binding stay on `RefreshTextboxFields`.
      Why: this is the per-frame display read for migrated textboxes, reached every frame by `PanelSystem_RefreshBindings` (which has no `is_enabled` guard — load-bearing for steps 8/9). Both doc edits move together to avoid doc-rot.
      Files: `src/engine/ui/ui.c`, `include/ui/ui.h`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. (No migrated textbox exists yet, so the new branch is reached only with `binding == NULL` → early return; behaviour unchanged.)

- [ ] 6. Migrate the commit path in `HandleTextCommit` to prefer `binding`, else fall back to `binder`, with byte-identical revert.
      What: In `src/engine/ui/ui_input.c`, `HandleTextCommit`: keep the outer `if (!element || !IsTextbox(element))` revert guard unchanged. Replace the `if (!IsEditableTextbox(element) || !element->data.textbox.binder) { revert; return; }` + `if (!Binder_ValidateAndWrite(...)) { revert; return; }` block with: an `if (!IsEditableTextbox(element)) { RevertTextChanges; return; }` guard; then `const char *text = element->data.textbox.text.string;`; then — if `element->data.textbox.binding` is non-NULL, commit via `Binding_Commit(element->data.textbox.binding, text)` (revert on false); else if `element->data.textbox.binder` is non-NULL, commit via `Binder_ValidateAndWrite(element->data.textbox.binder, text)` (revert on false); else revert and return (matches the old binder-missing revert). On success fall through to the existing `ResetTextBuffers(tbox_buffers);`. Comment each branch.
      Why: migrated fields commit through the symmetric core; unmigrated keep the legacy binder. For a stable address-sink field with no validator the committed bytes are byte-identical (design section 6: `Binder_ValidateAndWrite` and `Binding_Commit` run the identical parse+write).
      Files: `src/engine/ui/ui_input.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. (Still no migrated textbox; every textbox takes the `binder` fallback branch — behaviour unchanged.)

- [ ] 7. (Documentation only) Record the lpanel edit-view pattern note — NO code change.
      What: lpanel edit-view migration is OUT OF SCOPE (dead code: `InitLPanelEditView` only called from a commented-out fallback; `lpanel.xml` DRAW view commented out; `G_UIState.edit_*_tbox` never populated in the live XML path). Make NO edits to `edit_specs`, `InitLPanelEditView`, or `RefreshEntityEditorFields`. The pattern to apply if it is ever reinstated — build a fixed-address `BindTextboxStable`-style binding per field at a view-activation hook and remove the matching `RefreshEntityEditorFields`/`edit_fields[]` rows in lockstep — is captured in design section 7. This item creates no obligation and no file change; it exists so the implementer explicitly does NOT touch lpanel expecting a runtime effect.
      Files: none.
      Verify: N/A (no edit). Confirm by inspection that lpanel edit-view code was not modified.

- [ ] 8. LIVE STABLE proof — migrate rpanel `rpanel_create_gravity_tbox` to `BindTextboxStable`, with single-writer removal AND construction-spec `data_bind` NULLed, all in lockstep.
      What: In `src/engine/system/ui/rpanel_system.c`, three edits that MUST land together:
      (a) In the construction `const UIFieldSpec create_fields[]` array, NULL the `data_bind` on the Gravity row — change `{"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &rpanel_create_gravity_tbox, NULL, GetNextWorldGravityPtr()}` to `... &rpanel_create_gravity_tbox, NULL, NULL}` (keep the `&rpanel_create_gravity_tbox` target pointer; add a comment: migrated to BindTextboxStable, data_bind NULLed so InitUIFields builds no legacy Binder). This stops `InitUIFields` (`ui_constructors.c`, builds a `Binder` from any non-NULL `data_bind`) from creating a second carrier.
      (b) Immediately after the `InitUIFields(create_world_cont, create_fields, ARRAY_COUNT(create_fields), ...)` call, add: `BindTextboxStable(rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2);` with a comment that the next-world gravity param is a fixed address (STABLE proof) and this replaces the per-frame refresh row removed below.
      (c) In the per-frame `TextboxField create_fields[]` array (inside the draw/refresh path), remove the gravity row `{rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2, NULL},` (comment it out with `// MIGRATED to BindTextboxStable`). The remaining rows (spawn, resolution, basis u, basis v, objects) stay on the legacy path.
      Why: proves the STABLE path on a live field. Single-writer: with the per-frame row gone and the construction `data_bind` NULLed, `BindTextboxStable` is the sole carrier. Commit is byte-identical (no validator; address sink). (Addresses design-review-v2 Finding 1 and v1 Finding 3.)
      Files: `src/engine/system/ui/rpanel_system.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. Review-grep (two parts): confirm `rpanel_create_gravity_tbox` does NOT appear in BOTH a `BindTextboxStable(...)` attach AND a surviving per-frame `TextboxField` row; AND confirm its construction `UIFieldSpec` Gravity row's `data_bind` is now `NULL`.

- [ ] 9. LIVE DYNAMIC proof — migrate state_manager `damage_tbox` to `BindTextboxDynamic` with panel query + callback, single-writer removal, and the is_projectile visibility decision preserved.
      What: In `src/engine/system/ui/state_manager_system.c`:
      (a) Add two static functions (domain policy lives here, not in the core). `static BindingValue StateManager_QueryDamage(int key)`: `(void)key;` get `Newtonoid2d *object = UIState_GetSelectedObject();` and return `(BindingValue){ .type = BIND_NONE }` when `!object || object->id == INVALID_ENTITY_ID`, else `(BindingValue){ .type = BIND_FLOAT, .as.f = object->damage }`. `static bool StateManager_WriteDamage(int key, BindingValue value)`: `(void)key;` get the selection; return `false` when `!object || object->id == INVALID_ENTITY_ID || value.type != BIND_FLOAT`; else `object->damage = value.as.f; return true;`. Comment both (clear-when-none / reject-when-unselected policy owned here). The query keys ONLY on selection, NOT on `is_projectile` (visibility decision below).
      (b) After the `InitUIFields(gameplay_section, gameplay_specs, ARRAY_COUNT(gameplay_specs), ...)` call, add: `BindTextboxDynamic(s_sm_ui.damage_tbox, BIND_FLOAT, 2, StateManager_QueryDamage, StateManager_WriteDamage, 0);` with a comment that damage targets the current selection and the query/callback own selection policy. (The construction `gameplay_specs` Damage row already has `data_bind == NULL`, so no NULLing is needed there.)
      (c) Single-writer: in `RefreshPhysView`'s `TextboxField state_fields[]`, remove the damage row `{s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL},` (comment it out with `// MIGRATED to BindTextboxDynamic`). This removes the only site where `RefreshTextboxFields` -> `BindTextboxData` built a dynamic binder, so post-migration `binder == NULL` and commit flows solely through `Binding_Commit` -> the callback sink.
      (d) is_projectile VISIBILITY DECISION — option (a) from the design: LEAVE `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` in `RefreshGameplaySection` UNCHANGED. Visibility stays in the panel; the pull writes `object->damage` for any selected object (the walk ignores `is_enabled`), and the parent-disable hides it for non-projectiles — identical to today's visible behaviour. Do NOT move is_projectile into the query.
      Why: proves the DYNAMIC path on a live FLOAT field; the FLOAT query depends on step 1's `BindingValue` return and the callback on step 2's `BIND_SINK_CALLBACK`. (Addresses design-review v1 Finding 4 and v2 Finding 2.)
      Files: `src/engine/system/ui/state_manager_system.c`
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, links. Review-grep: confirm `s_sm_ui.damage_tbox` is NOT in both a `BindTextboxDynamic(...)` attach and a surviving `state_fields[]` row; confirm `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` is still present and unchanged.

- [ ] 10. Final whole-project build verification.
      What: Run the full configure + build once more after all edits to confirm the integrated result links cleanly. Review-check the two single-writer invariants (steps 8c/9c removed; 8a NULLed) and that both doc comments (step 5) and the core domain-freedom (steps 1-2 added no selection/debug/entity concept to `binding.h`/`binding.c`) hold.
      Files: none (verification only).
      Verify: `cmake --preset debug` then `cmake --build --preset debug` — exit code 0, `raylib-game.exe` links. If the only failure is a final-link `Permission denied` with every `.c` compiled, treat compile as passing and report that the user must close the running game (do NOT kill it, do NOT run the exe). DO NOT commit or stage anything.

---

## Out of scope (restated)

- lpanel edit-view migration (dead code; pattern note only — step 7).
- Any git operation (commit/stage/branch/worktree/rebase).
- Any new library, allocator, or build target.
- Running the produced `raylib-game.exe`.

## Gaps / assumptions

- The design's section-9 damage refresh lives in `RefreshPhysView` (verified) — the design text's "RefreshPhysView builds a state_fields[] row" is correct; the `SetParentEnabledState` call lives in `RefreshGameplaySection` (verified ~line 642). No conflict; both are left as the design specifies.
- `systems.h` must gain `#include "ui/binding.h"` for the new helper declarations to compile (header-ordering note in the approach section). If a future reader prefers to declare the helpers in a different header that already sees the binding types, that is an equivalent choice; this plan uses `systems.h` because the sibling helpers (`BindTextboxData`, `RefreshTextboxFields`, `TextboxField`) are already declared there.
