# Design v2: Integrating Textboxes into the Symmetric Binding System (Hybrid Model)

## Overview

The symmetric binding core (`include/ui/binding.h`, `src/engine/ui/binding.c`) already drives buttons: it reads a value from a source, formats it to text, parses text back to a value, validates it, and writes it to a sink. Textboxes still live on two older paths - the per-frame read path `RefreshTextboxFields` in `src/engine/system/integration_system.c`, and the commit path where `HandleTextCommit` in `src/engine/ui/ui_input.c` calls `Binder_ValidateAndWrite`. This design brings textboxes onto the one binding abstraction so a single `Binding *binding` on `TextBoxData` is the bidirectional carrier: its `source` drives the per-frame display read, its `sink` drives the commit write.

The user chose a **hybrid** model, and the single most important constraint across this whole change is that the **generic binding core carries no domain knowledge**. The core (`binding.h`, `binding.c`, `UIElement_RefreshBinding` in `ui.c`, the `PanelSystem_RefreshBindings` walk in `panel_system.c`) knows only how to READ a source, FORMAT a value, PARSE text, and WRITE a sink. Everything domain-specific - which address, which query, what "selected object" means, when the target changes, clear-when-nothing-selected policy, visibility policy - lives in the individual UI systems (`rpanel_system.c`, `state_manager_system.c`). The two shapes a panel can choose per field are:

- **STABLE** targets (fixed addresses that never move) use `BIND_SRC_ADDRESS` / `BIND_SINK_ADDRESS`. A plain address, no per-field function. Proven here on rpanel's `create_fields` (next-world-to-create params from `GetNextWorld*Ptr()`).
- **DYNAMIC / selection-driven** targets use a panel-supplied **query** function (read) and a panel-supplied **callback** function (write). The "which address right now / what is selected / clear when none / visibility" policy lives entirely in the panel; the core just calls the function. Proven here on state_manager's `damage` field (`object ? &object->damage : NULL`).

The core supports both; each panel chooses per field.

Technology stack (locked once approved): C11, raylib, the existing in-house memory pool (`memory/cmemory.h`), and the existing symmetric binding core. No new libraries, no new allocators, no new build targets. UK English spelling in code comments (colour, unrecognised, behaviour).

**Git is untouched by this work. No commits, no staging, no branch or worktree changes are made as part of implementing this design.** Work is in-place on the currently checked-out `ui-overhaul` branch of the main worktree. No worktree is created. The user commits themselves.

### This is a v2 revision — relationship to the prior review

The v1 review (`.agents/tasks/design-review.md`) raised two HIGH and two MEDIUM findings against the prior design (`.agents/tasks/textbox-binding-design.md`). The core cause of both HIGH findings was that the prior design proved the STABLE path on **dead code**: `InitLPanelEditView` is only called from a commented-out fallback in `lpanel_system.c`, `lpanel.xml`'s DRAW view (and its `<TextField binding="physics.*">` rows) is commented out, and `G_UIState.edit_*_tbox` are never populated in the live XML path. This v2 corrects the scope entirely: lpanel edit-view migration is **out of scope** (documented as a pattern note, section 7), the STABLE path is proven on a **live** field (rpanel `create_fields`, section 8), and the DYNAMIC path on a **live** field (state_manager `damage`, section 9). The v1 review's MEDIUM/NIT fixes (single-writer rule, is_projectile visibility decision, NULL contracts, `ResolveBindingType` co-location) are folded in.

This document is itself a **v2 revision**: the v2 review (`.agents/tasks/design-review-v2.md` / `.json`) returned CHANGES_REQUESTED with one blocking MEDIUM finding — the migrated rpanel gravity field still carried a construction-time legacy `Binder` because `InitUIFields` builds one from the construction spec's non-NULL `data_bind`, which the earlier single-writer analysis (focused only on the per-frame refresh array) overlooked. That is now resolved in section 8 by NULLing the construction-spec `data_bind` in lockstep. The three v2 NITs (line drift, the section-9 "no legacy Binder" phrasing, and the attach-site line span) are also folded in. Responses to every finding from both reviews are tabulated at the end (section 11).

---

## 1. Generic core extension A: query source returns a `BindingValue`

### Current state (verified)

`binding.h` line 91: `typedef int (*BindingQueryFn)(int key);`. `Binding_ReadSource`'s `BIND_SRC_QUERY` branch in `binding.c` hardcodes an INT result:

```c
case BIND_SRC_QUERY:
{
    if (!src->query)
    {
        return value; // BIND_NONE
    }
    value.as.i = src->query(src->query_key);
    value.type = BIND_INT;
    return value;
}
```

This can only yield INT, so FLOAT / VECTOR2D / STRING dynamic reads are impossible - which blocks the dynamic textbox path for `damage` (a FLOAT).

### Change — exact deltas (three lockstep sites, no more)

The signature change touches exactly three sites, which must move together (verified by grep: exactly one typedef, exactly one `BIND_SRC_QUERY` branch, exactly one shim definition + one assignment).

**(a) `include/ui/binding.h` line 91 — the typedef:**

```c
// OLD
typedef int (*BindingQueryFn)(int key);
// NEW
typedef BindingValue (*BindingQueryFn)(int key);
```

Also update the `value_type` field comment in `BindingSource` (line ~96). It currently reads `// how to interpret the address / query result`; change it to:

```c
BindingValueType value_type; // address: how to interpret the deref; query: advisory only (the query owns the returned type)
```

