# Design Review: One Consistent Binding Mechanism for Every View Host

Reviewed document: `.agents/tasks/binding-consistency-design.md`
Repo: `c:\Projects\raylib-testing` (branch `ui-overhaul`)
Scope: DESIGN ONLY. No source, build, or git changes were made. No build or
tests were run.

This review was performed fresh. Every load-bearing claim in the design was
checked against the actual source rather than taken on the design's word. The
results of those checks are in the Verified / Unverified sections at the end.

---

## Summary verdict

CHANGES_REQUESTED.

HIGH: 0
MEDIUM: 1
NIT: 4

The design is unusually thorough and most of its hard claims (positional
initialiser, append-only struct layout, ON/OFF reuse, Describe/HasComponent
equivalence, `CommandSystem_ResolveToggleOverlay` staying for `ExecuteCommand`,
the single component loop, the 9 debug overlays, the 7 flag-button call sites)
are correct against the source. All ten gate items are met in substance. The one
MEDIUM is a concrete parity defect in the state-manager flag-family query guard
that would silently change on-screen behaviour; it must be corrected or
explicitly justified before implementation.

---

## Gate checklist (brief)

1. Binding struct UNCHANGED — MET. The design makes no structural edit; verified
   `include/ui/binding.h` already carries `source` + `sink` + `precision`.
2. No new XML tags — MET. Toggles stay `<Button type="simple">` with
   `binding=` + `action=`; no `<FlagButton>`/`<Toggle>`/`<FlagGrid>`/`format=`.
3. Loader stays domain-free — MET. `UIBinding`/`UIAction` carry only fn pointers
   + opaque int keys + value types; the inline `DataType->BindingValueType` map
   is not a domain name (`DataType` is already visible in `ui_loader.h`).
4. Resolver widening additive, no regression — MET (see Finding 1 which is a
   state-manager parity issue, NOT a loader regression). Append-only layout +
   legacy shim + retained `resolve_command` path preserve all existing callers.
5. Debug toggles declarative via `binding=` resolved by `LPanel_ResolveBinding`,
   `LPanel_AttachToggleSources` + its call DELETED — MET.
6. Existing `"<text>: ON/OFF"` composition reused, not duplicated — MET.
7. BIND_STRING non-owning lifetime documented — MET (section 5.2 + the per-query
   note).
8. State-manager flag-button plan present (concrete, not half-done) — MET. A
   concrete in-C plan (query source + retire `UpdateFlagButtons`) with a
   documented, defensible reason for leaving the sink `BIND_SINK_NONE` in the C
   interim.
9. Revert-safety verbatim LEGACY block, inert — MET (section 6).
10. Git untouched — MET (section 9).

---

## Findings

### 1. [MEDIUM] State-manager flag-family query guard changes on-screen behaviour vs `UpdateFlagButtons`

Where: section 4.2 example `StateManager_QueryEntityCapability`, and the general
rule "Each returns OFF when its target is absent - reproducing `UpdateFlagButtons`'
`is_valid` gating."

The design's per-family queries guard the read with
`o && o->id != INVALID_ENTITY_ID`. The design claims this matches
`UpdateFlagButtons`' `is_valid`. It does NOT, for the five entity flag families.
Verified in `state_manager_system.c`:

- `RefreshAttributeView` computes `bool is_valid = (object != NULL);` — there is
  **no** `id != INVALID_ENTITY_ID` term — and passes that `is_valid` to all five
  `UpdateFlagButtons` calls (entity_role, entity_capability, entity_constraint,
  entity_status, collision_role_mask).
- `RefreshComponentsSection` uses the *stricter*
  `bool is_valid = (object != NULL && object->id != INVALID_ENTITY_ID);` — that
  stricter term is the component buttons' gate, not the flag families'.

So the design has imported the *component* loop's validity term into the *flag
family* queries. For a selected object that is non-NULL but carries
`INVALID_ENTITY_ID`, today the five flag families display their actual bitmask
(`object->roles & flag`, etc.); the design's queries would display OFF. That is a
silent behaviour change, and the "additive with no regression" / "label value is
identical" claim in section 9 is violated for exactly this case.

Note the design's own prose compounds the confusion: 4.2 says the guard matches
"`UpdateFlagButtons`' `is_valid`", while 4.4 correctly describes the component
buttons as the ones with the `is_enabled = is_valid` (id-checked) semantics. The
two families genuinely have different validity terms in source and the design
collapses them.

