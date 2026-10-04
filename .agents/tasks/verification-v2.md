# Verification Note — Textbox Binding HYBRID Refactor (v2, first iteration)

This note records exactly what was built and verified. It is NOT committed or staged.

## Iteration

`review-v2.json` did not exist, so this was the FIRST iteration: the design
(`.agents/tasks/textbox-binding-design-v2.md`) and plan
(`.agents/tasks/textbox-binding-plan-v2.md`) were implemented from scratch.

## Build

Commands (UCRT64 + Ninja, PowerShell, from repo root):

```
cmake --preset debug        # configure, exit 0
cmake --build --preset debug # exit 0, raylib-game.exe links
```

Result: SUCCESS. Every `.c` compiled and `raylib-game.exe` linked. Final `$LASTEXITCODE == 0`.
The `ninja: warning: premature end of file; recovering` prefix appeared and was ignored
(not an error). No `Permission denied` link failure occurred. The produced `.exe` was NOT run.

Three builds were run: after core extensions (steps 1-2), after the carrier/commit edits
(steps 3-6), and the final whole-project build after the live migrations (steps 8-9). All exit 0.

## What became GENERIC (zero domain knowledge added to the core)

- `include/ui/binding.h`:
  - `BindingQueryFn` generalised from `int (*)(int)` to `BindingValue (*)(int)` (query owns the type).
  - `BindingSource.value_type` comment clarified (advisory for query, meaningful for address).
  - `BIND_SINK_CALLBACK` added as the last `BindingSinkKind`.
  - `BindingSinkFn` typedef `bool (*)(int key, BindingValue value)` added next to `BindingCommandFn`.
  - `BindingSink` gained `write` + `write_key` fields; `value_type` comment appended `callback sink: parse type`.
- `src/engine/ui/binding.c`:
  - `BIND_SRC_QUERY` branch now a straight pass-through `return src->query(src->query_key);`.
  - `Binding_WriteSink` gained one `BIND_SINK_CALLBACK` branch (`!write` -> false, else `write(write_key, value)`).
  - `Binding_Commit` unchanged.
- Grep confirms `binding.h`/`binding.c` carry no selection/debug/entity/projectile/damage/gravity
  logic (only the pre-existing comment that describes the decoupling itself).
- `UIElement_RefreshBinding` (ui.c) textbox branch only NULL-checks, focused-skips, and formats.
- The `PanelSystem_RefreshBindings` walk was not touched.

Domain policy lives in the panels: `LPanel_QueryDebugEnabled` (lpanel), `StateManager_QueryDamage`
/ `StateManager_WriteDamage` (state_manager), and `SetParentEnabledState(damage_tbox, is_projectile)`.

## LIVE fields migrated

- STABLE (fixed address): rpanel `rpanel_create_gravity_tbox` (FLOAT, next-world gravity) ->
  `BindTextboxStable(..., FLOAT, GetNextWorldGravityPtr(), 2)`.
- DYNAMIC (selection-driven): state_manager `damage_tbox` (FLOAT) ->
  `BindTextboxDynamic(..., BIND_FLOAT, 2, StateManager_QueryDamage, StateManager_WriteDamage, 0)`.

Left on LEGACY (unmigrated, additive): all other rpanel create fields (spawn, resolution, basis u,
basis v, objects) and all other state_manager fields (id/slot/mass/health/etc.). The lpanel
edit-view was NOT touched (dead code, documentation-only pattern note, out of scope).

## Focused-skip evidence

`UIElement_RefreshBinding` (ui.c) textbox branch returns early when `e->is_focused` BEFORE calling
`Binding_RefreshText`, so in-progress typing is never overwritten. This preserves the invariant
that previously lived in `RefreshTextboxFields` (which also skipped focused textboxes).

## Byte-identity evidence (commit path)

`HandleTextCommit` (ui_input.c) now prefers `binding` via `Binding_Commit`, else falls back to the
legacy `binder` via `Binder_ValidateAndWrite`, else reverts (matching the old binder-missing revert).
`Binder_ValidateAndWrite` is already a thin adapter that builds a `BIND_SINK_ADDRESS` sink with the
same `value_type`/`address`/`validator`/`validator_ctx` and runs `Binding_ParseText` +
`Binding_WriteSink` — the identical sequence `Binding_Commit` runs. The migrated gravity field has
no validator and an address sink, so the committed bytes land at the identical address with
identical content (INT/FLOAT/VECTOR2D direct store, STRING keeps the 255-cap in `Binding_WriteSink`).
For the dynamic damage field the only difference is the final store target (the callback instead of
a raw address); parse + validate are unchanged.

## Single-writer removals (verified by grep)

- `rpanel_create_gravity_tbox`:
  - construction `UIFieldSpec` Gravity row `data_bind` NULLed (was `GetNextWorldGravityPtr()`) so
    `InitUIFields` builds NO legacy `Binder`.
  - per-frame `TextboxField create_fields[]` gravity row commented out (`// MIGRATED to BindTextboxStable`).
  - sole carrier is now `BindTextboxStable`.
- `s_sm_ui.damage_tbox`:
  - construction `gameplay_specs` row already had `data_bind == NULL` (no change needed).
  - per-frame `state_fields[]` damage row in `RefreshPhysView` commented out
    (`// MIGRATED to BindTextboxDynamic`).
  - sole carrier is now `BindTextboxDynamic`; post-migration `binder == NULL`, so commit flows
    solely through `Binding_Commit` -> `StateManager_WriteDamage`.

Grep confirms neither textbox appears in BOTH a binding attach AND a surviving per-frame row.

## is_projectile visibility decision

Design option (a): `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` in
`RefreshGameplaySection` is LEFT UNCHANGED. Visibility policy stays in the panel. The query keys
ONLY on selection (returns BIND_NONE when nothing selected), NOT on is_projectile. Because the
refresh walk ignores `is_enabled`, the pull writes `object->damage` for any selected object, and
the parent-disable hides the row for non-projectiles — identical to today's visible behaviour.

## Git

Git was NOT mutated: no commit, no stage, no branch/worktree/rebase. All changes are left as
uncommitted working-tree edits for the user to commit manually.