**(b) `src/engine/ui/binding.c` — the `BIND_SRC_QUERY` branch of `Binding_ReadSource`:**

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

The core no longer sets `value.as.i`/`value.type`: it is a straight pass-through of whatever `BindingValue` the query returns. This carries no domain knowledge: the core still only knows "call this fn with this opaque int key and take whatever `BindingValue` it returns". `src->value_type` becomes advisory for a query source (the query is authoritative); it remains meaningful for `BIND_SRC_ADDRESS`.

**(c) `src/engine/system/ui/lpanel_system.c` — the sole existing shim + its assignment.** `LPanel_QueryDebugEnabled` (verified: definition at line 246, the only `BindingQueryFn` in the tree) becomes a trivial `BindingValue`-returning shim:

```c
// OLD (lpanel_system.c line 246)
static int LPanel_QueryDebugEnabled(int overlay_key)
{
    return IsDebugEnabled((DebugOverlayId)overlay_key);
}

// NEW
// Query shim matching BindingQueryFn. The core stays decoupled from the debug subsystem: it
// only knows "call this fn with this int key". The key is a DebugOverlayId; the shim is a thin
// adapter over IsDebugEnabled that normalises the on/off state to a canonical 0/1 BindingValue INT.
static BindingValue LPanel_QueryDebugEnabled(int overlay_key)
{
    return (BindingValue){ .type = BIND_INT, .as.i = IsDebugEnabled((DebugOverlayId)overlay_key) ? 1 : 0 };
}
```

The assignment `binding->source.query = LPanel_QueryDebugEnabled;` (verified at line 274, inside `LPanel_AttachToggleSources`) and the `binding->source.query_key = (int)(code - CMD_TOGGLE_DEBUG_DASHBOARD);` line (verified at line 276) are **textually unchanged** — only the shim's return type and body change. The assignment continues to type-check only because the shim's new return type matches the new typedef; a stale `int`-returning definition would break the build.

### Behaviour preserved exactly

`IsDebugEnabled` is declared `int IsDebugEnabled(DebugOverlayId)` and may return any non-zero int for "enabled". The old shim returned that raw int; the new shim normalises with `? 1 : 0`. This is behaviour-preserving for the button branch of `UIElement_RefreshBinding`, which treats the result as a boolean (`value.as.i != 0`, verified in `ui.c`) and composes `"<text>: ON/OFF"` from `button.text` into `button.display_text`. Because the shim still returns `BIND_INT` and the `!= 0` test is unchanged, the toggle ON/OFF presentation stays byte-identical. No other `BindingQueryFn` exists in the tree, so this is the complete migration of the generalised signature.

---

## 2. Generic core extension B: symmetric SINK CALLBACK for dynamic write-back

### Rationale

The query source is a panel-supplied read function; its write-side mirror is a panel-supplied store function. The core already has `BIND_SINK_ADDRESS` (store to a fixed address) and `BIND_SINK_COMMAND` (dispatch an opaque command code). We add a third sink kind that hands a committed `BindingValue` to a panel function, which decides where it lands right now. This is what lets a dynamic field (e.g. `damage` on the currently-selected object) commit without the core knowing anything about selection.

### Chosen shape (mirrors the source side exactly)

The source has `BindingQueryFn query; int query_key;`. Choosing the symmetric shape — `BindingSinkFn write; int write_key;` — over a `void *ctx` alternative keeps read and write identical in structure, so a reviewer reasons about both the same way and the core gains no new concept. Exact deltas in `include/ui/binding.h`:

```c
// BindingSinkKind enum (line ~101): add BIND_SINK_CALLBACK
typedef enum BindingSinkKind
{
    BIND_SINK_NONE = 0,
    BIND_SINK_ADDRESS,
    BIND_SINK_COMMAND,
    BIND_SINK_CALLBACK,   // NEW: panel-supplied store for dynamic targets
} BindingSinkKind;

// NEW typedef, placed next to BindingCommandFn (line ~113):
// Opaque write callback: the core hands over a validated BindingValue plus the opaque key.
// Returns true if the panel stored it, false to reject (caller reverts). Mirrors BindingQueryFn.
typedef bool (*BindingSinkFn)(int key, BindingValue value);

// BindingSink struct (line ~114): add the two NEW fields, update one comment
typedef struct BindingSink
{
    BindingSinkKind kind;
    BindingValueType value_type; // address sink: type to write; command sink: ignored; callback sink: parse type
    void *address;               // BIND_SINK_ADDRESS: *(T*)address = value
    ValidatorFn validator;       // optional, same contract as the Binder validators
    void *validator_ctx;         // == the old Binder.user_data
    BindingCommandFn command;    // BIND_SINK_COMMAND: command(code, NULL)
    int command_code;            // CommandType code, kept opaque here (0 => no dispatch)
    BindingSinkFn write;         // NEW: BIND_SINK_CALLBACK: write(write_key, value)
    int write_key;               // NEW: opaque key for the write callback
} BindingSink;
```

