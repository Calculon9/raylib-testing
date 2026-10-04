# Design: One Consistent Binding Mechanism for Every View Host

## Overview

Every `ViewHostSystem` host (lpanel, rpanel, state-manager) already shares one
draw path (`ViewHostSystem_Draw`) and one per-frame pull
(`ViewHostSystem_RefreshBindings` -> `UIElement_RefreshBinding`). The binding
*core* (`include/ui/binding.h`, `src/engine/ui/binding.c`) is already symmetric
and complete: a `Binding` carries a `BindingSource` (display read) and a
`BindingSink` (commit write), and the core already supports every source kind
(`BIND_SRC_ADDRESS`, `BIND_SRC_QUERY`) and sink kind (`BIND_SINK_ADDRESS`,
`BIND_SINK_COMMAND`, `BIND_SINK_CALLBACK`) that this work needs.

What is *not* consistent is how widgets acquire that `Binding`. Three divergent
mechanisms exist today:

1. **lpanel** loads from `lpanel.xml`, but its debug-toggle *source* is bolted on
   by a C post-pass (`LPanel_AttachToggleSources`) that walks the loaded tree and
   derives a query source from each button's command code. The XML cannot express
   "this button also reads a live value".
2. **state-manager** is C-built and uses a hand-rolled toggle display
   (`UpdateFlagButtons` does `UpdateString64(..., "%s: %s", label, on?"ON":"OFF")`
   every refresh) instead of a real `Binding` + the generic refresh walk.
3. **rpanel** is C-built and hand-formats bespoke multi-value readouts
   (`"%d/%d"`, `"%.0fx%.0f"`) directly into textbox strings each frame.

The goal is a single declarative mechanism: a widget declares a **source** via
`binding=` (what it displays) and optionally a **sink** via `action=` (what a
click/commit does); both are resolved by application-supplied resolvers into the
one existing `Binding` struct; and the existing per-frame refresh walk drives
display for every host identically. The bespoke per-panel hand-rolls and the C
post-pass toggle wiring are retired.

This design is **additive with no regression**. It **does not touch git**, **adds
no new XML tags**, and **leaves the `Binding` struct unchanged**. A flag button /
debug toggle remains a `<Button type="simple">` carrying a `binding=` (query
source) and an `action=` (sink); there is no `<FlagButton>`, `<Toggle>`,
`<FlagGrid>`, or `format=` attribute.

## Technology stack (locked)

C11, built with CMake + FetchContent, UCRT64 gcc + Ninja. Dependencies raylib,
cJSON, Mini-XML (`mxml`). No new dependencies. UK English in identifiers and
comments (`colour`, `Unrecognised`, `Initialise`). The symmetric binding core
and the `UILoader_*` registry/resolver architecture are the frameworks this
change extends; nothing new is introduced.

## Current verified state (what this builds on)

- `Binding { BindingSource source; BindingSink sink; int precision; }` already
  holds both halves. `Binding_ReadSource` resolves address or query; the
  `BIND_SRC_QUERY` branch calls `query(key)` and passes the returned
  `BindingValue` through unchanged (any type). `Binding_WriteSink` dispatches
  address / command / callback; `BIND_SINK_CALLBACK` calls `write(write_key,
  value)`.
- `UIElement_RefreshBinding` (`src/engine/ui/ui.c`): for a button whose binding
  `source.kind != BIND_SRC_NONE` it reads the source and composes
  `snprintf(display_text, "%s: %s", button.text, value.as.i ? "ON" : "OFF")`;
  for a textbox it formats the source into the textbox text, skipping focused
  textboxes. A button with no source keeps its authored text (plain command
  button). This is the ONE display path for boolean toggles - no new path is
  added.
- Loader resolvers (`include/system/ui/ui_loader.h`): `UIBindingResolver` returns
  `UIBinding { void *address; DataType data_type; }` (**address only** today);
  `UICommandResolver` returns an `int` command code; `UIViewTypeResolver` resolves
  a `ViewType`. `BuildButton` resolves `action=` to a command code and builds a
  `BIND_SINK_COMMAND` binding; it does **not** read `binding=` today.
  `BuildTextField` resolves `binding=` to an address via `resolve_binding`.
- lpanel debug toggles (TO BE RETIRED): `LPanel_AttachToggleSources(root)` walks
  the tree post-load and, for each button whose command sink maps to a
  `CMD_TOGGLE_*` (via `CommandSystem_ResolveToggleOverlay`), attaches a
  `BIND_SRC_QUERY` source using `LPanel_QueryDebugEnabled` (returns `BIND_INT`
  0/1 from `IsDebugEnabled`).
- state-manager hand-rolls (TO BE RETIRED under the revert-safety rule):
  `CreateFlagButtons` builds `UI_ELEMENT_BUTTON_SIMPLE` buttons; `UpdateFlagButtons`
  hand-writes the `"label: ON/OFF"` text each refresh. The click handlers
  (`HandleEntityTypeFlagClick` exclusive-set; `HandleEntityCapabilityFlagClick`,
  `HandleEntityConstraintFlagClick`, `HandleEntityStatusFlagClick`,
  `HandleCollisionMaskFlagClick`, `HandleWorldFlagClick`, `HandleCellFlagClick`
  toggle-bit) encode EXCLUSIVE vs INDEPENDENT semantics and must be preserved.
- The three C bind helpers (`include/system/systems.h`, `integration_system.c`):
  `BindTextboxStable` (address<->address), `BindTextboxDynamic` (query/callback),
  `ClearTextboxBinding`. These stay for code-built UI and are the exact template
  for the state-manager flag-button helper below.
- `rpanel` and `state-manager` are still C-built; `rpanel.xml` and
  `state_manager.xml` exist as authored mirrors but are **not** loaded.

---

## 1. Resolver widening (the one real API change)

### 1.1 Problem

`resolve_binding` can only express an **address source**. The lpanel debug
toggles need a **query source** (`BindingQueryFn` + key); the state-manager flag
buttons (if migrated declaratively later) need a query source for display and a
callback sink for the click. The sink side can only express a **command code**;
it needs to also express a **callback** (`BindingSinkFn` + key). And `BuildButton`
needs to populate a Binding's *source* half from `binding=`, which it does not do
at all today.

### 1.2 Chosen approach: widen the resolver return structs with a kind tag

Two viable shapes were considered:

- **(A) Parallel resolver callbacks** - add `resolve_query_source` and
  `resolve_callback_sink` alongside the existing resolvers. Rejected: it
  multiplies the resolver surface (and the `UILoaderContext` fields and the
  loader entry-point signatures) and forces every app to register and dispatch
  across several callbacks that all answer "what does this attribute mean".
- **(B) One tagged return struct per attribute** - keep exactly the two existing
  resolver callbacks (`resolve_binding` for `binding=`, `resolve_command`... see
  below) but widen what each returns to a tagged union of
  address / query (for the source) and address / command / callback (for the
  sink). **Chosen.** It is additive, keeps one resolver per XML attribute, keeps
  the loader's dispatch trivial (one `switch` on `kind`), and keeps all app
  domain names inside the app resolver.

