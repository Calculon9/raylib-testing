# Binding-consistency refactor — implementation review (v2)

Migrates every view host onto one declarative binding mechanism: a widget declares a display
**source** via `binding=` and an optional commit **sink** via `action=`, both resolved by
application-supplied resolvers into the single `Binding` struct and driven by the shared
per-frame refresh walk. The loader resolver structs are widened (append-only), `BuildButton`
now assembles both halves into one `Binding`, lpanel's nine debug toggles become declarative
`binding="debug.*"` query sources (the C post-pass is deleted), and the state-manager
flag/component buttons gain query-source `Binding`s with the old `UpdateFlagButtons`
hand-roll retired into an inert commented LEGACY block.

Watch for: the only remaining friction is a wording conflict inside the acceptance list
itself — one line asks for "the `Binding` struct in `binding.h` UNCHANGED" while another asks
for "a resolve_action path can yield a CALLBACK sink", which structurally requires
`BIND_SINK_CALLBACK` to exist in `binding.h` (**confirmed**). The orchestrator already ruled on
this in the previous pass and amended the gate (recorded in
`binding-consistency-verification.md`): the additive core widening is permitted and expected;
the real constraint is that the change is additive/non-regressive, which it is. No new
blocking concern was found in this pass.

**Verdict**: APPROVED

## High-level view

The loader-side widening is append-only and domain-free. `UIBinding` keeps `address` in slot 0
and `data_type` in slot 1 and appends `kind`/`query`/`query_key`/`value_type`, so lpanel's
positional `{NULL, FLOAT}` initialiser still means `{address=NULL, data_type=FLOAT}`; the new
`UIAction` sink descriptor and opt-in `resolve_action` sit beside the retained
`resolve_command`, and the loader names no application symbol.

`BuildButton` reads both attributes into one `Binding`: a source from `binding=` (address or
query, with the legacy non-NULL-address shim) and a sink from `action=` (opt-in `resolve_action`
→ legacy `resolve_command` → integer-parse). A click handler is attached only when a sink
resolved, so an `action=`-only button is byte-identical to before and a `binding=`-only button
becomes a sink-less live label.

lpanel's debug toggles are declarative: the nine `<Button>`s carry `binding="debug.*"` paired
with their existing `action="toggle-*"`, `LPanel_ResolveBinding` resolves the `debug.` prefix to
a query source before the physics dot-parse and independent of `entity_create_params`, and
`LPanel_AttachToggleSources` plus its call are gone while `LPanel_QueryDebugEnabled` and
`CommandSystem_ResolveToggleOverlay` are retained.

The state-manager flag buttons move onto real query-source `Binding`s. The five entity families
guard on `object != NULL` only — the corrected parity with `RefreshAttributeView`, with no
`id != INVALID_ENTITY_ID` term — while `StateManager_QueryComponentAttached` keeps the stricter
id + bounds guard and reads attachment through the same
`EntityRegistry_Describe(o).components[t]` expression the retired label loop used. All seven
`UpdateFlagButtons` call sites are removed, the component loop keeps `is_enabled = is_valid`
with `desc` still declared, and every removed line is preserved verbatim in a fully-commented
inert LEGACY block with the all-or-nothing revert note.

The binding core (`binding.h`/`binding.c`) and the textbox carrier (`ui.c`/`ui.h`) are modified,
but strictly additively: `BindingQueryFn` is retyped to return a `BindingValue` (query owns the
type; the core passes it through), `BIND_SINK_CALLBACK`/`BindingSinkFn` are added and
`BindingSink` grows `write`/`write_key`, and a textbox refresh arm is added. No enumerator or
field is removed or reordered; the top-level `Binding { source; sink; precision; }` shape is
intact; address and command paths are untouched. The build evidence is present and credible: a
clean `cmake --build --preset debug` with `EXIT=0`, the exe linked, and the exe not run.

<details>
<summary>Issues (1)</summary>

1. **Gate wording self-conflict (non-blocking, already adjudicated)** — The acceptance list
   simultaneously demands "`Binding` struct in `binding.h` UNCHANGED" and "resolve_action can
   yield a CALLBACK sink"; the latter requires `BIND_SINK_CALLBACK` in `binding.h`, so the two
   cannot both hold literally. The orchestrator amended the gate in the prior pass
   (`binding-consistency-verification.md`) to permit the additive core widening; the real
   additive/non-regressive constraint holds. No action required for this change; the stale
   "unchanged" phrasing should be dropped from any future restatement of the gate.

