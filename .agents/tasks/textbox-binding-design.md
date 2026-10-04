# Design: Integrating Textboxes into the Symmetric Binding System (Hybrid Model)

## Overview

The symmetric binding core (`include/ui/binding.h`, `src/engine/ui/binding.c`) already drives buttons: it can read a value from a source, format it to text, parse text back to a value, validate it, and write it to a sink. Textboxes, however, still live on two older paths - the per-frame read path `RefreshTextboxFields` in `integration_system.c`, and the commit path where `HandleTextCommit` in `ui_input.c` calls `Binder_ValidateAndWrite` (defined in `binding.c`). This design brings textboxes onto the one binding abstraction so a single `Binding *binding` on `TextBoxData` is the bidirectional carrier: its `source` drives the per-frame display read, its `sink` drives the commit write.

The user chose a **hybrid** model, and the single most important constraint across this whole change is that the **generic binding core carries no domain knowledge**. The core knows only how to READ a source, FORMAT a value, PARSE text, and WRITE a sink. Everything domain-specific - which address, which query, what "selected object" means, when the target changes, and the clear-when-nothing-selected policy - lives in the individual UI systems (`lpanel_system.c`, `rpanel_system.c`, `state_manager_system.c`). The two shapes a panel can choose per-field are:

- **STABLE** targets (fixed addresses that never move - e.g. lpanel's entity-create editor fields bound to `G_UIState.entity_create_params->physics.*`): use `BIND_SRC_ADDRESS` / `BIND_SINK_ADDRESS`. A plain address, no per-field function.
- **DYNAMIC / selection-driven** targets (e.g. state_manager `object ? &object->field : NULL`, rpanel world/selection fields): use a panel-supplied **query** function (read) and a panel-supplied **callback** function (write). The "which address right now / what is selected / clear when none" policy lives entirely in the panel; the core just calls the function.

The core supports both; each panel chooses per field.

Technology stack (locked once approved): C11, raylib, the existing in-house memory pool (`memory/cmemory.h`), and the existing symmetric binding core. No new libraries, no new allocators, no new build targets. UK English spelling throughout code comments (colour, unrecognised, behaviour).

**Git is untouched by this work. No commits, no staging, no branch or worktree changes are made as part of implementing this design.** The user has stated they will commit themselves.

---

## 1. Generic core extension: query source returns a `BindingValue`

### Current state

`binding.h` defines `typedef int (*BindingQueryFn)(int key);` and `Binding_ReadSource`'s `BIND_SRC_QUERY` branch hardcodes an INT result:

```c
case BIND_SRC_QUERY:
    if (!src->query) return value;      // BIND_NONE
    value.as.i = src->query(src->query_key);
    value.type = BIND_INT;
    return value;
```

This can only yield INT, so FLOAT / VECTOR2D / STRING dynamic reads are impossible - which blocks the dynamic textbox path entirely.

### Change

Generalise the query so it yields a full `BindingValue`:

```c
// OLD
typedef int (*BindingQueryFn)(int key);
// NEW
typedef BindingValue (*BindingQueryFn)(int key);
```

`Binding_ReadSource`'s query branch becomes a straight pass-through - the query decides the type, the core does not interpret it:

```c
case BIND_SRC_QUERY:
{
    if (!src->query)
    {
        return value; // BIND_NONE
    }
    return src->query(src->query_key); // query owns the type; core never interprets it
}
```

Note `src->value_type` is now advisory for a query source (the query is authoritative about the returned type); it remains meaningful for `BIND_SRC_ADDRESS`. This carries no domain knowledge: the core still only knows "call this fn with this opaque int key and take whatever `BindingValue` it returns".

The existing struct-field comment in `binding.h` must be updated to match the new contract. It currently reads `BindingValueType value_type; // how to interpret the address / query result`; change it to:

```c
BindingValueType value_type; // address: how to interpret the deref; query: advisory only (the query owns the returned type)
```

This is the only `binding.h` struct-comment edit required by the source-side change (NIT finding 6).

### Migrating the existing INT query (behaviour preserved exactly)

`LPanel_QueryDebugEnabled` in `lpanel_system.c` (verified: the sole definition at ~line 246) is the only current `BindingQueryFn`. It becomes a trivial `BindingValue`-returning shim:

```c
// Query shim matching BindingQueryFn. The core stays decoupled from the debug subsystem:
// it only knows "call this fn with this int key". The key is a DebugOverlayId; the shim is a
// thin adapter over IsDebugEnabled that returns the on/off state as a BindingValue INT.
static BindingValue LPanel_QueryDebugEnabled(int overlay_key)
{
    return (BindingValue){ .type = BIND_INT, .as.i = IsDebugEnabled((DebugOverlayId)overlay_key) ? 1 : 0 };
}
```

### Lockstep edit sites (exactly one of each)

The signature change touches **three** sites that must move together, and no more (verified by grep - there is exactly one definition and one assignment of this query):

1. The typedef in `binding.h`: `typedef int (*BindingQueryFn)(int key);` -> `typedef BindingValue (*BindingQueryFn)(int key);`
2. The `BIND_SRC_QUERY` branch of `Binding_ReadSource` in `binding.c` (the pass-through rewrite above).
3. The single shim **definition** `LPanel_QueryDebugEnabled` AND its single **assignment** `binding->source.query = LPanel_QueryDebugEnabled;` in `lpanel_system.c` (verified at ~line 274, inside `LPanel_AttachToggleSources`). The assignment continues to type-check only because the shim's new return type matches the new typedef; leaving a stale `int`-returning definition would break the build. These are the same function, so changing the definition is the whole of the lpanel-side change - the assignment line itself is textually unchanged. The `query_key = (int)(code - CMD_TOGGLE_DEBUG_DASHBOARD)` assignment (`lpanel_system.c` ~line 275) that produces the `DebugOverlayId` the shim casts back is **also textually unchanged**; only the shim's return type and body change, so a reader should not expect to edit the `query_key` line.

`IsDebugEnabled` is declared `int IsDebugEnabled(DebugOverlayId)` and the current shim returns that raw `int` directly (`return IsDebugEnabled((DebugOverlayId)overlay_key);`). The new shim normalises it with `? 1 : 0`. This is a **deliberate** normalisation, not an identity rewrite: `IsDebugEnabled` may return any non-zero int for "enabled", and the `? 1 : 0` pins it to a canonical 0/1. It is behaviour-preserving for the button branch, which only tests `value.as.i != 0` (verified in `ui.c`), so ON/OFF presentation stays byte-identical.

`UIElement_RefreshBinding`'s button branch is otherwise unchanged in behaviour: it reads the source, treats a non-`BIND_NONE` INT as a boolean (`value.as.i != 0`), and composes `"<text>: ON/OFF"` from `button.text` into `button.display_text`. Because the shim still returns `BIND_INT`, the toggle presentation is preserved. No other `BindingQueryFn` exists in the tree (verified by grep), so this is the complete migration of the generalised signature.

---

## 2. Generic core extension: symmetric SINK CALLBACK for dynamic write-back

### Rationale

The query source is a panel-supplied read function. Its write-side mirror is a panel-supplied store function. The core already has `BIND_SINK_ADDRESS` (store to a fixed address) and `BIND_SINK_COMMAND` (dispatch an opaque command code). We add a third sink kind that hands a committed `BindingValue` to a panel function, which decides where it lands right now.

### Chosen shape (consistent with the existing structs)

Mirror the source side exactly. The source has `BindingQueryFn query; int query_key;`; the sink gains `BindingSinkFn write; int write_key;`:

```c
typedef enum BindingSinkKind
{
    BIND_SINK_NONE = 0,
    BIND_SINK_ADDRESS,
    BIND_SINK_COMMAND,
    BIND_SINK_CALLBACK,   // NEW: panel-supplied store for dynamic targets
} BindingSinkKind;

// Opaque write callback: the core hands over a validated BindingValue plus the opaque key.
// Returns true if the panel stored it, false to reject (caller reverts). Mirrors BindingQueryFn.
typedef bool (*BindingSinkFn)(int key, BindingValue value);

typedef struct BindingSink
{
    BindingSinkKind kind;
    BindingValueType value_type; // address sink: type to write; command sink: ignored; callback sink: parse type
    void *address;               // BIND_SINK_ADDRESS
    ValidatorFn validator;       // optional, same contract as the Binder validators
    void *validator_ctx;
    BindingCommandFn command;    // BIND_SINK_COMMAND
    int command_code;
    BindingSinkFn write;         // NEW: BIND_SINK_CALLBACK: write(write_key, value)
    int write_key;               // NEW: opaque key for the write callback
} BindingSink;
```

The existing field comment is `// address sink: type to write; command sink: ignored`. Keep the "command sink: ignored" clause verbatim - it is accurate, because `Binding_WriteSink`'s command branch never reads `value_type` (verified in `binding.c`). Only the new callback clause is appended, so the final comment is `// address sink: type to write; command sink: ignored; callback sink: parse type`. The callback is the only new consumer of `value_type` on the sink side (NIT finding 7).

**Why `int key` + `BindingValue` and not a `void *ctx`:** this is deliberately symmetric with `BindingQueryFn(int key)`. The key is opaque to the core exactly as `query_key` is; a panel keys on whatever it likes (a field enum, an index). Keeping the two sides identical in shape means a reviewer reasons about read and write the same way, and the core needs no new concept.

`Binding_WriteSink` gains the branch (the only change to that function):

```c
case BIND_SINK_CALLBACK:
{
    if (!sink->write)
    {
        return false; // no store fn => reject, caller reverts
    }
    return sink->write(sink->write_key, value);
}
```

`Binding_Commit` is unchanged in shape: it parses with `sink.value_type` + `sink.validator` + `sink.validator_ctx`, then calls `Binding_WriteSink`. For a callback sink the parse/validate step is identical to an address sink; only the final store differs. This keeps validation (and therefore the byte-identical INT/FLOAT landing, section 6) uniform across sink kinds.

The callback carries no domain knowledge into the core: the core never knows what the panel does with the value, only that it returned true/false.

---

## 3. `BIND_NONE` source = "leave the display unchanged" (clear-when-unselected)

`Binding_RefreshText` already returns `false` on a `BIND_NONE` read without touching the output buffer:

```c
BindingValue v = Binding_ReadSource(&b->source);
if (v.type == BIND_NONE) return false; // output buffer untouched
```

This is preserved. It is the generic mechanism for clear-when-unselected: a dynamic panel's query returns `{ .type = BIND_NONE }` when nothing is selected, and the core simply leaves the last-good display in place. The *policy* ("nothing selected" / "N/A") lives in the panel's query function; the core only sees a type-NONE value and declines to write. No selection concept leaks into the core.

A panel that wants an explicit "N/A" string on empty selection owns that too: it either formats the field itself before the refresh, or returns a `BIND_STRING` value pointing at a static `"N/A"`. The core treats that as an ordinary string read. This matches the existing `empty_text`/`"N/A"` behaviour in `RefreshTextboxFields` without the core knowing what "N/A" means.

**String lifetime (required):** `BindingValue.as.s` is non-owning, but `Binding_FormatValue`'s STRING case does a bounded `strncpy` into the caller's buffer *during* the single refresh call (verified in `binding.c`), so the pointed-to storage only has to outlive that one call. A string literal or a `static` buffer qualifies; a stack-local buffer declared inside the query does **not** - returning a pointer into it is a dangling-pointer bug. Query authors returning a `BIND_STRING` must point at literal/static storage.

---

## 4. `TextBoxData` field + dispose

### Field

Add `Binding *binding` to `TextBoxData` in `include/ui/ui.h`:

```c
typedef struct
{
    String64 text;
    DataType data_type;
    void *data_bind;   // retained: legacy path still uses it
    Binder *binder;    // retained: legacy path still uses it
    Binding *binding;  // NEW: single bidirectional carrier (source=read, sink=write)
    Bitmap_Font font;
    int cursor_position;
} TextBoxData;
```

`binder`, `data_bind`, and `binding` coexist. The migration is additive: a migrated field uses `binding`; an unmigrated field keeps using `binder`/`data_bind`. UI elements come from the zeroing pool (`PoolCreate(sizeof(UIElement), 2048)`, `PoolAlloc` does `MemorySet` 0), so `binding` defaults to `NULL` safely for every untouched textbox - no constructor change required.

### Dispose

`DisposeUIElement` in `src/engine/ui/ui.c` already frees the textbox `binder` and the button `binding`. Add the textbox binding free, mirroring both:

```c
if (IsTextbox(e) && e->data.textbox.binder)
{
    Binder_Destroy(e->data.textbox.binder);
    e->data.textbox.binder = NULL;
}

// NEW: free a textbox's heap binding (mirrors the button binding free below and the binder free above).
// Pool-zeroing guarantees binding == NULL for untouched textboxes, so this is a no-op there.
if (IsTextbox(e) && e->data.textbox.binding)
{
    Binding_Destroy(&e->data.textbox.binding);
}

if (IsBtn(e) && e->data.button.binding)
{
    Binding_Destroy(&e->data.button.binding);
}
```

`Binding_Destroy(Binding **)` NULLs the caller pointer, so no explicit `= NULL` is needed after it.

---

## 5. Textbox read branch in `UIElement_RefreshBinding`

Add a textbox branch to `UIElement_RefreshBinding` in `src/engine/ui/ui.c`, after the existing button branch. (To be precise about the starting point: no textbox branch exists today - the current function has only a button branch followed by a deferral `NOTE` comment stating that textbox refresh still flows through `RefreshTextboxFields` and that textboxes "do not yet carry a Binding source". We are adding the branch and retiring that NOTE's premise, not reinstating previously-deleted code.)

```c
if (IsTextbox(e))
{
    const Binding *b = e->data.textbox.binding;
    // Skip unbound / source-less textboxes - they stay on the legacy RefreshTextboxFields path.
    if (!b || b->source.kind == BIND_SRC_NONE)
    {
        return;
    }

    // CRITICAL invariant: never overwrite text the user is actively editing. This skip used to
    // live in RefreshTextboxFields and must be preserved here now that the pull drives textboxes.
    if (e->is_focused)
    {
        return;
    }

    // Read/display direction: source -> format -> text buffer. A BIND_NONE read (dynamic query
    // returning "nothing selected") leaves the last-good display untouched (section 3).
    Binding_RefreshText(b, e->data.textbox.text.string, sizeof(e->data.textbox.text.string));
    return;
}
```

This branch is reached every frame by the existing `PanelSystem_RefreshBindings` walk in `panel_system.c`, which recurses through `ForEachChild` calling `UIElement_RefreshBinding` on **every** element before layout/draw. Note the walk is unconditional - it does **not** skip disabled subtrees (verified: `PanelSystem_RefreshBindings` has no `is_enabled` guard; it recurses the whole `panel->root` tree). This fact matters for the lpanel stable migration (section 7) and is the reason the stable binding must be attached/detached by panel policy rather than relying on the walk to skip inactive views.

For a textbox the added per-frame cost is a NULL check (plus a format only for bound, unfocused textboxes).

### Doc-comment updates (BOTH must change together)

Two comments currently document the old textbox-defers-to-`RefreshTextboxFields` behaviour and must be brought into agreement so they do not rot:

1. The inline `NOTE` at the tail of `UIElement_RefreshBinding` in `ui.c` (verified present) - replace it with a note that migrated textboxes refresh here via the pull while unmigrated ones remain on the explicit `RefreshTextboxFields` path.
2. The declaration doc comment on `UIElement_RefreshBinding` in `include/ui/ui.h` (verified present) - it currently ends "(Textbox read-refresh still uses the RefreshTextboxFields path.)" and documents button-only behaviour. Update it to state that a textbox with a readable binding source refreshes via this pull (skipped while focused), and that textboxes without a binding stay on `RefreshTextboxFields`.

Leaving either stale while updating the other is a doc-rot bug; both edits are part of this change.

### Edge cases

- **Unbound textbox:** `binding == NULL` -> early return (identical to a button with no binding).
- **Focused textbox:** skipped before any write; the user's in-progress text is never clobbered.
- **Dynamic query returns NONE (nothing selected):** `Binding_RefreshText` returns false, buffer untouched, last-good text remains. If the panel wants "N/A", it supplies that via its query (section 3).
- **STRING source into a `String64`:** `Binding_FormatValue`'s STRING case does a bounded `strncpy` into `out_bytes - 1` with explicit NUL; `out_bytes` here is `sizeof(String64)`, so it cannot overflow the textbox buffer.

---

## 6. Commit (write) direction: migrate `HandleTextCommit` to `Binding_Commit`

### Current

`HandleTextCommit` in `src/engine/ui/ui_input.c`:

```c
if (!IsEditableTextbox(element) || !element->data.textbox.binder)
{
    RevertTextChanges(element, tbox_buffers);
    return;
}
if (!Binder_ValidateAndWrite(element->data.textbox.binder, element->data.textbox.text.string))
{
    RevertTextChanges(element, tbox_buffers);
    return;
}
ResetTextBuffers(tbox_buffers);
```

### Change (additive - binding first, legacy fallback preserved)

Prefer the new binding when present; fall back to the legacy binder when not. The revert-on-failure behaviour is preserved exactly in both branches:

```c
if (!IsEditableTextbox(element))
{
    RevertTextChanges(element, tbox_buffers);
    return;
}

const char *text = element->data.textbox.text.string;

if (element->data.textbox.binding)
{
    // Migrated path: parse -> validate -> sink-write through the symmetric core.
    if (!Binding_Commit(element->data.textbox.binding, text))
    {
        RevertTextChanges(element, tbox_buffers);
        return;
    }
}
else if (element->data.textbox.binder)
{
    // Legacy path for fields not yet migrated. Additive - do not break unmigrated callers.
    if (!Binder_ValidateAndWrite(element->data.textbox.binder, text))
    {
        RevertTextChanges(element, tbox_buffers);
        return;
    }
}
else
{
    // Neither carrier: nothing to commit to.
    RevertTextChanges(element, tbox_buffers);
    return;
}

ResetTextBuffers(tbox_buffers);
```

### Byte-identical guarantee

`Binder_ValidateAndWrite` is already a thin adapter over the symmetric core (see `binding.c`): it builds a `BIND_SINK_ADDRESS` sink with `value_type = b->type`, `address = b->target`, `validator = b->validator`, `validator_ctx = b->user_data`, then calls `Binding_ParseText` + `Binding_WriteSink`. `Binding_Commit` does the identical sequence from `b->sink`. Therefore, for a stable migrated field whose sink is `BIND_SINK_ADDRESS` with the same `value_type`, `address`, and validator, the committed bytes land at the identical address with identical content:

- **INT:** `Binding_ParseText` uses the validator if present (same `ValidatorIntRange`/`ValidatorIntPositive`), else `strtol`; `Binding_WriteSink` does `*(int*)addr = v.as.i`. Identical to the binder path.
- **FLOAT:** validator or `strtof`; `*(float*)addr = v.as.f`. Identical.
- **VECTOR2D:** `ParseVector2d` (same accepted formats); `*(Vector2d*)addr = v.as.v`. Identical.
- **STRING:** `Binding_WriteSink`'s STRING case keeps the historic 255-cap: `strncpy(addr, s, 255); ((char*)addr)[255] = '\0';` - preserved byte-for-byte.

Validated INT/FLOAT therefore land identically because both paths run the same validator against the same scratch member and store the same union member to the same address. The only difference for a callback-sink dynamic field is the final store target (the panel's `write` fn instead of a raw address), which is exactly the intended dynamic indirection; parse and validate are unchanged.

---

## 7. lpanel STABLE migration (first proof of the stable path)

The lpanel entity-create editor fields in `lpanel_system.c` (`InitLPanelEditView`, the `edit_specs` block) are bound to `G_UIState.entity_create_params->physics.*`. These addresses are fixed for the lifetime of `entity_create_params`, so they are the clean first proof of the stable path: address source + address sink, no panel function.

### The stable binding helper

Rather than hand-build ten bindings, add a reusable helper next to `BindTextboxData` in `integration_system.c`:

```c
// Build a stable (fixed-address) bidirectional binding: source and sink both target the same
// address, interpreted per data_type. No panel function - the address never moves. Attaches it
// to the textbox's binding slot (freed by DisposeUIElement). Validator-free by design.
void BindTextboxStable(UIElement *textbox, DataType type, void *address, int precision);
```

It maps `DataType` to `BindingValueType` with the existing `ResolveBindingType`, builds a `Binding` with `source.kind = BIND_SRC_ADDRESS` and `sink.kind = BIND_SINK_ADDRESS` (both `value_type` from the map, both `address` the same pointer, `precision` as given), heap-allocates it with `Binding_Create`, and stores it in `textbox->data.textbox.binding`. If a binding already exists it is updated in place (same pattern `BindTextboxData` uses for `binder`). On a NULL textbox or an unresolvable type it is a no-op.

**`BindTextboxStable` is validator-free by design.** Its signature carries no `ValidatorFn`, and `UIFieldSpec` (verified in `ui_constructors.h`: it has only `label`, `type`, `size`, `data_type`, `target`, `text_target`, `data_bind` - no validator field) cannot supply one. The lpanel physics fields use no validator today (verified: `BindTextboxData` builds their binders with `validator = NULL`), so this exactly matches current behaviour. If a future stable field needs range validation it must be hand-wired (build the `Binding` directly with `sink.validator`/`sink.validator_ctx` set) rather than going through this helper; extending the helper to carry a validator - which would also require adding fields to `UIFieldSpec` - is explicitly out of scope for this pass. This removes the earlier ambiguous "attached here if the field needs them" claim: there is no mechanism for it and none is added.

### Chosen attach/detach mechanism: per-activation via `on_view_selected` (not `UIFieldSpec`)

"Stable" means the *address* never moves while `entity_create_params` is live - not that the field is always shown. `RefreshEntityEditorFields` in `ui_system.c` currently binds these fields to `(editor_active && params) ? &physics.<field> : NULL` (verified), i.e. it deliberately clears them when the draw view is inactive. That enable/clear policy is domain policy and must stay in the panel.

A critical source fact drives the choice between the two candidate mechanisms: **`PanelSystem_RefreshBindings` walks the entire panel tree every frame and does not skip disabled subtrees** (verified in `panel_system.c` - no `is_enabled` guard, see section 5). Therefore a construction-time stable binding attached once in `InitLPanelEditView` would be refreshed every frame even while the edit view is inactive, overwriting the textbox the policy wants left blank. A construction-time opt-in (a boolean on `UIFieldSpec`) cannot honour the clear-when-inactive policy on its own, so it is **rejected**.

The design therefore uses a **per-activation** mechanism and names the exact call site:

- The lpanel's view selector already runs a `ViewSelectionCallback on_view_selected` whenever the active view changes: `PanelSystem_SelectView` calls `selector->on_view_selected(selected_view)` (verified in `panel_system.c` ~line 652). This is the single existing hook for "a view became active/inactive"; no new per-frame hook is invented. **The wiring is at the lpanel init site, not in `PanelSystem_FinaliseInit`.** `ViewSelector` has exactly one callback field (`ViewSelectionCallback on_view_selected`; `panel_system.h`), and it is populated from the `selector_callback` argument passed to `PanelSystem_CreateStandard` at `lpanel_system.c` ~line 308 (which forwards it through `PanelSystem_CreateStandardViewSelector` -> `AllocatePanelViewSelector`). `PanelSystem_FinaliseInit` neither creates the selector nor registers a callback - it only selects the initial view and lays out (verified ~line 823). So the callback is chosen at the `PanelSystem_CreateStandard(...)` call, nowhere else.

- **The slot is already occupied - `LPanel_OnViewSelected` must wrap, not replace, the shared handler (chosen fix: review Option A).** Today the lpanel passes `PanelSystem_HandleViewSelected` as that `selector_callback` (verified `lpanel_system.c` ~line 308). `PanelSystem_HandleViewSelected` does the critical work `G_UIState.active_panel_view = view->type;` (verified `panel_system.c` ~line 41), and `RefreshEntityEditorFields` in `ui_system.c` reads `G_UIState.active_panel_view == LPANEL_DRAW_VIEW` to decide whether to refresh. Because the slot is singular, registering a brand-new `LPanel_OnViewSelected` into it would *silently drop* the `active_panel_view` update and break the rest of the system. Therefore `LPanel_OnViewSelected` **delegates to the shared handler first, then applies the attach/clear policy**, and is passed as the `selector_callback` *in place of* `PanelSystem_HandleViewSelected`:

```c
// lpanel view-selection hook. MUST keep the shared active_panel_view update working, so it
// delegates to PanelSystem_HandleViewSelected first, then applies lpanel's stable-binding policy:
// attach the fixed-address bindings when the DRAW (edit) view becomes active, clear them otherwise.
// Registered as PanelSystem_CreateStandard's selector_callback at the lpanel init site (~line 308),
// REPLACING the bare PanelSystem_HandleViewSelected argument - not added to a second slot.
static void LPanel_OnViewSelected(View *selected)
{
    PanelSystem_HandleViewSelected(selected); // preserve G_UIState.active_panel_view = view->type

    if (selected && selected->type == LPANEL_DRAW_VIEW)
    {
        // Attach the fixed-address stable bindings for the edit fields.
        BindTextboxStable(G_UIState.edit_width_tbox,  FLOAT, &G_UIState.entity_create_params->physics.width,  2);
        BindTextboxStable(G_UIState.edit_height_tbox, FLOAT, &G_UIState.entity_create_params->physics.height, 2);
        // ... remaining physics.* edit fields ...
    }
    else
    {
        // Any other view active: detach + clear so the pull shows nothing for the inactive editor.
        ClearTextboxBinding(G_UIState.edit_width_tbox);
        ClearTextboxBinding(G_UIState.edit_height_tbox);
        // ... remaining physics.* edit fields ...
    }
}
```

The only edit at the init site is changing the `selector_callback` argument from `PanelSystem_HandleViewSelected` to `LPanel_OnViewSelected` at `lpanel_system.c` ~line 308 (`PanelSystem_CreateStandard(&lpanel_viewport, 2, labels, ARRAY_COUNT(labels), LPanel_OnViewSelected, ...)`). No new field is added to `ViewSelector`; Option B (a second callback field) is rejected as a larger struct change for no benefit over delegation.

- Switching away from the edit view fires the same callback for the newly-selected view, so detach happens exactly when the edit view stops being active. This keeps the clear/show policy in the panel and keeps the stable binding a pure fixed-address binding.
- The `UIFieldSpec` struct is **not** changed and no opt-in boolean is added. `edit_specs` continues to be laid out by `InitUIFields`, which creates the textboxes; the stable bindings are attached afterwards by the activation callback, never at construction.

A tiny panel helper is added alongside `BindTextboxStable`:

```c
// Detach and free a textbox's stable binding and clear its text. Panel-side policy helper -
// used when the editor view goes inactive so the pull shows nothing (binding == NULL).
void ClearTextboxBinding(UIElement *textbox); // in integration_system.c, next to BindTextboxStable
```

(Rejected alternative: making the field a dynamic query returning `BIND_NONE` when inactive. This works but turns a genuinely stable address into the dynamic shape unnecessarily, blurring the two halves of the hybrid for a field whose address never moves.)

### Reconciling construction-time wiring (no double writer)

The `edit_specs` rows in `InitLPanelEditView` currently pass a non-NULL `data_bind` (e.g. `&G_UIState.entity_create_params->physics.width`), so `InitUIFields` -> `BindTextboxData` attaches a legacy `Binder` at construction (verified). If the stable `Binding` is attached by the activation callback on top of that, the field would carry **both** a `binder` and a `binding`. `HandleTextCommit` (section 6) prefers `binding`, so commits are unambiguous - but to avoid any read-side confusion and any lingering legacy carrier, **set `data_bind = NULL` on the migrated `edit_specs` rows** so `InitUIFields`/`BindTextboxData` creates no `Binder` for them (verified: `BindTextboxData` with `data_bind == NULL` calls `BindTextbox(textbox, NULL)` and attaches nothing). `BindTextboxStable` (via the activation callback) is then the **sole** carrier for every migrated field. This is the chosen reconciliation - option (a) from the review: no legacy `Binder` is created, rather than tolerating an inert one.

**`InitEntityCreateDefaults`'s seed writes are retained and are not a competing writer.** `InitEntityCreateDefaults` (`lpanel_system.c` ~line 212) seeds the edit textboxes once at setup with direct `WriteTextbox*` calls (e.g. `WriteTextboxFloat(G_UIState.edit_width_tbox, params->physics.width, 2)`) from the same `physics.*` values. These writes are kept: they are what populates the fields before the first pull frame, so without them a freshly-activated editor would show blank until the pull runs. They are not a conflicting second writer - they execute once at construction, whereas the per-frame pull reads the *same* `physics.*` addresses the seed came from, so the two always agree. (If the edit view happens to be active when a seed runs, the next pull frame simply re-reads the identical value - harmless.) After this migration the only writers of these textboxes are (1) `InitEntityCreateDefaults`'s one-time seed and (2) the stable binding's pull; the removed `edit_fields[]` rows and the NULL-ed `data_bind` ensure nothing else writes them.

Correspondingly, **remove the migrated fields' rows from `RefreshEntityEditorFields`'s `edit_fields[]` table** in `ui_system.c`. `RefreshTextboxFields` is driven by `ARRAY_COUNT(...)`, so deleting rows is self-consistent and the count follows automatically. Once the whole `edit_fields[]` table is migrated, `RefreshEntityEditorFields` has nothing left to refresh and may be removed along with its single call site in `UpdateGlobalUIState`; if any lpanel edit field is intentionally left on the legacy path in this pass, keep the function with only the remaining rows. Either way, no field is driven by both paths at once.

### Result

Once an lpanel edit field carries a stable binding (while the edit view is active):
- **Read:** the per-frame `UIElement_RefreshBinding` textbox branch (section 5) formats `entity_create_params->physics.<field>` into the textbox when unfocused. Because the migrated rows are gone from `edit_fields[]` and the `edit_specs` `data_bind` is NULL, there is exactly one writer (the pull via the stable binding). When the edit view is inactive the activation callback has freed the binding, so the pull is a no-op and the field is blank.
- **Write:** `HandleTextCommit` (section 6) commits through the binding's `BIND_SINK_ADDRESS`, landing at `&physics.<field>` byte-identically to the old binder.

The addresses never move while bound, so no query/callback is needed - this is exactly the stable half of the hybrid; only the attach/detach timing (panel policy, driven by `on_view_selected`) gates visibility.

---

## 8. One proven DYNAMIC field end-to-end in state_manager

The dynamic tables live in `state_manager_system.c` (`RefreshStateManagerFields`-style functions), e.g.:

```c
{s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL},
```

Here the bound address depends on the current selection and is `NULL` when nothing is selected. This is the hard half of the hybrid. We migrate **one** field end-to-end as proof - `damage` is the chosen field (a plain `FLOAT` on the selected object, no component indirection, representative of the common case) - with its query and callback living in `state_manager_system.c`. The remaining dynamic fields stay on `RefreshTextboxFields` for now (additive migration); the pattern below is documented so the user can migrate the rest.

### Panel-side query (read): where the "what is selected" policy lives

```c
// Dynamic read for the selected object's damage. Owns the selection policy: returns the live
// value when something is selected, BIND_NONE when nothing is - which the core treats as
// "leave the display unchanged" (clear-when-unselected). No selection concept leaks to the core.
static BindingValue StateManager_QueryDamage(int key)
{
    (void)key; // single field; key unused here (a multi-field panel would switch on it)
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!object)
    {
        return (BindingValue){ .type = BIND_NONE };
    }
    return (BindingValue){ .type = BIND_FLOAT, .as.f = object->damage };
}
```

### Panel-side callback (write): where "which address right now" lives

```c
// Dynamic write for the selected object's damage. Resolves the current target at commit time;
// rejects (returns false, caller reverts) when nothing is selected. The validated FLOAT lands
// byte-identically to a direct *(float*)&object->damage = value store.
static bool StateManager_WriteDamage(int key, BindingValue value)
{
    (void)key;
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!object || value.type != BIND_FLOAT)
    {
        return false; // nothing selected / wrong type => revert
    }
    object->damage = value.as.f;
    return true;
}
```

### Wiring

Attach the binding once at construction (when `s_sm_ui.damage_tbox` is created), not per frame:

```c
Binding damage_binding = {
    .source = { .kind = BIND_SRC_QUERY, .value_type = BIND_FLOAT,
                .query = StateManager_QueryDamage, .query_key = 0 },
    .sink   = { .kind = BIND_SINK_CALLBACK, .value_type = BIND_FLOAT,
                .write = StateManager_WriteDamage, .write_key = 0 },
    .precision = 2,
};
s_sm_ui.damage_tbox->data.textbox.binding = Binding_Create(damage_binding);
```

The designated initializer leaves `sink.validator`/`sink.validator_ctx` zero (NULL): the `damage` callback sink is **validator-free by design** (parse-only `strtof`), matching its current `RefreshTextboxFields` row, which attaches no validator. If a future dynamic field needs range checking it sets `sink.validator`/`sink.validator_ctx` in its own initializer; the core runs them identically for a callback sink and an address sink (section 6).

Remove the `damage_tbox` row from the `state_fields` `TextboxField` table so the two paths do not fight. There is no construction-time legacy `Binder` to reconcile here: `damage_tbox` is built from a `gameplay_specs` row with `data_bind == NULL` (verified in `state_manager_system.c`), so `InitUIFields` attaches no `Binder` to it. The new `binding` is therefore the sole carrier from the outset, and removing the one `state_fields` row leaves exactly one writer. The per-frame `UIElement_RefreshBinding` textbox branch now:
- reads `StateManager_QueryDamage` every frame -> when an object is selected, shows live damage (unless focused); when none is selected, `BIND_NONE` leaves the display unchanged.
- commits through `StateManager_WriteDamage` on ENTER/focus-loss -> writes to the currently selected object, reverts if selection vanished mid-edit.

### Why `RefreshTextboxFields` stays

Many callers still populate it (the rest of state_manager's fields, rpanel's world/create tables, `ui_system.c`'s entity-editor table for any still-legacy field). It is **not** deleted while callers remain. The migration is additive and incremental; the proven mechanism plus this documented pattern is what lets the user migrate the remaining dynamic fields field-by-field.

### rpanel note

rpanel's world/selection fields (`rpanel_world_gravity_edit_tbox` bound to `selected_world ? &selected_world->gravity : NULL`, etc.) are structurally identical to the state_manager dynamic case: a query returning the live value or `BIND_NONE`, and a callback resolving `Universe_GetSelectedWorld` at commit time. They stay on `RefreshTextboxFields` in this pass and follow the documented pattern when migrated. Their `"N/A"` empty text is reproduced by the query returning a `BIND_STRING` pointing at a static `"N/A"` (section 3) when no world is selected, if that display is wanted.

---

## Error handling (per operation that can fail)

- **`Binding_ReadSource` with NULL address or NULL query:** returns `BIND_NONE`. Recoverable, non-fatal. `Binding_RefreshText` then leaves the display unchanged. Not logged - this is the normal "nothing selected" case and would spam every frame.
- **`Binding_RefreshText` into the textbox:** a false return (NONE read or unknown type) is a no-op on the buffer. Recoverable, not logged (per-frame).
- **`Binding_Commit` parse failure (bad text):** returns false; `HandleTextCommit` calls `RevertTextChanges`, restoring the pre-edit snapshot. Recoverable, not logged (user typo, expected).
- **`Binding_Commit` validator rejection (out of range):** same as parse failure - false, revert. Recoverable, not logged.
- **`Binding_WriteSink` to a `BIND_SINK_CALLBACK` whose panel fn returns false (e.g. selection vanished mid-edit):** `Binding_Commit` returns false, `HandleTextCommit` reverts. Recoverable, not logged.
- **`BIND_SINK_ADDRESS` with NULL address / `BIND_SINK_CALLBACK` with NULL `write` fn:** `Binding_WriteSink` returns false -> revert. This indicates a wiring bug rather than user error; it is caught by the revert and is self-evident during development, so no runtime log is added (consistent with the existing core, which logs nothing).
- **`Binding_Create` allocation failure:** returns `NULL`; the textbox simply has no binding and falls back to the legacy path (or displays nothing). Non-fatal. Allocation failure in this pool-backed engine is not currently logged elsewhere; no new log is introduced for consistency.

The guiding rule: the core returns a bool for every fallible operation and never logs per-frame; the caller (textbox commit) translates false into a revert. This matches the existing button/binder behaviour exactly.

## Input validation (per external input)

The only external input is the committed textbox text:
- **Required/optional:** text is always present (the textbox buffer); an empty string is parsed per type and typically fails parse -> revert.
- **Type:** parsing is driven by `sink.value_type` (INT/FLOAT/VECTOR2D/STRING). A validator, when attached, enforces range/positivity.
- **Limits:** STRING writes are capped at 255 bytes with an explicit NUL (preserved). INT/FLOAT ranges are enforced by the existing `ValidatorIntRange`/`ValidatorFloatRange`/`ValidatorIntPositive` where a field attaches them.
- **Behaviour on failure:** revert to the pre-edit snapshot via `RevertTextChanges`; the bound data is never partially written (the single store happens only after a successful parse+validate).

## Invariants and ownership

- **Focused-edit protection** (never overwrite text the user is editing): owned by the UI layer in `UIElement_RefreshBinding`'s textbox branch (`if (e->is_focused) return;`). It lives here, not in the core, because "is this widget focused" is a UI concept the core does not model. Previously enforced in `RefreshTextboxFields`; now enforced on the pull path too.
- **Selection / clear-when-unselected policy:** owned entirely by the panel's query (read) and callback (write) functions. The core only observes `BIND_NONE` / a false callback.
- **Single-store-on-success** (no partial writes): owned by the core (`Binding_ParseText` writes a scratch value; the real store is one `Binding_WriteSink` call after success).
- **Binding heap ownership:** the owning `UIElement` owns its `binding`; `DisposeUIElement` frees it. The pool zeroes `binding` to `NULL` for untouched elements.
- **STRING 255-cap:** owned by `Binding_WriteSink`'s address-sink STRING case; preserved byte-for-byte.

## Testability

- **Unit testable (pure, no raylib/UI):** `Binding_ReadSource` with a query returning each `BindingValue` type; `Binding_WriteSink` with a `BIND_SINK_CALLBACK` (assert the callback receives the committed value and its false return is propagated); `Binding_Commit` end-to-end for INT/FLOAT/VECTOR2D/STRING including validator rejection and the 255-cap; `Binding_RefreshText` returning false on a `BIND_NONE` query without touching the buffer. These need only `binding.c` and a stub query/callback.
- **Byte-identity test:** commit the same text through `Binder_ValidateAndWrite` and through a `Binding_Commit` with an equivalent address sink; assert the target bytes are identical for INT/FLOAT/VECTOR2D/STRING. This directly guards the section 6 guarantee.
- **Integration testable (needs the UI tree + pool):** the focused-skip (`UIElement_RefreshBinding` must not write when `is_focused`); the per-frame pull refreshing a stable lpanel field; the dynamic state_manager `damage` field showing live value when selected and leaving the display unchanged when deselected; dispose freeing the binding without leaking (pool/leak check).
- The design is testable because the core stays pure and the domain policy is isolated behind two function pointers that a test can stub - a design that forced selection logic into the core would be far harder to test, which is one more reason the core stays domain-free.
- **Confirmed by review:** `Binding_RefreshText` returns false on a `BIND_NONE` read *before* calling `Binding_FormatValue`, so the output buffer is untouched (verified in `binding.c`). The clear-when-unselected / leave-last-good behaviour this design relies on (section 3) is therefore guaranteed by the existing core, not something this change must add.

---

## Responses to design-review findings

### Second review (current): CHANGES_REQUESTED - 1 HIGH, 1 MEDIUM, 3 NIT

From `design-review.json` / `design-review.md`. Every blocking (HIGH/MEDIUM) finding is addressed below; the NITs are addressed too. No finding is backlogged or ignored.

**Finding 1 (HIGH) - the `on_view_selected` hook is already occupied, and the wiring site was misdescribed.** Addressed in section 7 ("Chosen attach/detach mechanism"). Confirmed against source: `ViewSelector` has a single `on_view_selected` field (`panel_system.h`), already populated by `PanelSystem_HandleViewSelected` (which sets `G_UIState.active_panel_view = view->type`, `panel_system.c` ~line 41) passed as `PanelSystem_CreateStandard`'s `selector_callback` at `lpanel_system.c` ~line 308. Chose the review's **Option A**: `LPanel_OnViewSelected(View *)` now **delegates to `PanelSystem_HandleViewSelected` first** (preserving the `active_panel_view` update that `RefreshEntityEditorFields` depends on), then applies the attach/clear policy; it is passed as the `selector_callback` **in place of** the bare handler at the ~line 308 call. Rejected Option B (a second callback field on `ViewSelector`) as a needless struct change. Corrected the wiring prose: the callback is chosen at the `PanelSystem_CreateStandard(...)` call site, **not** registered inside `PanelSystem_FinaliseInit` (which only selects the initial view and lays out, ~line 823).

**Finding 2 (MEDIUM) - `InitEntityCreateDefaults` seed writes vs. the single-writer pull model.** Addressed in section 7 ("Reconciling construction-time wiring"). Added a paragraph stating `InitEntityCreateDefaults`'s one-time direct `WriteTextbox*` seed writes (`lpanel_system.c` ~line 212) are **retained** - they populate the fields before the first pull frame - and are **not** a competing writer: they run once and read from the same `physics.*` values the pull later reads, so the two always agree. Spelled out that after migration the only writers are the one-time seed and the stable binding's pull.

**Finding 3 (NIT) - Overview mislocates `Binder_ValidateAndWrite`.** Addressed in the Overview: reworded to "the commit path where `HandleTextCommit` in `ui_input.c` calls `Binder_ValidateAndWrite` (defined in `binding.c`)".

**Finding 4 (NIT) - `query_key` assignment guarantee not stated.** Addressed in section 1 lockstep edit sites: noted that `query_key = (int)(code - CMD_TOGGLE_DEBUG_DASHBOARD)` (`lpanel_system.c` ~line 275) is textually unchanged; only the shim's return type and body change.

**Finding 5 (NIT) - `damage` callback sink validator-free not stated.** Addressed in section 8 wiring: added that the `damage` callback sink is validator-free by design (parse-only `strtof`), matching its current `RefreshTextboxFields` row, and that a future validated dynamic field sets `sink.validator`/`sink.validator_ctx` in its own initializer.

### First review (resolved in the prior revision): CHANGES_REQUESTED - 2 HIGH, 3 MEDIUM, 4 NIT

Retained for the record; all findings were addressed in the previous pass.

**Finding 1 (HIGH) - `BindTextboxStable` wiring self-contradictory (construction-time opt-in vs. per-activation hook).** Addressed in section 7. Committed to the per-activation mechanism and dropped the `UIFieldSpec` opt-in boolean entirely. The decisive source fact is that `PanelSystem_RefreshBindings` walks the whole tree without skipping disabled subtrees (verified), so construction-time attach cannot honour clear-when-inactive and is rejected. (The call-site description was corrected in the second review's Finding 1 above.)

**Finding 2 (HIGH) - `BindingQueryFn` signature change: omitted lockstep assignment site and the `IsDebugEnabled` int->0/1 normalisation.** Addressed in section 1 under "Lockstep edit sites". Enumerated the exactly-three sites (typedef in `binding.h`, `Binding_ReadSource` branch in `binding.c`, the single shim definition + its single `source.query =` assignment at `lpanel_system.c` ~line 274). Stated that `IsDebugEnabled` returns a raw `int` and the shim's new `? 1 : 0` is deliberate 0/1 normalisation, behaviour-preserving under the button branch's `value.as.i != 0` test.

**Finding 3 (MEDIUM) - validator attachment for `BindTextboxStable` has no mechanism.** Addressed in section 7. Declared `BindTextboxStable` validator-free by design; removed the "attached here if the field needs them" claim and noted `UIFieldSpec` carries no validator field, the lpanel physics fields use none today, and a future validated stable field must be hand-wired - extending the helper/`UIFieldSpec` is explicitly out of scope.

**Finding 4 (MEDIUM) - construction-time `BindTextboxData`/legacy `Binder` not reconciled with the removed refresh rows.** Addressed in section 7 under "Reconciling construction-time wiring": set `data_bind = NULL` on the migrated `edit_specs` rows so no legacy `Binder` is created, making `BindTextboxStable` the sole carrier; and remove the migrated rows from `RefreshEntityEditorFields`'s `edit_fields[]`. Section 8 records that state_manager's `damage_tbox` has `data_bind == NULL` already, so no `Binder` reconciliation is needed there.

**Finding 5 (MEDIUM) - inaccurate "removed earlier" narrative and un-updated `ui.h` doc comment.** Addressed in section 5. Reworded to "add a textbox branch (none exists today; the current function has only a button branch plus a deferral NOTE)". Added an explicit instruction to update BOTH the `ui.c` inline NOTE and the `ui.h` declaration doc comment so they agree.

**Finding 6 (NIT) - `src->value_type` becoming advisory not reflected in the `binding.h` field comment.** Addressed in section 1: the comment is changed to `// address: how to interpret the deref; query: advisory only (the query owns the returned type)`.

**Finding 7 (NIT) - `BIND_SINK_CALLBACK` `value_type` role vs. "command sink: ignored".** Addressed in section 2: kept "command sink: ignored" verbatim (verified the command branch never reads `value_type`) and only appended the callback clause, so the comment reads `// address sink: type to write; command sink: ignored; callback sink: parse type`.

**Finding 8 (NIT) - query `BIND_STRING` storage lifetime.** Addressed in section 3: added the explicit rule that the pointed-to string must outlive the single refresh call (literal/static qualifies; a stack-local in the query does not).

**Finding 9 (NIT, verification-only) - `Binding_RefreshText` returns false on `BIND_NONE` without touching the buffer.** No change required; recorded as confirmed in both section 3 and the Testability section.