The `value_type` comment keeps `command sink: ignored` verbatim (accurate — `Binding_WriteSink`'s command branch never reads `value_type`, verified) and appends `callback sink: parse type`. The callback is the only new consumer of `value_type` on the sink side.

Exact delta in `src/engine/ui/binding.c` — `Binding_WriteSink` gains one branch (the only change to that function):

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

`Binding_Commit` is unchanged in shape: it parses with `sink.value_type` + `sink.validator` + `sink.validator_ctx`, then calls `Binding_WriteSink`. For a callback sink the parse/validate step is identical to an address sink; only the final store differs. This keeps validation (and the byte-identical INT/FLOAT landing, section 6) uniform across sink kinds. The callback carries no domain knowledge into the core: the core never knows what the panel does with the value, only that it returned true/false.

---

## 3. `BIND_NONE` source = "leave the display unchanged" (clear-when-unselected)

`Binding_RefreshText` already returns `false` on a `BIND_NONE` read without touching the output buffer (verified in `binding.c`):

```c
BindingValue v = Binding_ReadSource(&b->source);
if (v.type == BIND_NONE) return false; // output buffer untouched
return Binding_FormatValue(v, b->precision, out, out_bytes);
```

**This is preserved — no edit.** It is the generic mechanism for clear-when-unselected: a dynamic panel's query returns `{ .type = BIND_NONE }` when nothing is selected, and the core leaves the last-good display in place. The *policy* ("nothing selected" / "N/A") lives in the panel's query; the core only sees a type-NONE value and declines to write. No selection concept leaks into the core.

**String lifetime (required for any query returning `BIND_STRING`):** `BindingValue.as.s` is non-owning. `Binding_FormatValue`'s STRING case does a bounded `strncpy` into the caller's buffer *during* the single refresh call (verified), so the pointed-to storage need only outlive that one call. A string literal or `static` buffer qualifies; a stack-local buffer declared inside the query does **not** — returning a pointer into it is a dangling-pointer bug. Neither live proof field in this pass returns `BIND_STRING` (both are numeric), so this is a contract note for future query authors.

---

## 4. `TextBoxData` field + dispose

### Field — `include/ui/ui.h`

Add `Binding *binding` to `TextBoxData` (verified current fields: `text`, `data_type`, `data_bind`, `binder`, `font`, `cursor_position`). Additive:

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

`binder`, `data_bind`, and `binding` coexist. The migration is additive: a migrated field uses `binding`; an unmigrated field keeps `binder`/`data_bind`. UI elements come from the zeroing pool (`PoolAlloc` does `MemorySet` 0), so `binding` defaults to `NULL` safely for every untouched textbox — no constructor change required.

### Dispose — `src/engine/ui/ui.c`

`DisposeUIElement` (verified ~line 1044) already frees the textbox `binder` (lines 1064-1069) and the button `binding` (lines 1072-1076). Add the textbox binding free, mirroring both, between them:

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

`Binding_Destroy(Binding **)` NULLs the caller pointer (verified), so no explicit `= NULL` is needed.

---

## 5. Textbox read branch in `UIElement_RefreshBinding` (correcting the stale "removed/future-work" narrative)

`UIElement_RefreshBinding` in `src/engine/ui/ui.c` (verified ~line 1103) is BUTTON-ONLY today, followed by a `NOTE` comment (lines 1129-1132) stating textbox refresh "still flows through the existing `RefreshTextboxFields` path" and that "textboxes do not yet carry a Binding source". That premise is retired by this change: textboxes now carry a `Binding` (section 4) and migrated ones refresh here. Add a textbox branch after the button branch (before the `NOTE`, which is rewritten):

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

This branch is reached every frame by the existing `PanelSystem_RefreshBindings` walk in `panel_system.c` (verified ~line 661), which recurses `ForEachChild` calling `UIElement_RefreshBinding` on **every** element before layout/draw, with **no `is_enabled` guard** (verified — it recurses the whole `panel->root` tree). This "walk ignores `is_enabled`" fact is load-bearing for sections 8 and 9.

### Doc-comment updates (BOTH must change together)

1. The inline `NOTE` at the tail of `UIElement_RefreshBinding` in `ui.c` (lines 1129-1132) — replace with a note that migrated textboxes refresh here via the pull (skipped while focused) while unmigrated ones remain on the explicit `RefreshTextboxFields` path.
2. The declaration doc comment on `UIElement_RefreshBinding` in `include/ui/ui.h` (verified: the doc block ends ~line 318, immediately above the `void UIElement_RefreshBinding(UIElement *e);` declaration) — it currently ends `(Textbox read-refresh still uses the RefreshTextboxFields path.)`. Update it to state that a textbox with a readable binding source refreshes via this pull (skipped while focused), and that textboxes without a binding stay on `RefreshTextboxFields`.

Leaving either stale while updating the other is a doc-rot bug; both edits are part of this change.

### Edge cases

- **Unbound textbox:** `binding == NULL` → early return (identical to a button with no binding).
- **Focused textbox:** skipped before any write; in-progress text never clobbered.
- **Dynamic query returns NONE (nothing selected):** `Binding_RefreshText` returns false, buffer untouched, last-good text remains.
- **STRING source into a `String64`:** `Binding_FormatValue`'s STRING case does a bounded `strncpy` into `out_bytes - 1` with explicit NUL; `out_bytes` here is `sizeof(String64)`, so it cannot overflow.

---

## 6. Commit (write) direction: migrate `HandleTextCommit` to `Binding_Commit` (byte-identical)

### Current — `src/engine/ui/ui_input.c` (verified ~line 498)

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

(An outer `if (!element || !IsTextbox(element))` revert guard precedes this, verified line 501 — retained unchanged.)

### Change — binding first, legacy fallback preserved (additive)

Prefer the new binding when present; fall back to the legacy binder when not. Revert-on-failure is preserved exactly in every branch:

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
    // Neither carrier: nothing to commit to (matches the old binder-missing revert).
    RevertTextChanges(element, tbox_buffers);
    return;
}