Within (B), the sink resolution has a sub-choice: extend `UICommandResolver` (an
`int`-returning function) to a struct, OR keep `resolve_command` returning the
command code and add the callback case onto the *binding* resolver. Chosen:
**introduce a widened sink descriptor returned by a renamed/extended resolver so
the sink can be command OR callback**, while preserving the existing
`resolve_command` call path for backward compatibility (see 1.5). This keeps the
`binding=` attribute meaning "source" and the `action=` attribute meaning "sink",
matching the user's settled framing exactly.

### 1.3 New typedefs in `include/system/ui/ui_loader.h`

Add a source descriptor that can carry an address **or** a query, tagged by kind.
The kinds mirror `BindingSourceKind` but stay loader-local so the header does not
force every resolver to include the full enum semantics; they map 1:1 in the
loader.

```c
// Kind of source a binding= attribute resolved to.
typedef enum {
    UI_BIND_SRC_NONE = 0,   // unresolved / absent
    UI_BIND_SRC_ADDRESS,    // *(T*)address
    UI_BIND_SRC_QUERY,      // query(query_key) -> BindingValue
} UIBindingSourceKind;

// Widened binding (source) descriptor. CRITICAL LAYOUT CONSTRAINT: `address` and
// `data_type` MUST remain the FIRST TWO members, in that order. The new members
// (`kind`, `query`, `query_key`, `value_type`) are APPENDED AFTER `data_type` and
// must never be inserted before it. The reason is given directly below.
typedef struct {
    void *address;              // slot 0 (unchanged): UI_BIND_SRC_ADDRESS data address
    DataType data_type;         // slot 1 (unchanged): address deref interpretation
    UIBindingSourceKind kind;   // NEW (appended): which payload is valid; 0 == UI_BIND_SRC_NONE
    BindingQueryFn query;       // NEW: UI_BIND_SRC_QUERY display read fn
    int query_key;              // NEW: UI_BIND_SRC_QUERY opaque key (e.g. DebugOverlayId)
    BindingValueType value_type;// NEW: query advisory value type (query owns actual type)
} UIBinding;
```

`UIBinding` is **grown, not replaced**. Why the ordering is mandatory:
`LPanel_ResolveBinding` initialises its result with a **positional** initialiser
(verified in `lpanel_system.c`):

```c
UIBinding binding = {NULL, FLOAT};  // positional: NULL -> slot 0, FLOAT -> slot 1
```

This is a positional, **not** a designated, initialiser. It assigns `NULL` to the
first member and `FLOAT` to the second member *by position*. By keeping `address`
in slot 0 and `data_type` in slot 1, this init continues to mean exactly
`{address = NULL, data_type = FLOAT}` and continues to compile unchanged, and the
appended members (`kind`, `query`, `query_key`, `value_type`) are value-initialised
to zero -> `kind == UI_BIND_SRC_NONE`, `query == NULL`. If `kind` were placed in
slot 0 instead, the positional init would assign `NULL` to `kind` and the integer
`FLOAT` to `address` (a type-mismatched, semantically broken pointer) - so the
append-only layout is a hard requirement, not a stylistic preference. No edit to
`LPanel_ResolveBinding`'s existing initialiser or to `BuildTextField`'s
`binding.address` / `binding.data_type` reads is needed. `BindingQueryFn` and
`BindingValueType` are already visible via `ui/ui.h` -> `ui/binding.h`, which
`ui_loader.h` includes.

Add a sink descriptor that can carry a command code **or** a callback:

```c
// Kind of sink an action= attribute resolved to.
typedef enum {
    UI_BIND_SINK_NONE = 0,  // unresolved / absent
    UI_BIND_SINK_COMMAND,   // command(command_code, NULL)
    UI_BIND_SINK_CALLBACK,  // write(write_key, value)
} UIBindingSinkKind;

// Widened action (sink) descriptor returned by the action resolver.
typedef struct {
    UIBindingSinkKind kind;     // which payload below is valid
    int command_code;           // UI_BIND_SINK_COMMAND: CommandType code (0 => none)
    BindingSinkFn write;        // UI_BIND_SINK_CALLBACK: panel store fn
    int write_key;              // UI_BIND_SINK_CALLBACK: opaque key
    BindingValueType value_type;// callback: parse/commit value type
} UIAction;
```

### 1.4 Resolver callback signatures

Keep the `binding=` resolver one callback, widened return:

```c
// Resolve a binding= string to a source descriptor (address OR query).
typedef UIBinding (*UIBindingResolver)(const char *binding_string, UILoaderContext *ctx);
```

The signature is unchanged; only the returned struct grew. Existing
implementations (`LPanel_ResolveBinding`) keep returning an address `UIBinding`
and need **no edit** because the new (appended) fields zero-initialise to
`UI_BIND_SRC_NONE`/NULL - but the loader must treat a returned non-NULL
`address` as `UI_BIND_SRC_ADDRESS` even when `kind == UI_BIND_SRC_NONE`, to
honour the old implicit contract (see 1.6).

**This shim is only sound because of the slot-0/slot-1 layout from section 1.3.**
An unmodified address resolver returns `{address = <ptr-or-NULL>, data_type =
<type>, kind = 0 (zero-init tail), query = NULL, ...}`. So `kind ==
UI_BIND_SRC_NONE` together with a non-NULL `address` unambiguously means "legacy
address source", and `kind == UI_BIND_SRC_NONE` with `address == NULL` means
"unresolved". Were the struct reordered to put `kind` first (rejected in 1.3), the
positional init would scribble a non-NULL garbage value into `address` and the
shim would fire on garbage. The implementer must NOT reorder the struct; the shim
rule and the layout are a matched pair.

For the sink, add one widened resolver and keep the old one for compatibility:

```c
// NEW: resolve an action= string to a sink descriptor (command OR callback).
typedef UIAction (*UIActionResolver)(const char *action_string, UILoaderContext *ctx);
```

`UILoaderContext` gains one field: `UIActionResolver resolve_action;` placed next
to `resolve_command`. The existing `UICommandResolver resolve_command` field and
typedef are **retained unchanged** (see 1.5).

### 1.5 Backward compatibility for the sink

`BuildButton` resolves the sink in this order:

1. If `ctx->resolve_action` is set, call it -> `UIAction`. Dispatch on `kind`:
   `UI_BIND_SINK_COMMAND` builds a command sink; `UI_BIND_SINK_CALLBACK` builds a
   callback sink; `UI_BIND_SINK_NONE` leaves the button sink-less.
2. Else if `ctx->resolve_command` is set (the legacy path), call it -> `int code`
   and build a command sink exactly as today.
3. Else fall back to the existing integer-parse-of-`action=` path.

lpanel keeps using `resolve_command` (step 2) - its toggles are command sinks and
nothing about the sink changes for lpanel. No app is forced to adopt
`resolve_action`; it is opt-in for hosts that need callback sinks (future
state-manager XML migration). This preserves every existing caller.

### 1.6 Why the loader stays domain-free