CONCRETE FIX: make each family's query guard match the term that family uses
today. For the five entity flag families, drop the `id != INVALID_ENTITY_ID`
term so the guard is purely `o != NULL`, mirroring `RefreshAttributeView`:

```c
// Flag families mirror RefreshAttributeView's is_valid == (object != NULL).
static BindingValue StateManager_QueryEntityCapability(int flag_key)
{
    Newtonoid2d *o = UIState_GetSelectedObject();
    int on = (o && (o->capabilities & (uint32_t)flag_key)) ? 1 : 0; // no id check
    return (BindingValue){ .type = BIND_INT, .as.i = on };
}
```

Keep the stricter `o && o->id != INVALID_ENTITY_ID` guard ONLY for
`StateManager_QueryComponentAttached`, which mirrors `RefreshComponentsSection`.
Then correct the parity sentence in 4.2 to state the entity-flag queries match
`object != NULL` and the component query matches
`object != NULL && id != INVALID_ENTITY_ID`. (If the author instead believes a
selected object can never carry `INVALID_ENTITY_ID` so the two terms are
equivalent in practice, that must be stated and justified against
`UIState_GetSelectedObject`'s contract, not asserted implicitly — but the simpler,
provably-parity fix is to match each family's existing term.)

Also confirm the `world` and `cell` family queries mirror their current gates
(`world != NULL`; cell presence), which today use the plain non-NULL term in
`RefreshWorldView`/`RefreshCellView` — not an id check.

---

### 2. [NIT] `StateManager_QueryComponentAttached` indexes with a resolver-cast int without a documented bound

Where: section 4.2 `StateManager_QueryComponentAttached`,
`desc.components[(EntityComponentType)component_type]`.

Verified `EntityDescription.components` is sized `[ENTITY_COMPONENT_HEALTH + 1]`
and populated only for `type` in `1..ENTITY_COMPONENT_HEALTH`. The four button
types (PORTAL/ROTOR/GEAR/HEALTH) are all within range, so the current call sites
are safe. The query, however, indexes with whatever `int` key it is handed, and
the design does not state the invariant that the key is always a valid
`EntityComponentType` in `[0, ENTITY_COMPONENT_HEALTH]`. A future caller passing
an out-of-range key would read out of bounds.

CONCRETE FIX: add a one-line bound guard and document the key contract:

```c
// Key is an EntityComponentType in [1, ENTITY_COMPONENT_HEALTH]; guard the index.
EntityComponentType t = (EntityComponentType)component_type;
if (t < 1 || t > ENTITY_COMPONENT_HEALTH) return (BindingValue){ .type = BIND_INT, .as.i = 0 };
```

---

### 3. [NIT] "No `MarkStateManagerRefreshDirty` dependency for toggle labels" is stated as strictly-better without noting the per-frame query cost

Where: section 4.4 ("strictly more responsive and still correct") and section 9.

The flag/component ON/OFF labels move from dirty-driven recomputation to a
per-frame pull. Verified `UIElement_RefreshBinding` runs for every element every
frame via `ViewHostSystem_RefreshBindings`. For the entity families each refresh
now calls `UIState_GetSelectedObject()` per button, and the component query calls
`EntityRegistry_Describe` (which itself loops `EntityRegistry_GetComponent` over
all component types) once per component button per frame. This is almost
certainly negligible, but "strictly more responsive" omits that it is also
strictly more work per frame. Minor, but the design elsewhere claims performance
as a goal.

CONCRETE FIX: add a half-sentence acknowledging the trade: "the pull recomputes
each frame rather than on dirty; the per-button query cost (one
`UIState_GetSelectedObject` / one `EntityRegistry_Describe` per button) is
negligible at this button count."

---

### 4. [NIT] Section 1.3 advisory `value_type` for query sources is described but its use by the ON/OFF path is not pinned down

Where: section 1.3 (`value_type` "query advisory value type (query owns actual
type)") and the toggle display path.

Verified `UIElement_RefreshBinding`'s button branch composes ON/OFF purely from
`value.as.i` of the value the query returns (`BIND_INT`), ignoring any advisory
`value_type` on the source. So a debug/flag query that returns `BIND_INT` works
regardless of the advisory `value_type`. The design is internally consistent, but
it carries `value_type = BIND_INT` into the query source descriptor as if it
mattered to the toggle display, which could mislead the implementer into thinking
the advisory type drives the ON/OFF composition (it does not; the returned
`BindingValue.type`/`as.i` does).

CONCRETE FIX: note at section 2's query-source branch that for toggle buttons the
ON/OFF text depends only on the query's returned `as.i`, and the source
`value_type` is advisory and unused by the button display path (it matters only
if a query source is ever formatted through `Binding_FormatValue`, i.e. for
textbox/STRING sources).

---

### 5. [NIT] Section 6 legacy-block "inert so it cannot affect the build" should call out references to now-deleted symbols

Where: section 6, revert-safety LEGACY block.

The LEGACY block is specified as every removed line wrapped as a `//` comment, so
it does not compile — correct, and sufficient to keep the build green. But the
block will contain `UpdateFlagButtons(...)` call lines and the old
`CreateFlagButtons` body that reference the pre-change signatures and locals (e.g.
the removed `attached` local, the old `CreateFlagButtons` without the `query`
param). Because the whole block is commented, that is fine today; the risk is a
future reader un-commenting a single line for a partial revert and hitting a
compile error against the new signatures. This is purely a maintenance footgun,
not a build issue.

CONCRETE FIX: add one sentence to the LEGACY header noting that a revert is
all-or-nothing — the block restores the *pre-change* `CreateFlagButtons`
signature and call shape together, so individual lines must not be un-commented
in isolation.

---

## Verified Assumptions (checked against source)

- `Binding { BindingSource source; BindingSink sink; int precision; }` already
  holds both halves and already defines `BIND_SRC_QUERY`, `BIND_SINK_CALLBACK`,
  `BindingQueryFn`, `BindingSinkFn`. No struct change needed. (`include/ui/binding.h`)
- `LPanel_ResolveBinding` opens with the positional initialiser
  `UIBinding binding = {NULL, FLOAT};` — confirming the design's slot-0/slot-1
  append-only requirement in section 1.3. (`lpanel_system.c:39`)
- `UIBinding` today is exactly `{ void *address; DataType data_type; }`.
  (`include/system/ui/ui_loader.h`)
- `UIElement_RefreshBinding` button branch returns early when
  `b->source.kind == BIND_SRC_NONE` and composes `"%s: %s", text, on?"ON":"OFF"`
  from `value.as.i`. This is the single ON/OFF path; the design reuses it.
  (`src/engine/ui/ui.c:~1119-1135`)
- `BuildButton` today reads only `action=` into a `BIND_SINK_COMMAND` binding and
  attaches `UILoader_HandleCommandClick` only when `command_code != 0`. The
  design's `have_sink`/handler rule is equivalent. (`ui_loader.c:288-367`)
- `UILoader_HandleCommandClick` calls `Binding_WriteSink(&...->sink, BIND_NONE)`;
  the design's reuse for callback sinks is sound against `Binding_WriteSink`'s
  `BIND_SINK_CALLBACK` branch. (`ui_loader.c:272-281`, `binding.h`)
- `DebugOverlayId` has exactly the 9 overlays the design's `debug.*` table maps
  (DASHBOARD, VIEWPORT_GRID, WORLD_GRID, WORLD_GRID_LABELS, UNIVERSE_GRID_LABELS,
  UI_BORDERS, OBJECT_AXES, OBJECT_HULL, OBJECT_AABB). (`debug_overlay_system.h`)
- `lpanel.xml` has exactly those 9 toggle `<Button type="simple">` with the
  matching `action=` strings and no `binding=` yet — consistent with the planned
  additive edit. (`lpanel.xml`)
- `LPanel_AttachToggleSources` exists and is called once in `InitLPanel` after
  load; it derives the overlay via `CommandSystem_ResolveToggleOverlay`. Deleting
  both is as described. (`lpanel_system.c:197-250`)
- `CommandSystem_ResolveToggleOverlay` is also called by `ExecuteCommand` in
  `command_system.c`, so it correctly STAYS after the lpanel display-wiring use is
  removed. (`command_system.c:233,262`)
- `LPanel_QueryDebugEnabled` returns canonical `BIND_INT` 0/1 from
  `IsDebugEnabled`. (`lpanel_system.c:185-189`)
- `LPanel_ResolveBinding` currently only parses a `component.field` dot form and
  handles `physics.*`; a `debug.`-prefixed branch placed at the top returns before
  the dot-parse and does not interact with `component_len`/`dot_pos`. The design's
  section 3.2 placement is feasible. (`lpanel_system.c:37-116`)
- State-manager flag families and sizes match section 4.2: `entity_role[5]`,
  `entity_capability[4]`, `entity_constraint[2]`, `entity_status[3]`,
  `collision_role_mask[5]`, `world[7]`, `cell[4]`; `StateManagerFlagButton` has a
  `uint32_t flag` field usable as the query key. (`state_manager_system.c:100-119`)
- Component buttons are a SEPARATE struct `StateManagerComponentButton`
  {label; EntityComponentType type; button;} built in a SEPARATE inline loop in
  `InitStateManagerSystem` (4 buttons), not via `CreateFlagButtons`. The design's
  section 4.4 separate-attachment handling is correct. (`state_manager_system.c:27-32,533-546`)
- `UpdateFlagButtons` sets `is_enabled = true` UNCONDITIONALLY — confirming the
  flag families' "no behavioural loss" claim for the dropped per-frame enable.
  (`state_manager_system.c:419-432`)
- The component loop in `RefreshComponentsSection` is ONE loop with three
  statements per button (compute `attached`, `UpdateString64` label, set
  `is_enabled = is_valid`) and `desc` is used below the loop by the portal
  (`ENTITY_COMPONENT_PORTAL`) and relation (`ENTITY_COMPONENT_RELATION`) rows — so
  `desc` must stay declared and the enable write must stay, exactly as section 4.4
  specifies. (`state_manager_system.c:712-760`)
- `EntityRegistry_Describe(obj).components[type]` is filled by
  `EntityRegistry_GetComponent(obj->id, type)` for `type` in
  `1..ENTITY_COMPONENT_HEALTH`, and `EntityRegistry_HasComponent(id,type)` is
  defined as `GetComponent(id,type) != NULL`. The design's describe/HasComponent
  equivalence (section 4.2) is correct; `components` is sized
  `[ENTITY_COMPONENT_HEALTH + 1]`. (`entity_registry.c:467-561`, `entity_registry.h:22`)
- `ResolveBindingType(DataType)` is a file-static in `integration_system.c`
  mapping INT/FLOAT/VECTOR2D/STRING; the design's choice to re-map inline in the
  loader (rather than cross-TU) is justified and keeps the loader self-contained.
  (`integration_system.c:29-53`)
- `BindTextboxStable` / `BindTextboxDynamic` exist and are the C-build template
  the design mirrors for the state-manager query-source attach. (`integration_system.c`)
- `ui/ui.h` includes `ui/binding.h`, and `ui_loader.h` includes `ui/ui.h`, so
  `BindingQueryFn` / `BindingSinkFn` / `BindingValueType` are visible for the new
  typedefs — the design's section 1.3 visibility claim holds. (`include/ui/ui.h:13`)

## Unverified / Wrong Assumptions

- WRONG (Finding 1): the design's claim that the per-family query guard
  `o && o->id != INVALID_ENTITY_ID` "reproduces `UpdateFlagButtons`' `is_valid`
  gating" is incorrect for the five entity flag families. `RefreshAttributeView`
  passes `is_valid = (object != NULL)` with no id term; only
  `RefreshComponentsSection` uses the id-checked term. This is the one MEDIUM.
- UNVERIFIED: whether a selected object returned by `UIState_GetSelectedObject`
  can ever carry `INVALID_ENTITY_ID`. If it never can, the Finding 1 mismatch is
  benign in practice — but the design does not establish this, and the safe,
  provably-parity fix (match each family's existing term) is cheap, so the finding
  stands regardless.
- UNVERIFIED (acceptable, out of scope): the exact bodies of the `world` and
  `cell` family queries are not spelled out in the design; section 4.2 lists only
  their target expressions. Their validity-term parity (Finding 1 extension)
  should be confirmed at implementation against `RefreshWorldView` /
  `RefreshCellView`.
- UNVERIFIED (out of scope by the design's own section 7): the rpanel
  `BIND_STRING` query pattern (section 5) is documentation only; its
  stable-storage lifetime rule is stated correctly but no code is produced to
  verify, which is consistent with the "document only" scope.