</details>

<details>
<summary>Details</summary>

### Resolver widening is append-only and domain-free

`UIBinding` keeps `void *address` in slot 0 and `DataType data_type` in slot 1, appending
`kind`, `query`, `query_key`, `value_type` after them, with a header comment stating the layout
is a hard requirement. That is what keeps `LPanel_ResolveBinding`'s positional
`UIBinding binding = {NULL, FLOAT};` meaning `{address=NULL, data_type=FLOAT}` with the appended
tail zero-initialised to `UI_BIND_SRC_NONE`/`NULL`. The sink side adds `UIBindingSinkKind`, the
`UIAction` descriptor (`kind`, `command_code`, `write`, `write_key`, `value_type`), the
`UIActionResolver` typedef, and a `resolve_action` field on `UILoaderContext` placed next to the
retained `resolve_command` (and `resolve_view_type`, which is bundled unrelated work). A grep of
`ui_loader.c` for `IsDebugEnabled`/`ENTITY_ROLE_`/`CMD_TOGGLE`/`DEBUG_*` finds nothing — the
loader never names an application symbol.

### BuildButton assembles one Binding from both halves

The source branch resolves `binding=` via `resolve_binding`, applies the legacy shim
(`kind == UI_BIND_SRC_NONE && address ⇒ UI_BIND_SRC_ADDRESS`) and builds either a
`BIND_SRC_ADDRESS` source (type mapped by the self-contained `UILoader_MapDataType`, which maps
the three `STRING*` variants to `BIND_STRING` — the fix recorded in the verification evidence)
or a `BIND_SRC_QUERY` source. The sink branch resolves `action=` in the order `resolve_action`
(dispatching `UI_BIND_SINK_COMMAND`/`UI_BIND_SINK_CALLBACK`) → legacy `resolve_command` →
integer-parse, setting `have_sink` only for a command or callback sink. The handler is
`have_sink ? UILoader_HandleCommandClick : NULL` and the `Binding` is attached when
`have_source || have_sink`, so an `action=`-only button is byte-identical to before and a
`binding=`-only button becomes a sink-less live label. The callback-sink arm carries the
one-line comment pointing at the section 7 rename deferral.

### lpanel debug toggles are declarative

All nine toggles carry `binding="debug.*"` matching the design table, each alongside its
`action="toggle-*"`; no `<FlagButton>`/`<Toggle>`/`format=` was introduced. In
`LPanel_ResolveBinding` the `debug.` branch runs after the `!binding_string` guard and before
both the `!G_UIState.entity_create_params` guard and the `strchr` dot-parse, mapping the suffix
through the new `LPanel_ResolveDebugOverlayName` and returning a
`BIND_SRC_QUERY`/`LPanel_QueryDebugEnabled` source keyed on the `DebugOverlayId`; the physics
address dot-parse and the `{NULL, FLOAT}` initialiser are retained verbatim below it.
`LPanel_AttachToggleSources` and its `InitLPanel` call are gone (grep finds no references),
`LPanel_QueryDebugEnabled` is kept (now normalising to a canonical `BIND_INT` 0/1 `BindingValue`),
and `CommandSystem_ResolveToggleOverlay` is untouched and still used by `ExecuteCommand`.

### State-manager parity by construction

The five entity-family queries (`QueryEntityRole`/`Capability`/`Constraint`/`Status`/
`CollisionMask`) guard on `o != NULL` only — the corrected parity with `RefreshAttributeView`'s
`is_valid = (object != NULL)`, with the `id != INVALID_ENTITY_ID` term deliberately absent (an
inline comment on each notes this). `QueryWorldFlag`/`QueryCellFlag` mirror their views' plain
non-NULL gates. Only `StateManager_QueryComponentAttached` carries the stricter
`o && o->id != INVALID_ENTITY_ID` plus the bounds guard `t >= 1 && t <= ENTITY_COMPONENT_HEALTH`,
reading `EntityRegistry_Describe(o).components[t] != NULL` — the same expression the retired
label loop used, so the ON/OFF display is identical by construction.