`UIBinding`/`UIAction` carry only function pointers, an opaque `int` key, and a
value type - never a domain name. The loader never references `IsDebugEnabled`,
`CMD_TOGGLE_*`, `ENTITY_ROLE_*`, or any app symbol. The app resolver
(`LPanel_ResolveBinding`, a future `StateManager_ResolveBinding`) owns every name
and every function pointer; it is the only place that knows "`debug.world-grid`"
means `LPanel_QueryDebugEnabled` keyed on `DEBUG_WORLD_GRID`. This is the same
decoupling the binding core already uses (opaque fn + int key) and the same
pattern `resolve_view_type` already follows.

### 1.7 Edge cases and error handling (resolver layer)

- **`binding=` present but no resolver**: `BuildButton`/`BuildTextField` log
  `LOADER_WARNING(ctx, "Binding specified but resolver not available")` (textbox
  wording already exists) and build no source. Recoverable; widget renders with
  authored text, no regression.
- **Resolver returns `UI_BIND_SRC_NONE` and NULL address**: unresolved name. Log
  `LOADER_WARNING(ctx, "Binding did not resolve")`; build no source. Recoverable.
- **`UI_BIND_SRC_QUERY` with NULL `query`**: treat as unresolved (same warning);
  do not attach a half-built source. The core would degrade a NULL query to
  `BIND_NONE` anyway, but rejecting at build time is clearer. Recoverable.
- **`action=` resolves to `UI_BIND_SINK_NONE`**: button is sink-less (inert, no
  click handler attached), identical to today's "no action" button. Recoverable.
- **`UI_BIND_SINK_CALLBACK` with NULL `write`**: unresolved; warn and leave the
  button sink-less. Recoverable.
- No new fatal conditions. The loader continues on all of the above, matching its
  documented "continue on non-fatal errors" contract.

---

## 2. `BuildButton` assembles ONE Binding from BOTH `binding=` and `action=`

Today `BuildButton` builds a Binding with only a `.sink` (command). It must now
also read `binding=` and populate the `.source` half of the *same* Binding, so a
single `Binding_Create` call carries both halves - exactly as `BindTextboxStable`
/ `BindTextboxDynamic` already do for textboxes.

New `BuildButton` flow (pseudocode, preserving all existing behaviour):

```c
Binding b = {0};
bool have_source = false, have_sink = false;

// SOURCE from binding= (new).
const char *binding_attr = mxmlElementGetAttr(node, "binding");
if (binding_attr && ctx && ctx->resolve_binding) {
    UIBinding src = ctx->resolve_binding(binding_attr, ctx);
    // Honour the legacy implicit contract: a non-NULL address with kind NONE
    // means address source.
    UIBindingSourceKind k = src.kind;
    if (k == UI_BIND_SRC_NONE && src.address) k = UI_BIND_SRC_ADDRESS;

    if (k == UI_BIND_SRC_ADDRESS && src.address) {
        b.source.kind = BIND_SRC_ADDRESS;
        b.source.value_type = ResolveBindingType(src.data_type); // DataType -> BindingValueType
        b.source.address = src.address;
        have_source = true;
    } else if (k == UI_BIND_SRC_QUERY && src.query) {
        b.source.kind = BIND_SRC_QUERY;
        b.source.value_type = src.value_type; // advisory; query owns actual type
        b.source.query = src.query;
        b.source.query_key = src.query_key;
        have_source = true;
    } else {
        LOADER_WARNING(ctx, "Binding did not resolve");
    }
} else if (binding_attr) {
    LOADER_WARNING(ctx, "Binding specified but resolver not available");
}

// SINK from action= (existing command path + new callback path, section 1.5).
// have_sink is set TRUE only when the sink resolves to BIND_SINK_COMMAND or
// BIND_SINK_CALLBACK. A UI_BIND_SINK_NONE result (and the source-only case) leaves
// have_sink == false.
... resolve action to b.sink (command OR callback); set have_sink accordingly ...

// Attach the click handler only when a sink exists.
// This is EQUIVALENT to today's rule: today handler = (command_code != 0) ?
// UILoader_HandleCommandClick : NULL, and a resolved command => have_sink. A
// source-only (read-only label) button has have_sink == false and therefore
// handler == NULL - it must NOT get a click handler, matching today's behaviour
// for buttons with no action.
UIEventHandler handler = have_sink ? UILoader_HandleCommandClick : NULL;

... CreateUIButtonDefault(...) ...

if (button && (have_source || have_sink)) {
    button->data.button.binding = Binding_Create(b);
}
```

Notes:

- `ResolveBindingType` (the `DataType -> BindingValueType` map used by the C bind
  helpers) is in `integration_system.c` as a file-static. `BuildButton` needs the
  same mapping for an **address** button source. Rather than reach across TUs,
  the loader maps `DataType` -> `BindingValueType` inline with a tiny local
  switch (INT/FLOAT/VECTOR2D/STRING), keeping the loader self-contained and
  domain-free. Document this as the single intentional duplication.
- `UILoader_HandleCommandClick` already drives a command sink by calling
  `Binding_WriteSink(&binding->sink, BIND_NONE)`. For a **callback** sink the
  same handler works unchanged: `Binding_WriteSink`'s `BIND_SINK_CALLBACK` branch
  calls `write(write_key, value)` with that `BIND_NONE` value. For a toggle whose
  click just flips state (no committed value needed) this is sufficient; the
  handler name is slightly command-centric but renaming it is cosmetic and out of
  scope. The implementer should add a one-line comment at `BuildButton`'s
  callback-sink branch pointing at the section 7 rename deferral (e.g.
  `// callback sinks also use UILoader_HandleCommandClick; rename deferred - see design section 7`)
  so a future reader is not surprised that a "command" handler drives a callback.
  (If a future callback sink needs the widget's *value*, that is a separate
  enhancement; the flag toggles here do not.)
- A button with `binding=` but no `action=` gets a source-only Binding and no
  click handler - a live read-only ON/OFF label. A button with `action=` but no
  `binding=` is unchanged (sink-only, keeps authored text). A button with neither
  gets no Binding (unchanged).

Testability: `BuildButton` is exercised via `UILoader_LoadFromStringWithResolvers`
with a stub resolver returning each kind; assert the resulting
`button->data.button.binding->source.kind` / `.sink.kind`. This is a unit-level
test on the loader with no raylib window needed.

---

## 3. lpanel debug-toggle migration (declarative source + delete post-pass)

### 3.1 Declarative `binding=` on each toggle `<Button>`

Each debug toggle in `lpanel.xml` gains a `binding=` naming the overlay it
reads. Chosen namespace: `debug.<overlay>` mirroring the existing `action=`
command names, so the two attributes read as a matched source/sink pair:

```xml
<Button type="simple" text="World Grid"
        binding="debug.world-grid" action="toggle-world-grid"
        size="4.65,0.5" size-mode="fixed" />
```

The full set (one per existing toggle, names mirroring the `action=` strings):

