# Implementation Plan: One Consistent Binding Mechanism for Every View Host

Derived from the authoritative, approved design at
`.agents/tasks/binding-consistency-design.md`. The design is settled; this plan
only sequences its implementation. UK English in all new identifiers/comments.

Branch: `ui-overhaul`, in-place in the main worktree. DO NOT touch git in any way
(no stage/commit/branch/rebase). The user commits manually.

## Scope recap (what is and is not done)

- IN: resolver widening (additive), `BuildButton` reads `binding=` + `action=`,
  lpanel declarative `debug.*` toggle sources (delete the C post-pass), state
  manager flag/component buttons migrated to query-source `Binding`s in C
  (`UpdateFlagButtons` retired via the revert-safety LEGACY block).
- OUT (documentation only / deferred): rpanel BIND_STRING migration (design §5),
  any XML migration of rpanel/state-manager, the `resolve_action` callback-sink
  wiring beyond its type/field being added, renaming `UILoader_HandleCommandClick`.

## Build / verify commands (Windows PowerShell, UCRT64 + Ninja)

Only build when troubleshooting compile/link issues (per AGENTS.md); do NOT build
for trivial edits. No new `.c` file is added by this plan, so a reconfigure is NOT
required — but if `CMakeLists.txt` is ever touched, run configure first.

```powershell
cmake --preset debug                              # configure (only if CMakeLists changed / first time)
cmake --build --preset debug; "EXIT=$LASTEXITCODE"  # build; success = raylib-game.exe links and EXIT=0
```

Notes the coder MUST follow:
- `ninja: warning: premature end of file; recovering` is NOT an error.
- Never use npm/yarn/make/MSVC.
- EXE LOCK: if every `.c` compiled and only the FINAL link failed with
  `ld.exe: cannot open output file ... Permission denied`, that is an ENVIRONMENT
  lock, not a code error — treat compile as PASSING, run `Get-Process -Name raylib-game`,
  and if present report that the user must close the running game. Do NOT kill it,
  do NOT run the exe.
- To verify linking despite the lock, link all objects to a temp output
  (`linkcheck.exe`) then delete it. A `multiple definition` error naming a
  once-defined symbol means a stale orphan `.obj` from a prior rename is in the
  glob — delete it and relink. Prefer plain `cmake --build` as the primary signal.

This change touches no `CMakeLists.txt` and adds no translation unit, so the
convergence/verify evidence is: a clean `cmake --build --preset debug` with
`EXIT=0` (or compile-pass + link-lock as above).

---

# Implementation Plan

- [ ] 1. Widen the loader resolver structs in `include/system/ui/ui_loader.h` (additive, append-only).
      Grow `UIBinding` by APPENDING members after the existing two — keep `void *address;`
      in slot 0 and `DataType data_type;` in slot 1 (so `LPanel_ResolveBinding`'s positional
      `UIBinding binding = {NULL, FLOAT};` still means `{address=NULL, data_type=FLOAT}` and
      still compiles). Append in this order: `UIBindingSourceKind kind;`, `BindingQueryFn query;`,
      `int query_key;`, `BindingValueType value_type;`. Add the two new enums
      `UIBindingSourceKind { UI_BIND_SRC_NONE=0, UI_BIND_SRC_ADDRESS, UI_BIND_SRC_QUERY }` and
      `UIBindingSinkKind { UI_BIND_SINK_NONE=0, UI_BIND_SINK_COMMAND, UI_BIND_SINK_CALLBACK }`,
      the new `UIAction` struct (`kind`, `command_code`, `write` (BindingSinkFn), `write_key`,
      `value_type`), the `UIActionResolver` typedef
      (`typedef UIAction (*UIActionResolver)(const char *action_string, UILoaderContext *ctx);`),
      and add `UIActionResolver resolve_action;` to `UILoaderContext` next to the RETAINED
      `UICommandResolver resolve_command;`. `BindingQueryFn`/`BindingSinkFn`/`BindingValueType`
      are already visible via `ui/ui.h` -> `ui/binding.h` which this header includes.
      Add a comment on `UIBinding` stating the slot-0/slot-1 layout is a hard requirement
      (design §1.3). Do NOT change `UIBindingResolver`'s signature; do NOT remove `resolve_command`.
      Files: `include/system/ui/ui_loader.h`
      Verify: header-only; confirmed at the full build in step 9 (nothing links against the new
      types until later steps).