`CreateFlagButtons` gains a trailing `query` parameter and attaches a query-source `Binding`
(sink left `BIND_SINK_NONE`, the click handler keeps the write path); all seven call sites pass
the matching family query. The component-button loop in `InitPhysStateView` attaches its own
type-keyed `Binding` at a separate site. All seven `UpdateFlagButtons` calls are removed;
`RefreshComponentsSection`'s loop drops the `attached` local and the `UpdateString64` label
write while keeping `is_enabled = is_valid` and the `desc` declaration (still used by the
portal/relation rows below); the exclusive-vs-independent click handlers are untouched.

### LEGACY block is inert and complete

The block at the file bottom opens with the exact header
`// === LEGACY (pre-binding-consistency) — retained for easy revert ===`, carries the
all-or-nothing revert note, and contains — each line an inert `//` comment — the retired
`UpdateFlagButtons` definition, the old `CreateFlagButtons` body, the seven removed call sites
with context (five from `RefreshAttributeView`, one from `RefreshWorldView`, one from
`RefreshCellView`), and the removed component label line plus its `attached` local. Being fully
commented it cannot affect the build, and the recorded clean link with no duplicate-definition
errors confirms it.

### Binding-core / textbox edits are additive (gate wording note)

`git show HEAD:include/ui/binding.h` has `typedef int (*BindingQueryFn)(int key);`, no
`BIND_SINK_CALLBACK`, and no `BindingSinkFn`. The working tree retypes `BindingQueryFn` to
return `BindingValue`, adds `BIND_SINK_CALLBACK` and `BindingSinkFn`, and grows `BindingSink`
with `write`/`write_key`. `binding.c` adds the `BIND_SINK_CALLBACK` dispatch arm (no `write` fn
⇒ reject) and passes the query's returned `BindingValue` through unchanged; the address/command
arms are untouched, so existing bindings do not regress. `ui.c`/`ui.h` add a `Binding *binding`
carrier to `TextBoxData` alongside the retained `data_bind`/`binder`, and a textbox refresh arm
that is skipped while focused. No enumerator or field is removed or reordered and the top-level
`Binding { source; sink; precision; }` shape is unchanged. The refactor's own code
(`LPanel_QueryDebugEnabled` returning `BindingValue`, `BuildButton`'s callback-sink arm) depends
on these additive widenings. The literal "`binding.h` UNCHANGED" phrasing in the acceptance list
conflicts with the "resolve_action yields a CALLBACK sink" criterion in the same list; the
orchestrator amended the gate in the prior pass to permit the additive widening, and the
additive/non-regressive constraint holds.

### Build evidence and git state

The verification file records a first attempt failing on a non-existent `STRING` enumerator,
fixed by mapping the three `STRING*` variants, then a second attempt where all 111 objects
compiled and the final link produced `raylib-game.exe` with `EXIT=0`; the exe was not run. That
satisfies the build criterion. Read-only `git status`/`git log` confirm `HEAD` is `fd66fff` and
all binding-consistency edits are uncommitted on `ui-overhaul`; no git mutation was performed by
this review. (An index-staged `PanelSystem`→`ViewHostSystem` rename predates this review and was
not created here.)

</details>

<details>
<summary>Files reviewed</summary>

- `include/system/ui/ui_loader.h` — append-only `UIBinding`; new `UIAction`/`UIActionResolver`/
  sink+source kind enums; `resolve_action` (plus bundled `resolve_view_type`/selector decls)
- `src/engine/system/ui/ui_loader.c` — `BuildButton` reads both halves; `UILoader_MapDataType`
- `src/engine/ui/components/lpanel.xml` — `binding="debug.*"` on the nine toggles
- `src/engine/system/ui/lpanel_system.c` — `debug.*` resolver branch + overlay-name map;
  `LPanel_AttachToggleSources` deleted; `LPanel_QueryDebugEnabled` retained
- `src/engine/system/ui/state_manager_system.c` — per-family query fns, `CreateFlagButtons`
  `query` param, component-button binding, in-place loop edit, inert LEGACY block
- `include/ui/binding.h`, `src/engine/ui/binding.c`, `src/engine/ui/ui.c`, `include/ui/ui.h` —
  additive callback-sink / query-return / textbox-carrier widening

Full diff: `git diff HEAD` on branch `ui-overhaul`.

</details>