| Button text            | action=                      | binding=                   |
|------------------------|------------------------------|----------------------------|
| Dashboard              | toggle-debug-dashboard       | debug.dashboard            |
| Viewport Grid          | toggle-viewport-grid         | debug.viewport-grid        |
| World Grid             | toggle-world-grid            | debug.world-grid           |
| World Grid Labels      | toggle-world-grid-labels     | debug.world-grid-labels    |
| Universe Grid Labels   | toggle-universe-grid-labels  | debug.universe-grid-labels |
| UI Borders             | toggle-ui-borders            | debug.ui-borders           |
| Object Axes            | toggle-object-axes           | debug.object-axes          |
| Object Hull            | toggle-object-hull           | debug.object-hull          |
| Object AABB            | toggle-object-aabb           | debug.object-aabb          |

### 3.2 `LPanel_ResolveBinding` maps `debug.*` to the query source

`LPanel_ResolveBinding` (which today only handles `physics.*` address bindings)
gains a `debug.*` branch returning a **query** `UIBinding`:

```c
// Inside LPanel_ResolveBinding, before the physics.* branch:
if (!strncmp(binding_string, "debug.", 6)) {
    DebugOverlayId id;
    if (LPanel_ResolveDebugOverlayName(binding_string + 6, &id)) {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = LPanel_QueryDebugEnabled,   // existing shim, kept
            .query_key = (int)id,
            .value_type = BIND_INT,
        };
    }
    // Unknown overlay name -> unresolved (loader warns).
    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}
```

The `debug.*` branch is placed at the TOP of `LPanel_ResolveBinding`, before the
existing `component.field` dot-parse. It is self-contained: it matches the literal
`"debug."` prefix (6 chars) and uses `binding_string + 6` as the overlay suffix,
returning before the dot-parse runs, so it does NOT reuse or interact with the
existing `component_len`/`dot_pos` physics logic (the two prefixes are disjoint).

`LPanel_QueryDebugEnabled` is **kept verbatim** - it already returns the canonical
`BIND_INT` 0/1 that `UIElement_RefreshBinding` composes into `"<text>: ON/OFF"`.
A small new static `LPanel_ResolveDebugOverlayName(const char *suffix,
DebugOverlayId *out)` maps the nine suffixes to `DebugOverlayId` values, living in
`lpanel_system.c` (app owns the names). This replaces the old command-code ->
overlay derivation; the overlay id is now resolved straight from the `binding=`
string, independent of the `action=` command.

### 3.3 Delete `LPanel_AttachToggleSources`

With the source resolved declaratively at load time inside `BuildButton`,
`LPanel_AttachToggleSources` and its single call in `InitLPanel` are **deleted
outright** (this file is not under the revert-safety rule). `LPanel_QueryDebugEnabled`
stays (now referenced by the resolver). The dependency on
`CommandSystem_ResolveToggleOverlay` for *display wiring* is removed; that
function remains in `command_system.c` because `ExecuteCommand` still uses it for
dispatch - unchanged.

### 3.4 Behaviour parity

Before: load builds command-sink buttons; post-pass attaches query sources; the
refresh walk composes ON/OFF. After: load builds buttons with BOTH the query
source (from `binding=`) and the command sink (from `action=`) in one Binding;
the identical refresh walk composes ON/OFF. The displayed result is byte-for-byte
identical, and the label still self-updates when toggled by hotkey or any other
path (pure pull from `IsDebugEnabled`). No functional change for the user.

Testability: load `lpanel.xml` via the real `InitLPanel`, assert each toggle
button's `binding->source.kind == BIND_SRC_QUERY` and that
`Binding_ReadSource` returns the current `IsDebugEnabled` state. Integration-level
(needs the command/debug subsystems); the resolver mapping itself is unit-testable
with a stub.

---

## 4. state-manager flag buttons (SHOULD - clean in-C migration)

### 4.1 Feasibility verdict: DO IT, in C, without the XML migration

The SHOULD item is cleanly achievable without converting state-manager to
XML-loading, because the binding core is construction-agnostic: a `Binding` built
in C with `Binding_Create` and attached to `button->data.button.binding` is
driven by the **same** `ViewHostSystem_RefreshBindings` walk that drives the
XML-loaded lpanel. The flag toggle display is already a boolean ON/OFF - exactly
what `UIElement_RefreshBinding`'s button-source path composes. So we give each
flag button a real `Binding` (query **source**; sink left `BIND_SINK_NONE` in the
C interim, see section 4.3) in `CreateFlagButtons` and retire `UpdateFlagButtons`.

This is NOT the deferred XML migration and does NOT need the `resolve_action`
resolver (that is only for the *XML* path). It is C construction using the same
core, mirroring `BindTextboxDynamic`.

### 4.2 Query source: is-flag-set returning BIND_INT

Each flag button needs a query that returns `BIND_INT` 1/0 for "is this flag set
on the current selection". The selection target differs per flag family (entity
object, world, cell) and the flag field differs (roles, capabilities,
constraints, status, collision mask, world flags, cell flags). The query is
keyed on an opaque `int`; we need the key to encode BOTH *which family* and
*which flag bit*.

Chosen key encoding: the query reads the `StateManagerFlagButton` spec, not a
packed int. But `BindingQueryFn` takes only an `int`. Two options:

- **(A) Pack family + flag into the int key.** Fragile (flag values are wide
  bitmasks; packing a family tag alongside a 32-bit flag needs care) and spreads
  domain knowledge into bit-twiddling.
- **(B) One query fn per flag family, keyed on the flag bit.** Each family already
  has its own click handler (`HandleEntityCapabilityFlagClick`, etc.) and its own
  selection target; add a matching query fn per family that reads the current
  target and returns `(target->field & key) ? 1 : 0`. **Chosen** - it mirrors the
  existing per-family handler structure exactly, keeps each query trivially
  correct, and needs no packing. The `int` key is just the flag bit
  (`spec->flag`), which fits an `int` for all existing flags.

So we add, alongside the seven existing click handlers, seven query fns:

```c
// Example (capabilities family). The query owns the "what is selected / none"
// policy: no selection => flag reads 0 (OFF). The guard is purely `o != NULL`,
// matching RefreshAttributeView's `is_valid = (object != NULL)` for all five
// entity flag families (NO id != INVALID_ENTITY_ID term - adding one would
// silently flip a non-NULL/INVALID_ENTITY_ID object's bitmask display to OFF,
// a regression vs UpdateFlagButtons). The stricter id-checked guard is used ONLY
// by StateManager_QueryComponentAttached, which mirrors RefreshComponentsSection.
static BindingValue StateManager_QueryEntityCapability(int flag_key)
{
    Newtonoid2d *o = UIState_GetSelectedObject();
    int on = (o && (o->capabilities & (uint32_t)flag_key)) ? 1 : 0; // no id check
    return (BindingValue){ .type = BIND_INT, .as.i = on };
}
```

Families and their query reads (each returns `BIND_INT` 0/1):
`entity_role` -> `object->roles`; `entity_capability` -> `object->capabilities`;
`entity_constraint` -> `object->constraints`; `entity_status` ->
`object->status_flags`; `collision_role_mask` -> `object->collision_role_mask`;
`world` -> `Universe_GetSelectedWorld(...)->flags`; `cell` ->
`UIState_GetSelectedCell()->flags`. Each returns OFF when its target is absent.

Validity-term parity (must match each family's CURRENT gate exactly, per review
Finding 1):
- The five ENTITY flag families (role, capability, constraint, status,
  collision mask) use `is_valid = (object != NULL)` in `RefreshAttributeView` -
  NO `id != INVALID_ENTITY_ID` term. Their queries guard with `o != NULL` only.