ResetTextBuffers(tbox_buffers);
```

### Byte-identical guarantee (provable from source)

`Binder_ValidateAndWrite` is already a thin adapter over the symmetric core (verified in `binding.c`): it builds a `BIND_SINK_ADDRESS` sink with `value_type = b->type`, `address = b->target`, `validator = b->validator`, `validator_ctx = b->user_data`, then runs `Binding_ParseText` + `Binding_WriteSink`. `Binding_Commit` runs the identical sequence from `b->sink`. Therefore, for a stable migrated field whose sink is `BIND_SINK_ADDRESS` with the same `value_type`, `address`, and validator, the committed bytes land at the identical address with identical content:

- **INT:** validator if present (same `ValidatorIntRange`/`ValidatorIntPositive`), else `strtol`; `*(int*)addr = v.as.i`. Identical.
- **FLOAT:** validator or `strtof`; `*(float*)addr = v.as.f`. Identical.
- **VECTOR2D:** `ParseVector2d` (same accepted formats); `*(Vector2d*)addr = v.as.v`. Identical.
- **STRING:** `Binding_WriteSink`'s STRING case keeps the historic 255-cap: `strncpy(addr, s, 255); ((char*)addr)[255] = '\0';` — preserved byte-for-byte.

The migrated rpanel create fields (section 8) use no validator today (`BindTextboxData` builds their binders with `validator = NULL`, verified), so their migrated address-sink commit is byte-identical. For a callback-sink dynamic field (section 9) the only difference is the final store target (the panel's `write` fn instead of a raw address) — exactly the intended dynamic indirection; parse and validate are unchanged.

---

## 7. lpanel edit-view migration — EXPLICITLY OUT OF SCOPE (pattern note only)

The prior design proved the STABLE path on lpanel's entity-create edit fields. That is **dead code** and is not touched in this pass:

- `InitLPanelEditView` is only called from commented-out fallback (`lpanel_system.c` ~lines 342-343); the live `InitLeftPanelSystem` loads from XML and returns before the fallback.
- `lpanel.xml`'s entire DRAW view is commented out, including the `<TextField binding="physics.*">` rows.
- `G_UIState.edit_*_tbox` are never populated in the live XML path.

**No edits are made to `edit_specs`, `InitLPanelEditView`, or `RefreshEntityEditorFields` expecting a runtime effect.** Editing them would produce no behavioural change (the function is not invoked; the pointers are NULL) and would mask a double-writer only by accident.

**Pattern note (documentation, no code):** when the lpanel DRAW view / `InitLPanelEditView` is reinstated and `G_UIState.edit_*_tbox` are populated, apply the same stable-address binding mechanism proven here on rpanel (section 8): build a fixed-address `BIND_SRC_ADDRESS` + `BIND_SINK_ADDRESS` binding per field, attach/clear it by panel policy at a view-activation hook (so the unconditional refresh walk does not write into an inactive editor), and remove the corresponding `RefreshEntityEditorFields` / `edit_fields[]` rows in lockstep to preserve single-writer. This note creates no obligation in this pass.

---

## 8. LIVE STABLE proof: rpanel `create_fields` (fixed-address binding + single-writer row removal)

### Target (verified)

rpanel's "next world to create" params are genuinely stable addresses from `GetNextWorld*Ptr()`, refreshed every frame today via `RefreshTextboxFields`. Two sites in `rpanel_system.c`:

- Construction `UIFieldSpec create_fields[]` (array declared ~line 119; gravity row line 124), e.g. `{"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ..., FLOAT, &rpanel_create_gravity_tbox, NULL, GetNextWorldGravityPtr()}` and `{"Objects", ..., INT, &rpanel_create_objects_tbox, NULL, GetNextWorldObjectCountPtr()}`.
- Per-frame refresh `TextboxField create_fields[]` (~line 218): `{rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2, NULL}`, `{rpanel_create_objects_tbox, INT, GetNextWorldObjectCountPtr(), 0, NULL}`, plus the four VECTOR2D rows. This array is passed to `RefreshTextboxFields` (~line 226).

These addresses never move while the next-world params live — the clean STABLE proof. **Migrate at least one** (chosen: `rpanel_create_gravity_tbox` as the FLOAT proof; `rpanel_create_objects_tbox` as an INT proof is a natural second if desired).

### The stable binding helper — co-located in `integration_system.c`

Add a reusable helper next to `BindTextboxData` in `src/engine/system/integration_system.c`:

```c
// Build a stable (fixed-address) bidirectional binding: source and sink both target the same
// address, interpreted per data_type. No panel function - the address never moves. Attaches it
// to the textbox's binding slot (freed by DisposeUIElement). Validator-free by design.
// No-op on a NULL textbox or an unresolvable DataType.
void BindTextboxStable(UIElement *textbox, DataType type, void *address, int precision);
```

It maps `DataType` → `BindingValueType` with the existing `static BindingType ResolveBindingType(DataType)` (verified `static` at `integration_system.c` ~line 29). **The helper is placed in the same translation unit specifically so it can reuse `ResolveBindingType` without changing its linkage** — a helper in another TU could not call it. It builds a `Binding` with `source.kind = BIND_SRC_ADDRESS`, `sink.kind = BIND_SINK_ADDRESS`, both `value_type` from the map, both `address` the same pointer, `precision` as given, allocates with `Binding_Create`, and stores it in `textbox->data.textbox.binding` (updating in place if one already exists, mirroring `BindTextboxData`'s binder reuse). It is **validator-free by design**: `UIFieldSpec` (verified fields: `label`, `type`, `size`, `data_type`, `target`, `text_target`, `data_bind` — no validator) cannot supply one, and the rpanel create fields use no validator today, so this matches current behaviour exactly. A future stable field needing range validation must be hand-wired (build the `Binding` directly with `sink.validator`/`sink.validator_ctx`); extending the helper is out of scope.

**NULL contract (required):** `BindTextboxStable(NULL, ...)` is a no-op (returns immediately), as is `BindTextboxStable` on a `DataType` that resolves to `BINDING_NONE`.

### Construction-spec `data_bind` must be NULLed in the same change (no construction-time legacy `Binder`)

The migrated field has a **second** legacy carrier that is created at *construction*, independent of the per-frame refresh array. `InitUIFields` (verified `ui_constructors.c` lines 388-392) builds a legacy `Binder` from each spec row whose `data_bind` is non-NULL:

```c
if (specs[i].data_bind)
{
    BindTextboxData(input_child, specs[i].data_type, specs[i].data_bind);
}
```

The construction `UIFieldSpec create_fields[]` gravity row (verified `rpanel_system.c` line 124) currently passes `GetNextWorldGravityPtr()` as `data_bind`:

```c
{"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &rpanel_create_gravity_tbox, NULL, GetNextWorldGravityPtr()},
```

So without a change `InitUIFields` would build a legacy `Binder` on the gravity textbox at construction, and after `BindTextboxStable` attaches, that textbox would carry BOTH `binder` AND `binding` — a second commit carrier. Runtime double-*write* is masked only because `HandleTextCommit` prefers `binding` (section 6) and `DisposeUIElement` frees both; but carrying two carriers violates the single-writer invariant at the carrier level. Therefore, **in the same lockstep change, NULL the `data_bind` on the migrated construction row** so `InitUIFields` builds no binder, leaving `BindTextboxStable` as the sole carrier. The target pointer `&rpanel_create_gravity_tbox` stays so the field can still be attached after `InitUIFields`:

```c
// Gravity migrated to BindTextboxStable; data_bind NULLed so InitUIFields builds no legacy Binder.
{"Gravity", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &rpanel_create_gravity_tbox, NULL, NULL},
```

(This mirrors the v1 review's lpanel reconciliation — "set `data_bind = NULL` on the migrated row so no legacy `Binder` is created" — applied here to the live rpanel construction spec. The state_manager `damage` field in section 9 needs no equivalent fix: its `gameplay_specs` row already carries `data_bind == NULL`, so `InitUIFields` builds no binder there.)

### Attach site

The rpanel create textboxes are stable and always present in the create section (not view-gated like lpanel's edit view), so the binding is attached **once at construction** right after the `InitUIFields(create_world_cont, create_fields, ...)` call (verified `rpanel_system.c` lines 129-132 — the call spans those lines). For the gravity proof:

```c
InitUIFields(create_world_cont, create_fields, ARRAY_COUNT(create_fields), ...);

// STABLE binding proof: the next-world gravity param is a fixed address, so bind it directly.
// Replaces this field's per-frame RefreshTextboxFields row (removed below) to keep a single writer.
// The construction-spec data_bind for this row is NULLed above, so there is no legacy Binder.
BindTextboxStable(rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2);
```

(If also migrating objects: NULL the Objects construction row's `data_bind` too, then `BindTextboxStable(rpanel_create_objects_tbox, INT, GetNextWorldObjectCountPtr(), 0);`.)

### SINGLE-WRITER rule (load-bearing — same change)

**Rule:** a field migrated to a `binding` MUST have its `RefreshTextboxFields` `*_fields[]` row removed in the same change, or two writers race each frame (the per-frame `RefreshTextboxFields` address read vs. the per-frame `PanelSystem_RefreshBindings` pull — the walk has no `is_enabled` guard, so both run). So in lockstep remove the gravity row from the `TextboxField create_fields[]` array at `rpanel_system.c` ~line 218:

```c
TextboxField create_fields[] = {
    {rpanel_create_spawn_tbox, VECTOR2D, GetNextWorldSpawnOriginPtr(), 0, NULL},
    {rpanel_create_resolution_tbox, VECTOR2D, GetNextWorldResolutionPtr(), 0, NULL},
    {rpanel_create_basis_u_tbox, VECTOR2D, GetNextWorldBasisUPtr(), 0, NULL},
    {rpanel_create_basis_v_tbox, VECTOR2D, GetNextWorldBasisVPtr(), 0, NULL},
    // {rpanel_create_gravity_tbox, FLOAT, GetNextWorldGravityPtr(), 2, NULL}, // MIGRATED to BindTextboxStable
    {rpanel_create_objects_tbox, INT, GetNextWorldObjectCountPtr(), 0, NULL},
};
```

The remaining rows stay on the legacy path (additive migration). Commit: `HandleTextCommit` prefers `binding`, so the migrated gravity field commits via `Binding_Commit` through its address sink — byte-identical to its old binder commit (section 6), as it had no validator.

**Verification obligation (two parts):** confirm by grep that no migrated textbox (e.g. `rpanel_create_gravity_tbox`) (1) appears in BOTH a binding attach (`BindTextboxStable(...)`) AND a surviving per-frame `TextboxField *_fields[]` row, AND (2) has a non-NULL `data_bind` in its construction `UIFieldSpec *_fields[]` row. Part (2) is what prevents `InitUIFields` from silently rebuilding a legacy `Binder` carrier at construction. (This is a review check, not a build step — the build is not run in this phase.)

---

## 9. LIVE DYNAMIC proof: state_manager `damage` (panel query + callback; is_projectile visibility decided)

### Target (verified)

state_manager UI is built via hardcoded `InitUIFields` (not XML). `damage_tbox` is declared from `gameplay_specs` (~line 483) with `data_bind == NULL` (no legacy `Binder`). `RefreshPhysView` builds a `state_fields[]` row `{s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL}` (~line 779) and `RefreshGameplaySection` calls `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` (~line 642). `object->damage` is a plain FLOAT; `UIState_GetSelectedObject()` returns `Newtonoid2d *`. The "which address right now" is selection-driven — the DYNAMIC shape.

### Panel query + callback (domain policy lives in `state_manager_system.c`)

Add two static functions in `state_manager_system.c` (not in the core). The `write_key`/`query_key` are unused here (a single field), passed 0:

```c
// DYNAMIC read: current selection's damage, or BIND_NONE when nothing is selected (clear-when-none
// policy owned here, section 3). The core never learns what "selected" means.
static BindingValue StateManager_QueryDamage(int key)
{
    (void)key;
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!object || object->id == INVALID_ENTITY_ID)
    {
        return (BindingValue){ .type = BIND_NONE };
    }
    return (BindingValue){ .type = BIND_FLOAT, .as.f = object->damage };
}

// DYNAMIC write: store a committed FLOAT into the current selection's damage. Reject (false, caller
// reverts) when nothing is selected - storing to a stale/absent object is a bug, not a silent no-op.
static bool StateManager_WriteDamage(int key, BindingValue value)
{
    (void)key;
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!object || object->id == INVALID_ENTITY_ID || value.type != BIND_FLOAT)
    {
        return false;
    }
    object->damage = value.as.f;
    return true;
}
```

### is_projectile VISIBILITY DECISION — chosen: option (a), policy stays in `RefreshGameplaySection`

Two options were viable:
- **(a)** Keep `is_projectile` visibility owned by `RefreshGameplaySection` via `SetParentEnabledState(damage_tbox, is_projectile)`, and let the pull write `object->damage` unconditionally for any selected object (the walk ignores `is_enabled`, so a disabled textbox is still written but not drawn).
- **(b)** Encode `is_projectile` in `StateManager_QueryDamage` so it returns `BIND_NONE` for non-projectiles (query owns that policy).

**Chosen: (a).** It matches current behaviour exactly: today's `state_fields` row writes `object->damage` for any selected object too, while `RefreshGameplaySection` parents-disables the row for non-projectiles so the value is written-but-not-shown. Keeping visibility in `RefreshGameplaySection` leaves the two policies (which-value vs. is-shown) in their current, separate homes and avoids giving the query a second responsibility. The query above therefore keys only on selection (NOT on `is_projectile`), and the sample code matches that decision. `RefreshGameplaySection`'s `SetParentEnabledState(s_sm_ui.damage_tbox, is_projectile)` call is **unchanged**.

Consequence stated explicitly: because `PanelSystem_RefreshBindings` ignores `is_enabled`, the pull writes `object->damage` into the (possibly disabled) textbox whenever an object is selected, even a non-projectile; the parent-disable keeps it off-screen. This is identical to today's visible behaviour.

### Attach / detach by panel policy

The `damage_tbox` binding is attached once (it is dynamic by construction — the query resolves the live address each frame, so no re-attach on selection change is needed). Attach it where state_manager wires its fields, after `InitUIFields(gameplay_section, gameplay_specs, ...)` (~line 485), via a small co-located helper added next to `BindTextboxStable` in `integration_system.c`:

```c
// Attach a DYNAMIC binding: panel-supplied query (read) + callback (write). The panel owns
// "which address now / selected / clear-when-none". No-op on a NULL textbox.
void BindTextboxDynamic(UIElement *textbox, BindingValueType type, int precision,
                        BindingQueryFn query, BindingSinkFn write, int key);
```

It builds `source = { .kind = BIND_SRC_QUERY, .value_type = type, .query = query, .query_key = key }`, `sink = { .kind = BIND_SINK_CALLBACK, .value_type = type, .write = write, .write_key = key }`, `precision`, allocates with `Binding_Create`, stores in `textbox->data.textbox.binding`. **NULL contract:** no-op on NULL textbox. The call site:

```c
InitUIFields(gameplay_section, gameplay_specs, ARRAY_COUNT(gameplay_specs), ...);

// DYNAMIC binding proof: damage targets the current selection; the query/callback own selection.
BindTextboxDynamic(s_sm_ui.damage_tbox, BIND_FLOAT, 2,
                   StateManager_QueryDamage, StateManager_WriteDamage, 0);
```

### SINGLE-WRITER for damage

Remove the `damage_tbox` row from `RefreshPhysView`'s `state_fields[]` (~line 779) in lockstep, so the dynamic pull is the sole display writer:

```c
    {s_sm_ui.health_tbox, FLOAT, health ? (void *)&health->current_health : NULL, 2, NULL},
    {s_sm_ui.max_health_tbox, FLOAT, health ? (void *)&health->max_health : NULL, 2, NULL},
    // {s_sm_ui.damage_tbox, FLOAT, object ? (void *)&object->damage : NULL, 2, NULL}, // MIGRATED to BindTextboxDynamic
};
```

The "no legacy `Binder`" premise here needs care: the construction `gameplay_specs` row supplies no `data_bind` (verified `data_bind == NULL`), so `InitUIFields` builds no binder at construction — but `RefreshPhysView`'s `state_fields[]` row passes `object ? &object->damage : NULL`, and `RefreshTextboxFields` calls `BindTextboxData(... &object->damage)` every frame an object is selected (integration_system.c ~line 219), which DOES build a dynamic binder *today*. Removing the `state_fields[]` row (above) removes the only site that built that dynamic binder, so **post-migration `binder == NULL`** and commit flows solely through `Binding_Commit` → the callback sink → `StateManager_WriteDamage`. Unlike section 8's rpanel field, there is no construction-spec `data_bind` to NULL (it is already NULL). Focused-skip is preserved by the section 5 branch (the pull returns before writing while the user types; the query is not evaluated).

### NULL contracts summary (all stated)

- `BindTextboxStable(NULL, ...)` — no-op.
- `BindTextboxDynamic(NULL, ...)` — no-op.
- `ClearTextboxBinding(NULL)` — no-op (helper below).
- `Binding_RefreshText`/`Binding_ReadSource`/`Binding_WriteSink` already degrade NULL source/sink/address to BIND_NONE / false (verified) — the dynamic query returning `BIND_NONE` on no selection rides this.

A detach helper for completeness (used only if a panel ever needs to drop a binding; not required by either live proof since both are attach-once):

```c
// Detach and free a textbox's binding and NULL the slot. No-op on NULL textbox or NULL binding.
void ClearTextboxBinding(UIElement *textbox); // in integration_system.c, next to BindTextboxStable
```

---

## 10. Testability

- **Unit testable (pure, no UI tree):** `Binding_ReadSource` with a `BIND_SRC_QUERY` returning each `BindingValue` type (INT/FLOAT/VECTOR2D/STRING and NONE) — asserts the pass-through of extension A. `Binding_WriteSink` with `BIND_SINK_CALLBACK` — asserts it returns the callback's bool and rejects a NULL `write`. `Binding_Commit` through a callback sink with and without a validator — asserts parse→validate→callback ordering. The `LPanel_QueryDebugEnabled` shim's `? 1 : 0` normalisation — assert both a raw non-zero and 1 collapse to `as.i == 1`.
- **Unit testable (adapter equivalence):** feed the same text + `value_type` + address through `Binder_ValidateAndWrite` and through a hand-built `BIND_SINK_ADDRESS` `Binding_Commit`; assert identical bytes at the target for INT/FLOAT/VECTOR2D/STRING (the 255-cap case). This mechanically proves section 6's byte-identity claim.
- **Integration testable (needs the UI tree + pool):** `UIElement_RefreshBinding` on a focused vs. unfocused bound textbox — assert the focused one is not overwritten; `DisposeUIElement` on a textbox carrying a `binding` — assert no leak (binding freed, slot NULLed). The single-writer rule is a static grep check, not a runtime test.
- **Hard-to-test (acknowledged):** the per-frame interaction between `SetParentEnabledState(damage_tbox, is_projectile)` and the unconditional pull is a visual/integration concern; it is pinned by the explicit decision in section 9 rather than a unit test. No design rethink needed — the write-but-not-draw behaviour is intentional and matches today.

The design keeps the two new core extensions as pure functions over `BindingValue`, so the bulk of the risk is unit-coverable without the UI runtime.

---

## 11. Responses to the design review findings

### 11a. Responses to the v2 review (`design-review-v2.md` / `.json`) — this revision

- **Finding 1 (MEDIUM — migrated rpanel gravity keeps a construction-time legacy `Binder`; single-writer removal incomplete, section 8):** ADDRESSED. Section 8 now adds a dedicated subsection "Construction-spec `data_bind` must be NULLed in the same change", requiring the migrated gravity row of the construction `UIFieldSpec create_fields[]` (rpanel_system.c line 124) to set `data_bind = NULL` so `InitUIFields` (ui_constructors.c lines 388-392) builds no legacy `Binder`, leaving `BindTextboxStable` the sole carrier. The target pointer `&rpanel_create_gravity_tbox` is retained so the attach can still occur after `InitUIFields`. The section-8 verification obligation is extended to a two-part grep: (1) no migrated textbox in both a `BindTextboxStable(...)` attach and a surviving per-frame `TextboxField` row, and (2) no migrated textbox with a non-NULL `data_bind` in its construction `UIFieldSpec` row.
- **Finding 2 (NIT — section 9's "no legacy Binder" reasoning conflates construction spec with per-frame refresh):** ADDRESSED. Section 9's commit paragraph is restated: the construction `gameplay_specs` row supplies no `data_bind`, and `RefreshTextboxFields` builds a dynamic binder per-frame from `state_fields[]` until that row is removed; removing it leaves `binder == NULL` post-migration so commit flows solely through `Binding_Commit`. The premise is now stated as true *after* the row removal, not inherently.
- **Finding 3 (NIT — line-number drift):** ADDRESSED. `BindingQueryFn` typedef corrected to line 91; `LPanel_QueryDebugEnabled` def to line 246 and `query_key` to line 276; `ui.h` doc comment stated as ending ~line 318 with reliance on the verbatim quoted anchor. Section 11 finding-6 summary updated accordingly.
- **Finding 4 (NIT — rpanel attach-site line reference approximate):** ADDRESSED. The attach-site text now states the `InitUIFields(create_world_cont, create_fields, ...)` call spans rpanel_system.c lines 129-132.

### 11b. Responses to the v1 review (`design-review.md`) — carried forward

- **Finding 1 (HIGH — section 7 targeted dead lpanel code):** ADDRESSED. v2 descopes lpanel entirely (option (a) from the review). lpanel edit-view is a documentation-only pattern note (section 7); no `edit_specs`/`InitLPanelEditView`/`RefreshEntityEditorFields` edits are made. The STABLE path is proven on the LIVE rpanel `create_fields` instead (section 8).
- **Finding 2 (HIGH — `LPanel_OnViewSelected` fires at init on NULL edit pointers; `ClearTextboxBinding` NULL contract unstated):** ADDRESSED by descoping. No `LPanel_OnViewSelected` wrapper is introduced in this pass, so the init-fire-order and NULL-pointer concern does not arise. The rpanel stable field is attached once at construction (always-present section, not view-gated), avoiding the view-activation ordering problem altogether. `ClearTextboxBinding`'s NULL-contract is nonetheless stated (section 9) for the helper that exists.
- **Finding 3 (MEDIUM — single-writer vs. live per-frame refresh):** ADDRESSED. The single-writer rule is stated precisely and is load-bearing for the LIVE migrations: the migrated rpanel gravity row is removed from `RefreshTextboxFields`' `create_fields[]` (section 8) and the migrated `damage` row from `RefreshPhysView`'s `state_fields[]` (section 9), each in lockstep, with a grep verification obligation. Because these are live per-frame arrays, the removal is mandatory, not optional.
- **Finding 4 (MEDIUM — is_projectile visibility interaction):** ADDRESSED. Section 9 explicitly chooses option (a): visibility stays in `RefreshGameplaySection` via `SetParentEnabledState`; the pull writes `object->damage` unconditionally for a selected object (walk ignores `is_enabled`); this matches current behaviour. The query keys only on selection, and the sample code matches that decision.
- **Finding 5 (NIT — `ResolveBindingType` static/co-location):** ADDRESSED. Section 8 states `ResolveBindingType` is `static` in `integration_system.c` and that `BindTextboxStable` (and `BindTextboxDynamic`, `ClearTextboxBinding`) are placed in that same TU so they can reuse it without changing linkage.
- **Finding 6 (NIT — line references):** ADDRESSED. All line references were re-verified fresh against source: `BindingQueryFn` typedef binding.h line 91; `LPanel_QueryDebugEnabled` def line 246, assignment line 274, `query_key` line 276; rpanel `UIFieldSpec create_fields` ~line 119 (gravity row line 124), `InitUIFields` call spans lines 129-132, per-frame `TextboxField create_fields` ~218, refresh call ~226; state_manager `gameplay_specs` ~483, `state_fields` damage row ~779, `SetParentEnabledState(damage_tbox, is_projectile)` ~642; `DisposeUIElement` ~1044; `UIElement_RefreshBinding` ~1103 with NOTE ~1129-1132; `ui.h` doc comment ends ~line 318. Where a line drifted by one or two lines, the quoted anchor text matches source verbatim so edits stay unambiguous.

---

## Confirmation checklist

- Generic core stays domain-free (binding.h / binding.c / `UIElement_RefreshBinding` / `PanelSystem_RefreshBindings`): CONFIRMED. Extensions A and B carry only `(int key) -> BindingValue` and `(int key, BindingValue) -> bool`; no selection/debug/entity/visibility concept enters the core. The textbox refresh branch only NULL-checks, focused-skips, and formats.
- Focused-skip invariant (`e->is_focused` never overwritten): CONFIRMED (section 5 returns before any write), exercised live on both proof fields.
- Button toggle ON/OFF display unchanged: CONFIRMED (`? 1 : 0` is behaviour-preserving under the `value.as.i != 0` test).
- Migration additive (legacy Binder / RefreshTextboxFields retained for unmigrated fields): CONFIRMED; `HandleTextCommit` prefers `binding` then falls back to `binder`.
- No memory leaks (textbox binding freed in `DisposeUIElement`): CONFIRMED (section 4 mirrors the existing frees; `Binding_Destroy` NULLs; pool-zeroing defaults untouched slots NULL).
- Byte-identical commit (validated INT/FLOAT and STRING 255-cap): CONFIRMED against source (section 6; `Binder_ValidateAndWrite` and `Binding_Commit` run the identical parse+write).
- Single-writer enforced for every migrated field — at BOTH the per-frame refresh array AND the construction spec: CONFIRMED. Section 8 removes the migrated gravity row from the per-frame `TextboxField create_fields[]` AND NULLs its construction `UIFieldSpec` `data_bind` so `InitUIFields` builds no legacy `Binder`; section 9 removes the migrated `damage` row from `state_fields[]` (its construction `gameplay_specs` row already has `data_bind == NULL`). The two-part grep verification obligation is stated.
- Git untouched, nothing committed/staged/branched; no worktree created; build NOT run in this phase: CONFIRMED. This design phase writes only this document.