- [ ] 2. Teach `BuildButton` to assemble ONE `Binding` from BOTH `binding=` (source) and `action=` (sink) in `src/engine/system/ui/ui_loader.c`.
      Add a tiny file-static `DataType -> BindingValueType` map (switch over INT/FLOAT/VECTOR2D/STRING)
      so the loader stays self-contained and domain-free (document it as the single intentional
      duplication of `ResolveBindingType`). Rework `BuildButton`:
      (a) SOURCE: read `binding=`; if `ctx->resolve_binding` is set, call it; apply the legacy shim
      (`if (src.kind == UI_BIND_SRC_NONE && src.address) k = UI_BIND_SRC_ADDRESS;`); for ADDRESS set
      `b.source.kind=BIND_SRC_ADDRESS`, `value_type=map(data_type)`, `address=src.address`,
      `have_source=true`; for QUERY (with non-NULL `src.query`) set `b.source.kind=BIND_SRC_QUERY`,
      `value_type=src.value_type`, `query`/`query_key`, `have_source=true`; otherwise
      `LOADER_WARNING(ctx, "Binding did not resolve")`. If `binding=` present but no resolver, warn
      `"Binding specified but resolver not available"`.
      (b) SINK: resolve `action=` in the order (1) `ctx->resolve_action` -> `UIAction` dispatch on
      `kind` (COMMAND builds a `BIND_SINK_COMMAND` with `UILoader_DispatchCommand` + `command_code`;
      CALLBACK builds `BIND_SINK_CALLBACK` with `write`/`write_key`/`value_type`, add the one-line
      comment pointing at design §7 rename deferral; NONE leaves it sink-less), else (2) the existing
      `resolve_command` path (unchanged, builds a command sink), else (3) the existing
      integer-parse fallback. Set `have_sink=true` ONLY when the sink is COMMAND or CALLBACK.
      (c) Attach `UIEventHandler handler = have_sink ? UILoader_HandleCommandClick : NULL;` (preserves
      today's "command_code != 0 => handler" rule — a source-only button gets NO handler).
      (d) After `CreateUIButtonDefault`, keep the `button->is_enabled = enabled;` apply, then
      `if (button && (have_source || have_sink)) button->data.button.binding = Binding_Create(b);`
      Preserve all existing behaviour: `action=`-only buttons are byte-identical to today; no `binding=`
      and no `action=` => no Binding. Do NOT modify `BuildTextField`, `UILoader_HandleCommandClick`,
      or `UILoader_DispatchCommand`.
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: part of the step 9 build; functionally, lpanel toggles still dispatch their command and
      the DRAW `CREATE` submit still works after step 9 (manual run is the user's; do not run the exe).

- [ ] 3. Add `binding="debug.*"` to each of the nine debug toggle `<Button>` in `src/engine/ui/components/lpanel.xml`.
      Add one attribute per existing toggle, mirroring its `action=` (design §3.1 table): Dashboard ->
      `debug.dashboard`, Viewport Grid -> `debug.viewport-grid`, World Grid -> `debug.world-grid`,
      World Grid Labels -> `debug.world-grid-labels`, Universe Grid Labels -> `debug.universe-grid-labels`,
      UI Borders -> `debug.ui-borders`, Object Axes -> `debug.object-axes`, Object Hull ->
      `debug.object-hull`, Object AABB -> `debug.object-aabb`. Leave the DRAW-view `<TextField>`s and the
      `CREATE` button untouched. No new tags, no `format=`.
      Files: `src/engine/ui/components/lpanel.xml`
      Verify: XML well-formed; exercised at load time after steps 4-5 (toggle labels show `: ON/OFF`).

- [ ] 4. Add the `debug.*` query branch + overlay-name map to `LPanel_ResolveBinding` in `src/engine/system/ui/lpanel_system.c`.
      Add a file-static `LPanel_ResolveDebugOverlayName(const char *suffix, DebugOverlayId *out)` mapping
      the nine suffixes to `DebugOverlayId` (`dashboard`->`DEBUG_DASHBOARD`, `viewport-grid`->
      `DEBUG_VIEWPORT_GRID`, `world-grid`->`DEBUG_WORLD_GRID`, `world-grid-labels`->
      `DEBUG_WORLD_GRID_LABELS`, `universe-grid-labels`->`DEBUG_UNIVERSE_GRID_LABELS`, `ui-borders`->
      `DEBUG_UI_BORDERS`, `object-axes`->`DEBUG_OBJECT_AXES`, `object-hull`->`DEBUG_OBJECT_HULL`,
      `object-aabb`->`DEBUG_OBJECT_AABB`), returning false on no match. At the TOP of
      `LPanel_ResolveBinding` (BEFORE the `strchr`/dot-parse physics logic, so it never touches
      `component_len`/`dot_pos`), add: if `binding_string` starts with `"debug."` (6 chars), resolve the
      suffix via the new mapper and on success `return (UIBinding){ .kind = UI_BIND_SRC_QUERY,
      .query = LPanel_QueryDebugEnabled, .query_key = (int)id, .value_type = BIND_INT };`, else
      `return (UIBinding){ .kind = UI_BIND_SRC_NONE };`. Keep `LPanel_QueryDebugEnabled` verbatim. Keep the
      existing physics branch and the `{NULL, FLOAT}` initialiser unchanged. Note: the early-NULL guard
      `if (!binding_string || !G_UIState.entity_create_params) return binding;` currently sits before the
      dot-parse — place the `debug.` check AFTER the `!binding_string` NULL check but it must NOT depend on
      `entity_create_params` (debug bindings do not need it), so guard only on `binding_string` for the
      debug branch (re-order so the `debug.` branch runs even when `entity_create_params` is NULL).
      Files: `src/engine/system/ui/lpanel_system.c`
      Verify: covered by step 9 build; the resolver mapping is unit-reasoned (nine suffixes -> ids).

- [ ] 5. Delete `LPanel_AttachToggleSources` and its call in `src/engine/system/ui/lpanel_system.c` (deleted outright — this file is NOT under the revert-safety rule).
      Remove the whole `LPanel_AttachToggleSources` function definition and the single
      `LPanel_AttachToggleSources(root);` call in `InitLPanel` (and its explanatory comment line
      "Give toggle buttons a live debug-state source..."). KEEP `LPanel_QueryDebugEnabled` (now used by
      step 4's resolver). Do NOT touch `CommandSystem_ResolveToggleOverlay` in `command_system.c`
      (still used by `ExecuteCommand`). After this step the toggle source is resolved declaratively at
      load time inside `BuildButton` (steps 2-4).
      Files: `src/engine/system/ui/lpanel_system.c`
      Verify: step 9 build has no unused-function/undefined-symbol errors; lpanel toggles still show
      `: ON/OFF` and self-update (the user verifies visually; do not run the exe).

- [ ] 6. Add the state-manager per-family is-set query fns + the component-attached query fn in `src/engine/system/ui/state_manager_system.c`.
      Place these file-static fns near `StateManager_QueryDamage` (same style, same includes already
      present: `entities/entity_registry.h`, `entities/entity_components.h`, `world/universe.h`). All return
      `BindingValue{ .type = BIND_INT, .as.i = 0|1 }`. Validity terms MUST match each family's current gate
      exactly (design §4.2 — do NOT add an id term to the entity families):
      - `StateManager_QueryEntityRole(int flag_key)`: `o = UIState_GetSelectedObject(); on = (o && (o->roles & (uint32_t)flag_key)) ? 1 : 0;` (guard `o != NULL` ONLY).
      - `StateManager_QueryEntityCapability` -> `o->capabilities` (guard `o != NULL` only).
      - `StateManager_QueryEntityConstraint` -> `o->constraints` (guard `o != NULL` only).
      - `StateManager_QueryEntityStatus` -> `o->status_flags` (guard `o != NULL` only).
      - `StateManager_QueryCollisionMask` -> `o->collision_role_mask` (guard `o != NULL` only).
      - `StateManager_QueryWorldFlag(int flag_key)`: `w = Universe_GetSelectedWorld(&G_Universe); on = (w && (w->flags & (uint32_t)flag_key)) ? 1 : 0;` (mirror `RefreshWorldView`'s `world != NULL`).
      - `StateManager_QueryCellFlag(int flag_key)`: `c = UIState_GetSelectedCell(); on = (c && (c->flags & (uint32_t)flag_key)) ? 1 : 0;` (mirror `RefreshCellView`'s non-NULL).
      - `StateManager_QueryComponentAttached(int component_type)`: STRICTER guard
        `o && o->id != INVALID_ENTITY_ID` AND a bounds guard
        `t >= 1 && t <= ENTITY_COMPONENT_HEALTH` on `EntityComponentType t = (EntityComponentType)component_type;`
        then `EntityDescription desc = EntityRegistry_Describe(o); on = (desc.components[t] != NULL) ? 1 : 0;`
        (SAME expression the retired label loop used — parity by construction, design §4.2).
      Add a UK-English description comment above each fn (per AGENTS.md), and on the entity-family fns a
      one-line note that the guard is `o != NULL` only (matches `RefreshAttributeView`, no id term).
      Files: `src/engine/system/ui/state_manager_system.c`
      Verify: compiles in step 9; queries are pure fns of current selection + key.

- [ ] 7. Give flag/component buttons a query-source `Binding` at creation and drop the `UpdateFlagButtons` call sites in `src/engine/system/ui/state_manager_system.c`.
      (a) Change `CreateFlagButtons`'s signature to add a trailing `BindingQueryFn query` parameter; after
      each `CreateUIButtonDefault`, attach a source-only Binding (sink left `BIND_SINK_NONE`):
      `Binding b = { .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT, .query = query,
      .query_key = (int)buttons[i].flag } }; buttons[i].button->data.button.binding = Binding_Create(b);`
      (guard on `buttons[i].button` non-NULL). The existing `click_handler` wiring stays verbatim (the
      `UIEventHandler` still owns the write; design §4.3/§8).
      (b) Update the SEVEN `CreateFlagButtons` call sites to pass the matching family query fn from step 6:
      `entity_role`->`StateManager_QueryEntityRole` (InitAttributeStateView),
      `entity_capability`->`StateManager_QueryEntityCapability`,
      `entity_constraint`->`StateManager_QueryEntityConstraint`,
      `entity_status`->`StateManager_QueryEntityStatus`,
      `collision_role_mask`->`StateManager_QueryCollisionMask`,
      `world`->`StateManager_QueryWorldFlag` (InitWorldStateView),
      `cell`->`StateManager_QueryCellFlag` (InitCellStateView).
      (c) In the `comp_buttons` creation loop in `InitPhysStateView` (after the existing
      `CreateUIButtonDefault` that sets `comp_buttons[i].button`), attach its own query-source Binding keyed
      on the component TYPE: `Binding b = { .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT,
      .query = StateManager_QueryComponentAttached, .query_key = (int)s_sm_ui.comp_buttons[i].type } };
      s_sm_ui.comp_buttons[i].button->data.button.binding = Binding_Create(b);` (guard on button non-NULL).
      The `CreateFlagButtons` `query` param does NOT reach these buttons — this is a separate site.
      (d) Remove the FIVE `UpdateFlagButtons(...)` calls in `RefreshAttributeView` (keep the function's other
      work; note the `is_valid`/`*_flags` locals become unused there — remove the now-dead locals too), the
      ONE in `RefreshWorldView` (keep its `RefreshTextboxFields` work), and the ONE in `RefreshCellView`.
      All seven removed call lines are preserved in the LEGACY block in step 8.
      Files: `src/engine/system/ui/state_manager_system.c`
      Verify: step 9 build; flag/component labels now update every frame via
      `ViewHostSystem_RefreshBindings` reading the query (user verifies visually; do not run the exe).

- [ ] 8. Edit the component loop in place and move all removed `state_manager_system.c` code VERBATIM into a commented LEGACY block (revert-safety rule — this file ONLY).
      (a) In `RefreshComponentsSection`'s single component loop (do NOT add a new loop, do NOT delete the
      whole loop): delete the `bool attached = ...;` local and the `UpdateString64(..., "%s: %s", ...
      attached ? "ON" : "OFF")` label write; KEEP `if (!s_sm_ui.comp_buttons[i].button) continue;` and
      `s_sm_ui.comp_buttons[i].button->is_enabled = is_valid;`. `desc` STAYS declared (still used below by the
      portal/relation rows).
      (b) Append a fully-commented LEGACY block at the BOTTOM of the file under the exact header
      `// === LEGACY (pre-binding-consistency) — retained for easy revert ===` (plus a line noting the
      pointer to `.agents/tasks/binding-consistency-design.md` and that a revert is ALL-OR-NOTHING). Copy
      VERBATIM, each line wrapped as an inert `//` comment (so it cannot affect the build), per design §6:
      1. the retired `UpdateFlagButtons` full definition (removed from its current location — step 7 removed
         its calls; now remove the definition too and move it here);
      2. the OLD `CreateFlagButtons` signature/body BEFORE the `query` param was added (so the pre-change
         creation path is restorable);
      3. the seven removed `UpdateFlagButtons(...)` call-site lines with their enclosing context comment
         (five from `RefreshAttributeView`, one from `RefreshWorldView`, one from `RefreshCellView`);
      4. the removed component `UpdateString64` label line AND its `attached` local from (a).
      Do NOT move the retained `is_enabled = is_valid;` write or the `desc` declaration. Other files
      (ui_loader.*, lpanel_system.c, lpanel.xml) are NOT under this rule and change normally.
      Files: `src/engine/system/ui/state_manager_system.c`
      Verify: the LEGACY block is fully commented (inert); confirmed by the step 9 build linking with no
      duplicate-definition / unused-static errors.

- [ ] 9. Build and resolve any compile/link issues.
      Run `cmake --build --preset debug; "EXIT=$LASTEXITCODE"`. Expect `EXIT=0` and
      `build/Debug/raylib-game/raylib-game.exe` linked. Treat `ninja: warning: premature end of file`
      as noise. If the ONLY failure is the final link with `Permission denied` on the output exe and every
      `.c` compiled, treat compile as PASSING, check `Get-Process -Name raylib-game`, and report that the
      user must close the running game (do NOT kill it, do NOT run the exe); optionally link to a temp
      `linkcheck.exe` to confirm linking then delete it. Fix any genuine compile errors (most likely:
      a missing new query fn forward reference, an unused-local warning-as-error in `RefreshAttributeView`,
      or a positional-init regression in `UIBinding` — re-check step 1's slot ordering). Do NOT re-run the
      exe; the user runs/verifies the game.
      Files: none (build only)
      Verify: `cmake --build --preset debug` with `EXIT=0` (or compile-pass + link-lock as above).

## Dependency notes / ordering rationale

- Steps 1 -> 2 are hard-ordered: `BuildButton` uses the new structs/fields.
- Step 2 -> 3/4: the loader must read `binding=` + resolve QUERY before the XML and
  resolver changes have any effect; 3 and 4 are independent of each other but both
  must precede step 5's deletion (otherwise toggles lose their source between edits —
  though the codebase stays buildable at each step regardless).
- Steps 6 -> 7 -> 8: the query fns must exist before the call sites reference them;
  the LEGACY move (8) comes last so the removed code is captured exactly as left.
- Each step leaves the codebase buildable; the single build (step 9) is the end-to-end
  signal since the design is additive and no `.c`/`CMakeLists` is added.

## Key files touched

- `include/system/ui/ui_loader.h` — grow `UIBinding`; add `UIAction`,
  `UIBindingSourceKind`, `UIBindingSinkKind`, `UIActionResolver`, `resolve_action`.
- `src/engine/system/ui/ui_loader.c` — `BuildButton` reads `binding=` (source) +
  `action=` (sink); inline `DataType->BindingValueType` map; new sink resolution order.
- `src/engine/system/ui/lpanel_system.c` — `debug.*` query branch +
  `LPanel_ResolveDebugOverlayName`; delete `LPanel_AttachToggleSources` + its call;
  keep `LPanel_QueryDebugEnabled`.
- `src/engine/ui/components/lpanel.xml` — `binding="debug.*"` on the nine toggles.
- `src/engine/system/ui/state_manager_system.c` — per-family + component query fns;
  `CreateFlagButtons` gains `query` param; comp-button loop attaches its own Binding;
  component loop edited in place; `UpdateFlagButtons` + 7 call sites + old
  `CreateFlagButtons` body + component label line moved to the commented LEGACY block.

Unchanged (do NOT edit): `include/ui/binding.h`, `src/engine/ui/binding.c`,
`src/engine/ui/ui.c` (`UIElement_RefreshBinding`), `view_host_system.c`,
`integration_system.c`, `command_system.c`, `rpanel_system.c`.