- The `world` family guards with `Universe_GetSelectedWorld(...) != NULL` and the
  `cell` family with `UIState_GetSelectedCell() != NULL` - mirror `RefreshWorldView`
  / `RefreshCellView`'s plain non-NULL terms (confirm against those at impl).
- ONLY `StateManager_QueryComponentAttached` uses the stricter
  `o != NULL && o->id != INVALID_ENTITY_ID`, mirroring `RefreshComponentsSection`.
Importing the component loop's id-checked term into the flag families would be a
silent regression and is explicitly forbidden.

**Component buttons are a separate case - NOT a bitmask read.** The four
component-toggle buttons (PORTAL/ROTOR/GEAR/HEALTH) are a *different* struct
(`StateManagerComponentButton { label; EntityComponentType type; button; }`) built
by a *different* inline loop (not `CreateFlagButtons`), and attachment is NOT a
`field & bit` test on the object. The current label loop
(`RefreshComponentsSection`, verified at `state_manager_system.c`) computes
attachment via `EntityDescription desc = EntityRegistry_Describe(object)` then
`desc.components[type] != NULL`.

To make the swap **parity-exact by construction**, the query reads via the SAME
expression the label loop uses today - `EntityRegistry_Describe(o).components[type]
!= NULL` - keyed on the `EntityComponentType` (not a flag bit):

```c
// Returns BIND_INT 1 if the selected object has this component attached, else 0.
// Key is the EntityComponentType cast to int (NOT a flag bitmask).
// Reads via the SAME path the retired label loop used (EntityRegistry_Describe +
// desc.components[type] != NULL), so the displayed ON/OFF is identical by
// construction - no reliance on describe-vs-HasComponent agreement.
static BindingValue StateManager_QueryComponentAttached(int component_type)
{
    Newtonoid2d *o = UIState_GetSelectedObject();
    int on = 0;
    // Key contract: component_type is an EntityComponentType in
    // [1, ENTITY_COMPONENT_HEALTH]. Guard the index so a future out-of-range key
    // cannot read past desc.components (sized [ENTITY_COMPONENT_HEALTH + 1]).
    EntityComponentType t = (EntityComponentType)component_type;
    if (o && o->id != INVALID_ENTITY_ID && t >= 1 && t <= ENTITY_COMPONENT_HEALTH)
    {
        EntityDescription desc = EntityRegistry_Describe(o);
        on = (desc.components[t] != NULL) ? 1 : 0;
    }
    return (BindingValue){ .type = BIND_INT, .as.i = on };
}
```

**Why describe, not `EntityRegistry_HasComponent`:** the two are in fact
equivalent - verified in `entity_registry.c`,
`EntityRegistry_Describe(obj).components[type]` is populated by
`EntityRegistry_GetComponent(obj->id, type)` for every `type` in
`1..ENTITY_COMPONENT_HEALTH` (which covers all four button types), and
`EntityRegistry_HasComponent(id, type)` is defined as `EntityRegistry_GetComponent(id,
type) != NULL`. So either path yields the same ON/OFF. We nonetheless use the
`EntityRegistry_Describe` form because it is the exact expression the removed
label loop evaluated, making parity a textual identity rather than an argued
equivalence - the safer choice if the two registry paths ever diverge in future.

### 4.3 Callback sink: the existing handlers, adapted

The existing click handlers are `UIEventHandler` (`void(UIElement*)`) and read
`button->data.button.user_data` for the spec. The callback-sink path
(`BindingSinkFn` = `bool(int key, BindingValue)`) is invoked by
`UILoader_HandleCommandClick` -> `Binding_WriteSink` with a `BIND_NONE` value.
For state-manager C-built buttons we are NOT going through the loader's click
handler though - these buttons keep their existing `UIEventHandler` click
handlers (they are attached directly in `CreateFlagButtons`). So the **sink is
not strictly needed to drive the click** here: the click still runs
`HandleEntityCapabilityFlagClick(button)` which mutates state and marks dirty,
exactly as today.

Decision: **keep the click wiring as the existing `UIEventHandler` handlers
(unchanged), and attach a source-only `Binding` (query, no sink) for display.**
This is the minimal, lowest-risk migration that satisfies the actual goal
(retire `UpdateFlagButtons` in favour of the refresh walk) while preserving the
EXCLUSIVE vs INDEPENDENT click semantics verbatim in their current handlers. The
`Binding` struct still holds both halves; we simply leave the sink
`BIND_SINK_NONE` because the panel's own `UIEventHandler` owns the write path for
these C-built buttons.

Rationale for not forcing a callback sink here: the user's framing says a flag
button *may* carry a sink via `action=`, and that the exclusive/toggle handlers
must be **preserved as the callback-sink targets** *when the declarative path is
used*. For the C-built interim these handlers already ARE the click path; wrapping
them in a `BindingSinkFn` and routing clicks through `Binding_WriteSink` would be
pure churn with identical behaviour and MORE code. The callback-sink wiring
becomes relevant only in the deferred XML migration (section 7), where the loader
needs a resolvable sink. This keeps the interim change minimal and additive. If
the reviewer prefers full source+sink symmetry even in the C interim, section 4.5
documents the drop-in sink variant.

### 4.4 `CreateFlagButtons` adjusted; `UpdateFlagButtons` retired

`CreateFlagButtons` gains a per-family query fn parameter and attaches a
source-only `Binding` to each button:

```c
static void CreateFlagButtons(UIElement *section, StateManagerFlagButton *buttons,
                              size_t count, UIEventHandler click_handler,
                              BindingQueryFn query /* NEW: per-family is-set query */)
{
    for (size_t i = 0; i < count; i++) {
        buttons[i].button = CreateUIButtonDefault(
            section, UI_ELEMENT_BUTTON_SIMPLE, buttons[i].label,
            ui_wide_button_size, ui_standard_button_padding,
            state_manager_panel->palette, click_handler, (void *)&buttons[i], NULL);

        // Attach a query SOURCE so the generic refresh walk composes "label: ON/OFF".
        // Sink left NONE: the UIEventHandler click_handler owns the write for C-built buttons.
        if (buttons[i].button) {
            Binding b = {
                .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT,
                            .query = query, .query_key = (int)buttons[i].flag },
            };
            buttons[i].button->data.button.binding = Binding_Create(b);
        }
    }
}
```

Each `CreateFlagButtons` call site passes its family's query fn (e.g.
`StateManager_QueryEntityCapability`).

**Component buttons - explicit, separate attachment site.** The component buttons
are NOT built through `CreateFlagButtons`, so the `query` parameter above does NOT
reach them. Their `Binding` is attached directly in the existing `comp_buttons`
creation loop, immediately after `CreateUIButtonDefault`, keyed on `type` (not a
flag bit):

```c
// In the comp_buttons creation loop (InitStateManagerSystem), after the existing
// CreateUIButtonDefault call that sets s_sm_ui.comp_buttons[i].button:
if (s_sm_ui.comp_buttons[i].button) {
    Binding b = {
        .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_INT,
                    .query = StateManager_QueryComponentAttached,
                    .query_key = (int)s_sm_ui.comp_buttons[i].type },
    };
    s_sm_ui.comp_buttons[i].button->data.button.binding = Binding_Create(b);
}
```

`UpdateFlagButtons` is **retired** (moved to the legacy block per the
revert-safety rule, section 6). Its call sites are removed from
`RefreshAttributeView` (five: entity_role, entity_capability, entity_constraint,
entity_status, collision_role_mask), `RefreshWorldView` (one: world), and
`RefreshCellView` (one: cell). Those functions keep only their textbox/string
readout work (which stays on `RefreshTextboxFields`/`UpdateString64`). The ON/OFF
labels now update every frame via `ViewHostSystem_RefreshBindings` reading the
query - no `MarkStateManagerRefreshDirty` dependency for toggle labels, which is
strictly more responsive and still correct.

**`is_enabled` handling - scoped per button family (resolves Findings 4 & 5).**
The refresh walk (`UIElement_RefreshBinding`) only rewrites `display_text`; it
never touches `is_enabled`. So the per-frame `is_enabled` writes that the old
hand-rolls performed are NOT reproduced by the pull, and each family must be
handled on its own merits:

- **Flag families** (`UpdateFlagButtons`): set `is_enabled = true`
  *unconditionally* every frame. This is equivalent to enabling once at creation
  (`CreateUIButtonDefault` already enables by default), so dropping the per-frame
  write is genuinely **no behavioural loss** for these families.
- **Component buttons** (`RefreshComponentsSection`): set `is_enabled = is_valid`
  every frame (disabled when there is no valid selection). This is a real
  per-frame behaviour that the display pull does NOT reproduce, so the enable
  write must stay. The label write must go.

**The component loop is ONE loop today; it must be edited in place, not replaced
or deleted.** Verified at `state_manager_system.c`, the current single loop body
is:

```c
// CURRENT (one loop, three statements per button):
for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_buttons); i++) {
    if (!s_sm_ui.comp_buttons[i].button) continue;
    bool attached = is_valid && (desc.components[s_sm_ui.comp_buttons[i].type] != NULL); // (label src)
    UpdateString64(s_sm_ui.comp_buttons[i].button->data.button.display_text.string,
                   "%s: %s", s_sm_ui.comp_buttons[i].label, attached ? "ON" : "OFF");    // (label write)
    s_sm_ui.comp_buttons[i].button->is_enabled = is_valid;                              // (enable write)
}
```

The migration edits this SAME loop in place: delete the `attached` local and the
`UpdateString64` label write (the query now owns the ON/OFF label), KEEP the
`is_enabled = is_valid` write. The resulting live loop is exactly:

```c
// LIVE after migration - label composition removed, enable write retained.
for (size_t i = 0; i < ARRAY_COUNT(s_sm_ui.comp_buttons); i++) {
    if (!s_sm_ui.comp_buttons[i].button) continue;
    s_sm_ui.comp_buttons[i].button->is_enabled = is_valid; // kept
}
```

Do NOT add a second/new loop and do NOT delete the whole loop (that would take the
enable write with it). The `desc` local MUST remain declared in
`RefreshComponentsSection` - it is still used below the loop by the portal and
relation field rows (`desc.components[ENTITY_COMPONENT_PORTAL]`,
`desc.components[ENTITY_COMPONENT_RELATION]`, verified), so only the `attached`
local becomes unused and is removed with the label line.

Thus the "no behavioural loss" claim is scoped: it holds for the flag families
(always-enabled), while the component buttons intentionally keep their
`is_enabled = is_valid` write in the same loop. The removed component
`UpdateString64` label line (and its `attached` local) go to the LEGACY block per
section 6.

### 4.5 Alternative (full callback-sink symmetry) - documented, not chosen

If full source+sink symmetry is wanted in the C interim, wrap each family's click
in a `BindingSinkFn`:
`static bool StateManager_SinkEntityCapability(int flag_key, BindingValue v)`
that performs the same mutation and returns true, attach it as
`BIND_SINK_CALLBACK` with `write_key = spec->flag`, and route the click through
`Binding_WriteSink` instead of the `UIEventHandler`. This is behaviourally
identical and is the exact shape the XML migration will need. It is **not chosen
for the interim** because it adds a parallel set of fns and a click-routing change
for zero user-visible benefit; it is the natural first step of section 7.

Testability: the per-family query fns are pure functions of current selection +
flag bit - unit-testable by setting a selection and asserting the returned
`BindingValue`. The retirement of `UpdateFlagButtons` is verified by confirming
labels update via the refresh walk (integration).

---

## 5. BIND_STRING bespoke-format pattern (rpanel readouts - document only)

rpanel's multi-value readouts (`"%d/%d"` world index, `"%.0fx%.0f"` resolution,
`"N/A"` placeholders) are **out of scope to migrate**, but the design records the
uniform pattern so a later migration is mechanical and the mechanism is proven
consistent.

### 5.1 Pattern

A bespoke readout becomes a `BIND_SRC_QUERY` source whose query returns a
**preformatted `BIND_STRING`** `BindingValue`. No `format=` attribute and no core
formatting change: the query does the `snprintf` into stable storage and returns
a pointer to it. `UIElement_RefreshBinding` (textbox branch) ->
`Binding_RefreshText` -> `Binding_FormatValue`'s `BIND_STRING` case does the
bounded `strncpy` into the widget buffer.

```c
// World index "%d/%d". NOTE: BindingValue.as.s is NON-OWNING - the core copies it
// during refresh via Binding_FormatValue's bounded strncpy. The query MUST return a
// pointer to STABLE storage (a static or panel-state String64), NEVER a stack local,
// or the copied bytes are garbage.
static BindingValue RPanel_QueryWorldIndex(int key)
{
    (void)key;
    static String64 buf;   // stable for the duration of the refresh copy
    World2d *w = GetSelectedWorld();
    if (w && GetWorldCount() > 0)
        UpdateString64(buf.string, "%d/%d", GetSelectedWorldIndex() + 1, GetWorldCount());
    else
        UpdateString64(buf.string, "%s", "0/0");
    return (BindingValue){ .type = BIND_STRING, .as.s = buf.string };
}
```

The same shape covers resolution (`"%.0fx%.0f"`) and the `"N/A"` empty-selection
cases - the query returns the placeholder string. A `BIND_NONE` return would
instead leave the last-good display untouched (per `Binding_RefreshText`), which
is the right choice only when "no update" is wanted rather than "show N/A".

### 5.2 Non-owning lifetime note (mandatory in every such query)

Every bespoke STRING query MUST document, at its definition, that `as.s` is
non-owning and must point at a `static`/panel-state `String64`, never a stack
local. This is the single footgun of the pattern and is called out so each future
migration carries the warning.

This section is **documentation of the target pattern only**; rpanel's
`DrawRPanel` hand-formatting is left intact in this change.

---

## 6. Revert-safety handling (state_manager_system.c ONLY)

Any code **removed** from `src/engine/system/ui/state_manager_system.c` during
this work MUST NOT be deleted outright. It is moved **verbatim** into a
commented-out block at the **bottom** of the file, under this header:

```c
// ============================================================================
// LEGACY (pre-binding-consistency) - retained for easy revert; see
// .agents/tasks/binding-consistency-design.md
// ============================================================================
```

The following removed code is copied **verbatim** into that block, each line
wrapped as an inert `//` comment so the block cannot affect the build, so that a
revert is a pure copy-back:

1. The retired `UpdateFlagButtons` function body (its full definition).
2. The old `CreateFlagButtons` signature/body **before** the `query` parameter was
   added (so the pre-change creation path is restorable).
3. The seven removed `UpdateFlagButtons(...)` **call-site lines**, each with its
   enclosing context comment: five in `RefreshAttributeView` (entity_role,
   entity_capability, entity_constraint, entity_status, collision_role_mask), one
   in `RefreshWorldView` (world), one in `RefreshCellView` (cell). The call lines
   are removed code too and the revert-safety rule requires verbatim preservation,
   so they are copied into the LEGACY block, not merely reconstructed from memory.
4. The single hand-rolled component label statement removed from
   `RefreshComponentsSection` - specifically the one `UpdateString64("%s: %s", ...
   attached ? "ON" : "OFF")` label write **and its now-unused `attached` local**
   (`bool attached = is_valid && (desc.components[...type] != NULL);`). These two
   lines are deleted from the live component loop and copied verbatim to the
   LEGACY block. The `s_sm_ui.comp_buttons[i].button->is_enabled = is_valid;`
   write STAYS in the live loop (not copied to LEGACY), and `desc` STAYS declared
   (still used by the portal/relation field rows below the loop). See the explicit
   before/after loop bodies in section 4.4.

This rule is **specific to `state_manager_system.c`**. Other files change
normally:

- `ui_loader.h` / `ui_loader.c`: resolver structs/typedefs added, `BuildButton`
  edited - normal edits (additive).
- `lpanel_system.c`: `LPanel_AttachToggleSources` and its call **deleted
  outright** (not under the rule); `LPanel_ResolveBinding` gains a `debug.*`
  branch; `LPanel_ResolveDebugOverlayName` added.
- `lpanel.xml`: `binding=` attributes added to toggle buttons.

No file in this change invokes git in any way.

---

## 7. Deferred / next steps (explicit)

- **rpanel and state-manager XML migration** (converting `InitRPanel` /
  `InitStateManagerSystem` to load `rpanel.xml` / `state_manager.xml`) is OUT OF
  SCOPE. When undertaken it will need: (a) the `resolve_action` resolver (section
  1.3-1.5) wired for each panel so flag/component buttons resolve a
  **callback sink** to the existing `Handle*FlagClick` / `HandleComponentToggleClick`
  logic (section 4.5's sink variant), and a `resolve_binding` returning the
  per-family **query source** by name; (b) read-only vs editable textbox
  expression (not currently in the loader); (c) the bespoke STRING query pattern
  (section 5) for rpanel's `"%d/%d"` / `"%.0fx%.0f"` readouts; (d) the section
  visibility gating (`RefreshGameplaySection` / `RefreshComponentsSection`) staying
  in the app.
- Renaming `UILoader_HandleCommandClick` to a sink-neutral name (it now also
  drives callback sinks) is cosmetic and deferred.

---

## 8. Invariants and ownership

- **Toggle display format**: owned by `UIElement_RefreshBinding` (`ui.c`) - the
  single `"<text>: ON/OFF"` composition. No panel reimplements it after this
  change.
- **Flag mutation semantics** (EXCLUSIVE role set vs INDEPENDENT bit toggle):
  owned by the per-family `Handle*FlagClick` handlers in `state_manager_system.c`
  (domain logic stays in the panel), preserved verbatim. In the C interim these
  handlers are the **direct `UIEventHandler` click path** and the attached
  `Binding.sink` is `BIND_SINK_NONE` (the `Binding` carries a query *source* for
  display only). The callback-sink wiring (section 4.5), where the mutation is
  reached through `Binding.sink` / `Binding_WriteSink`, applies ONLY after the XML
  migration; until then nothing routes the click through the sink.
- **Selection / clear-when-none policy**: owned by each panel's query fns (lpanel
  debug query, state-manager per-family queries, rpanel STRING queries) - the
  core never learns what "selected" means.
- **BIND_STRING lifetime**: the query owns stable storage; the core copies via a
  bounded `strncpy`. Enforced by convention + the mandatory per-query note
  (section 5.2) - the core cannot enforce it, so the owning layer (the query)
  does.
- **Resolver domain-freedom**: owned by the loader boundary - `UIBinding`/`UIAction`
  carry only fn pointers + opaque keys; all names live in app resolvers.

---

## 9. Explicit statements (per the brief)

- **Git is untouched** by this change. No commit, stage, branch, or any git
  operation is part of it.
- **No new XML tags** are added. A flag button / debug toggle stays
  `<Button type="simple">` with `binding=` (source) and optional `action=`
  (sink). No `<FlagButton>`, `<Toggle>`, `<FlagGrid>`, or `format=`.
- **The `Binding` struct is unchanged.** It already holds both source and sink;
  no structural edit to `include/ui/binding.h` or the core in `binding.c`.
- **The change is additive with no regression.** `UIBinding` grows (old members
  keep name/position); `resolve_command` and all existing resolver callers keep
  working; `BuildTextField` is unchanged; buttons without `binding=` stay
  sink-only; buttons without `action=` become source-only read labels; buttons
  with neither get no Binding. lpanel's displayed output is identical;
  state-manager's flag/component label **value** is identical - it is now
  recomputed every frame by the query pull rather than only on
  `MarkStateManagerRefreshDirty`, which changes only *when* the text is written,
  never *what* text is shown (component attachment is read via the same
  `EntityRegistry_Describe` expression as before, section 4.2).

---

## 10. Files touched (summary)

| File | Change |
|------|--------|
| `include/system/ui/ui_loader.h` | Grow `UIBinding` (kind + query payload); add `UIAction`, `UIBindingSourceKind`, `UIBindingSinkKind`, `UIActionResolver`; add `resolve_action` to `UILoaderContext`. Additive. |
| `src/engine/system/ui/ui_loader.c` | `BuildButton` reads `binding=` -> source and `action=` -> sink (command or callback) into one Binding; inline `DataType->BindingValueType` map; new sink resolution order. |
| `src/engine/system/ui/lpanel_system.c` | `LPanel_ResolveBinding` gains `debug.*` -> query source; add `LPanel_ResolveDebugOverlayName`; **delete** `LPanel_AttachToggleSources` + its call. Keep `LPanel_QueryDebugEnabled`. |
| `src/engine/ui/components/lpanel.xml` | Add `binding="debug.*"` to each toggle `<Button>`. |
| `src/engine/system/ui/state_manager_system.c` | Add per-family `BIND_INT` is-set query fns + `StateManager_QueryComponentAttached` (via `EntityRegistry_Describe(o).components[type] != NULL`, the same expression the old label loop used); `CreateFlagButtons` gains a `query` param and attaches a query-source `Binding`; the `comp_buttons` loop attaches its own query-source `Binding` keyed on `type`; `RefreshComponentsSection`'s single component loop is edited in place to keep only its `is_enabled = is_valid` write (the `attached` local + `UpdateString64` label line removed), `desc` stays (portal/relation rows still use it); **retire** `UpdateFlagButtons`, its 7 call sites, the old `CreateFlagButtons` body, and the one component-label line into the LEGACY commented block (revert-safety rule). |

Nothing in `binding.h`, `binding.c`, `ui.c` (`UIElement_RefreshBinding`),
`view_host_system.c`, `integration_system.c`, `command_system.c`, or
`rpanel_system.c` is modified.

---

## 11. Responses to design review (revision)

Review: `.agents/tasks/binding-consistency-design-review.md` (verdict
CHANGES_REQUESTED; HIGH 0, MEDIUM 5, NIT 3). Every finding is addressed below.

- **Finding 1 [MEDIUM] - "designated-order init" claim wrong; struct put `kind` in
  slot 0.** ADDRESSED. Section 1.3 now places `address`/`data_type` in slots 0/1
  and APPENDS `kind`/`query`/`query_key`/`value_type` after `data_type`. The
  wording is corrected to state the init is **positional** and that the append-only
  layout is a hard requirement (reordering would break the positional
  `{NULL, FLOAT}` init). Verified against `lpanel_system.c`:
  `UIBinding binding = {NULL, FLOAT};`.
- **Finding 2 [MEDIUM] - implicit "non-NULL address + kind NONE => address source"
  shim coupled to struct order.** ADDRESSED. Section 1.4 now explicitly ties the
  shim to the slot-0/slot-1 layout from Finding 1: an unmodified resolver returns
  `{address, data_type, kind = 0 (zero tail)}`, so the shim is sound; a reorder
  would make it fire on garbage. The dependency is stated so the implementer does
  not reorder the struct.
- **Finding 3 [MEDIUM] - component buttons not built via `CreateFlagButtons`;
  "same treatment" under-specified.** ADDRESSED. Section 4.4 now specifies the
  component `Binding` attachment explicitly in the `comp_buttons` creation loop
  (`BIND_SRC_QUERY`, `query = StateManager_QueryComponentAttached`,
  `query_key = (int)type`), and states that the `CreateFlagButtons` `query`
  parameter does NOT reach these buttons. Verified the separate struct
  (`StateManagerComponentButton`) and inline loop in `state_manager_system.c`.
- **Finding 4 [MEDIUM] - `StateManager_QueryComponentAttached` read path
  unspecified / differs from bitmask queries.** ADDRESSED. Section 4.2 now gives
  the full query body using `EntityRegistry_HasComponent(o->id, type)` (NOT a
  `field & key` bitmask), and section 4.4 resolves the component buttons'
  per-frame `is_enabled = is_valid` by keeping an explicit enable pass in
  `RefreshComponentsSection`. Verified attachment is `desc.components[type]` /
  `EntityRegistry_HasComponent`, not a bitmask.
- **Finding 5 [MEDIUM] - "no behavioural loss" wrong for component buttons.**
  ADDRESSED. Section 4.4 now scopes the "no behavioural loss" claim to the flag
  families (which set `is_enabled = true` unconditionally) and documents that the
  component buttons keep a per-frame `is_enabled = is_valid` enable pass, so their
  disabled-when-no-selection behaviour is preserved exactly.
- **Finding 6 [NIT] - invariant table could imply flag-button sink carries the
  mutation in the C interim.** ADDRESSED. Section 8's "Flag mutation" bullet now
  states that in the C interim the `Handle*FlagClick` `UIEventHandler`s are the
  direct click path, `Binding.sink` is `BIND_SINK_NONE`, and callback-sink wiring
  (4.5) applies only after the XML migration.
- **Finding 7 [NIT] - revert-safety omits the `UpdateFlagButtons` call-site lines.**
  ADDRESSED. Section 6 now lists the removed call-site lines (five in
  `RefreshAttributeView`, one in `RefreshWorldView`, one in `RefreshCellView`) as
  verbatim copies into the LEGACY block, so a revert is a pure copy-back. (Note:
  the review's "six" is refined to the seven actual call sites verified in source.)
- **Finding 8 [NIT] - `have_sink` needs an explicit definition.** ADDRESSED.
  Section 2 now defines `have_sink` as true only when the sink resolved to
  `BIND_SINK_COMMAND` or `BIND_SINK_CALLBACK`; a source-only button has
  `have_sink == false` and `handler == NULL`, equivalent to today's
  `command_code != 0` behaviour.

### Round 2 responses

Review: `.agents/tasks/binding-consistency-design-review.md` (revision 2 review;
verdict CHANGES_REQUESTED; HIGH 0, MEDIUM 2, NIT 3). Every finding addressed below.

- **Finding 1 [MEDIUM] - component query parity assumed, not shown.** ADDRESSED
  by the review's preferred option (a). Section 4.2's
  `StateManager_QueryComponentAttached` now reads via the SAME expression the
  retired label loop used - `EntityRegistry_Describe(o).components[type] != NULL` -
  so parity is a textual identity, not an argued equivalence. The equivalence to
  `EntityRegistry_HasComponent` is additionally cited from `entity_registry.c`
  (`Describe.components[type]` is filled by `EntityRegistry_GetComponent(id,type)`
  for `type` in `1..ENTITY_COMPONENT_HEALTH`; `HasComponent` is
  `GetComponent(...) != NULL`), confirming the two were always equal for the four
  button types - but the describe form is used so parity cannot drift.
- **Finding 2 [MEDIUM] - single component loop split under-specified.** ADDRESSED.
  Section 4.4 now shows the CURRENT single loop (three statements) and the LIVE
  post-migration loop (only `if (!button) continue;` + `is_enabled = is_valid;`)
  explicitly, states the loop is edited IN PLACE (not replaced, not deleted, no
  new loop added), that the `attached` local and the `UpdateString64` label line
  are the only removals, and that `desc` stays declared (used by the portal/
  relation rows below). Section 6 item 4 is tightened to say the single
  `UpdateString64` label statement AND its `attached` local go verbatim to LEGACY
  while the `is_enabled = is_valid` write stays live and `desc` stays declared.
- **Finding 3 [NIT] - "byte-for-byte identical" vs "more responsive" tension.**
  ADDRESSED. Section 9 now qualifies the state-manager claim to: the label VALUE
  is identical; it is recomputed every frame by the pull rather than on dirty,
  changing only WHEN the text is written, never WHAT text is shown.
- **Finding 4 [NIT] - `UILoader_HandleCommandClick` name is a readability trap for
  callback sinks.** ADDRESSED (optional fix applied). Section 2 now instructs the
  implementer to add a one-line comment at `BuildButton`'s callback-sink branch
  pointing at the section 7 rename deferral.
- **Finding 5 [NIT] - `debug.*` branch placement vs the physics dot-parse.**
  ADDRESSED (optional fix applied). Section 3.2 now states the `debug.*` branch
  sits at the top of `LPanel_ResolveBinding`, returns before the existing
  dot-parse, and does not interact with the `component_len`/`dot_pos` logic.
