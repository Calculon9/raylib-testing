# Design: XML-authoritative ViewHost + View selection for the left panel

## Overview

The left panel currently declares its structure twice. `lpanel.xml` describes a
`<ViewHost>` with a `<ViewSelector>`, its `<Option>`s and two `<View>`s, but the
loader silently drops most of that intent: `<ViewHost>` is an unregistered tag so
its whole subtree is discarded, `BuildViewSelector`/`BuildOption` are empty stubs,
and `BuildView` throws away the `type=` attribute. The working panel is instead
assembled imperatively in `InitLPanel`, which hard-codes `labels[]={"STATE","DRAW"}`,
builds a `ViewHostSystem` via `ViewHostSystem_CreateStandard` (which constructs its
own selector on its own root) and then **replaces that root** with the XML root,
orphaning the C-built selector. The hard-coded C paths (`InitLPanelStateView`,
`InitLPanelEditView`) are left commented-out but still present.

This design makes the XML markup the single authoritative source for the left
panel's view hosting **and** view selection. After the change:

- `<ViewHost>` builds the host root container; its children load.
- `<View type="...">` carries its `ViewType` through to the `View` struct, so each
  view gets its correct type (not a blanket `LPANEL_STATE_VIEW`).
- `<ViewSelector>`/`<Option>` build a real, wired selector whose buttons drive
  `ViewHostSystem_SelectView` exactly as `ViewHostSystem_CreateViewSelector` does.
- `InitLPanel` adopts the XML-built host + selector instead of building its own.
- Both STATE and DRAW are distinct, selectable views; selecting DRAW sets
  `G_UIState.active_view = LPANEL_DRAW_VIEW` (keeping `geometry_editor.c` and
  `ui_system.c` working).

The one deliberate gap: the DRAW view's `<TextField>` controls render and the view
is selectable, but their `G_UIState.edit_*_tbox` pointers are **not** populated and
the fields are **not** bound to entity-create data in this pass. That is a
documented follow-up (see Out of Scope).

## Technology stack (locked)

C11, built with CMake + FetchContent, UCRT64 gcc + Ninja. XML parsed with Mini-XML
(`mxml`), the library already used by the loader. No new dependencies. UK English
spelling in identifiers and comments (`colour`, `Initialise`, `Unrecognised`). No
new parallel systems: everything is unified onto the existing `ui_loader.c` builder
registry and the existing `ViewHostSystem`/`ViewSelector` infrastructure in
`view_host_system.{h,c}`.

## Current-state facts this design relies on (re-verified)

- `lpanel.xml` root is `<ViewHost id="left_panel" viewport="lpanel_viewport"
  scale="1.0" layout="stacked" initialView="state_view">` containing one
  `<ViewSelector id="left_panel_views" type="enumerate">` with two `<Option>`s
  (STATE -> `state_view`, DRAW -> `draw_view`), then a live
  `<View id="state_view" type="LPANEL_STATE_VIEW">` and a live
  `<View id="draw_view" type="LPANEL_DRAW_VIEW">`. **Note:** in the current file the
  `draw_view` is already uncommented and present (the task brief's "commented-out"
  description is stale); requirement 5 is therefore mostly satisfied in markup and
  the work is to make the loader honour it. The design still covers the uncomment
  case defensively.
- `ui_loader.c` registers Panel, ViewSelector, Option, View, Section, Container,
  Button, Label, TextField — but **not** ViewHost. Unknown tags hit
  `UILoader_FindBuilder==NULL`, log "Unknown XML element type", return NULL and the
  subtree is dropped (`UILoader_ParseElementNode`).
- `BuildView` extracts `view_id` and `type=` but discards both; it creates a
  `CreateUIContainer`, sets `container->type = UI_ELEMENT_VIEW` so the
  post-processor can find it.
- `BuildViewSelector`/`BuildOption`/`BuildPanel`/`BuildContainer` are
  `CreateUIContainer`/`CreateUIButtonDefault` stubs with TODOs; `BuildOption`
  creates an inert `UI_ELEMENT_BUTTON_ENUMERATE` with `(NULL,NULL,NULL)` handlers
  and ignores `view=`.
- `UILoader_CollectViewContainers` allocates a `View` per `UI_ELEMENT_VIEW`
  element, sets `view->container=elem` and **hard-codes** `view->type =
  LPANEL_STATE_VIEW`. `UILoader_ExtractViews(root,&count)` returns the `View*`
  array.
- `ViewSelector { ViewHostSystem *panel; UIElement **buttons; int *view_indices;
  size_t count; size_t active_index; ViewSelectionCallback on_view_selected; }`.
  Selection path: button click -> `HandleViewHostSelectorClick` reads selector from
  `button->data.button.data_bind` and index from
  `*(int*)button->data.button.user_data` -> `ViewHostSystem_SelectView(selector,
  index)` -> `SetPanelActiveView` enables `views[index].container`, disables the
  rest -> `selector->on_view_selected(views[index])`. For lpanel that callback is
  `ViewHostSystem_HandleViewSelected`, which sets `G_UIState.active_view =
  view->type`. So `view->type` **must** be correct per view.
- `InitLPanel` loads XML, calls `LPanel_AttachToggleSources(root)` (wires the
  STATE toggle query sources), `UILoader_ExtractViews`, then
  `ViewHostSystem_CreateStandard(..., labels, 2, ViewHostSystem_HandleViewSelected,
  ...)`, grafts `lpanel->root = root`, pushes the extracted views, and
  `ViewHostSystem_FinaliseInit` (selects view 0). The `CreateStandard`-built
  selector is attached to the discarded root — the dangling-root bug.
- `UIElement.data.button` has `data_bind` and `user_data` (both `void*`) plus
  `binding`. `UI_ELEMENT_ROOT`, `UI_ELEMENT_VIEW`, `UI_ELEMENT_BUTTON_ENUMERATE`
  exist in the `UIElementType` enum.

---

## Design

### 1. Decoupling the ViewHostSystem from "selector is built on the root"

The root cause of the dangling-root bug is that `ViewHostSystem_CreateStandard`
does three things at once: creates the host, builds its root UIElement, and builds
a selector **as a child of that root**. When `InitLPanel` later overwrites
`lpanel->root` with the XML tree, the selector's buttons dangle under the old,
discarded root.

The chosen fix keeps the host generic and keeps lpanel-specific wiring in
`InitLPanel`: `InitLPanel` stops calling `ViewHostSystem_CreateStandard` entirely.
Instead it uses the lower-level primitives that already exist —
`ViewHostSystem_Create`, `ViewHostSystem_InitViews` — and adopts the XML root
directly. The views and the selector both come from the XML tree. No C-built
selector is ever created, so there is nothing to dangle.

Two viable approaches were considered for *where* the selector gets wired:

- **(A) Loader wires the selector fully** inside `BuildViewSelector`/`BuildOption`,
  producing a ready `ViewSelector` registered on the host. Rejected as the primary
  path because at `BuildViewSelector` time the host's `views` array does not yet
  exist (views are extracted *after* the whole tree is built), and the loader is
  meant to stay app-agnostic — it should not own `ViewHostSystem` lifecycle.
- **(B) Loader produces the button *structure* (container + enumerate buttons +
  parsed `text`/`view` metadata) and the application finalises the wiring** once
  all views are known. Chosen. It respects document-order (Options precede Views),
  keeps `ViewHostSystem` ownership on the application side, and mirrors the existing
  `ViewHostSystem_CreateViewSelector` wiring exactly.

So the division is: the loader builds the DOM (host container, selector container,
enumerate buttons with their labels, view containers tagged with correct
`ViewType` and preserved `id`), and records enough metadata (each Option's target
view `id`, each View's `id`) for a new application-agnostic finalisation pass to
bind buttons to view indices and register a `ViewSelector` on the host.

### 2. ViewHost builder — `BuildViewHost`

New builder registered for the `ViewHost` tag. `<ViewHost>` is the root element of
`lpanel.xml`, so `parent` is NULL. It maps to the host's **root container** concept:
it builds a fill-sized container that will become `lpanel->root`'s content host.

```c
// Builder for <ViewHost> elements (root of a multi-view panel markup).
// Attributes: id (optional), viewport (informational), scale (informational),
//             layout (optional: "stacked"/"stack"/...), initialView (optional).
// Produces a fill-size stacked container. The ViewSelector and Views nest inside.
// The host's ViewportRegion / palette are supplied by the application in InitLPanel;
// viewport/scale attributes are recorded for documentation but not resolved here,
// keeping the generic loader free of lpanel-specific viewport knowledge.
static UIElement *BuildViewHost(mxml_node_t *node, UIElement *parent,
                                const UIPalette *palette, UILoaderContext *ctx);
```

Behaviour:

- Extract `id` and `layout` via `UILoader_ExtractCommonAttrs` + `UILoader_ParseLayout`
  (treat `"stacked"` as an alias of `"stack"` in `UILoader_ParseLayout` — add that
  alias so the existing `layout="stacked"` attribute resolves instead of silently
  defaulting). **Scope note:** `UILoader_ParseLayout` is the shared layout parser
  used by every element that reads `layout=` (`BuildSection`, `BuildContainer`,
  `BuildViewHost`, ...), so adding the `stacked -> stack` alias is a **global**
  loader change, not local to `BuildViewHost`: `layout="stacked"` now resolves
  everywhere. This is harmless (previously it silently defaulted) and intended.
- Create a fill-size container (`ui_fill_container_size`, stack spacing from the
  parsed layout) via `CreateUIContainer` with `parent` (NULL at root). This becomes
  the tree root returned to `InitLPanel`.
- Record `initialView` for the application: see §6. The attribute value is read from
  the node again in `InitLPanel` (the loader does not need to persist it on the
  UIElement, since `InitLPanel` can re-read the XML attribute via a small accessor,
  OR — chosen approach — the loader exposes the parsed value through a
  post-process accessor; see §6 for the concrete, allocation-free choice).

`viewport="lpanel_viewport"` and `scale="1.0"` are intentionally **not** resolved by
the loader. The viewport and scale are lpanel-specific; `InitLPanel` already owns
`&lpanel_viewport` and passes `scale=1.0` today. Resolving them in the generic
loader would hard-code application knowledge. They remain documentation in the
markup; `InitLPanel` supplies the real values.

### 3. View builder — `BuildView` parses `type=` into a `ViewType`

The generic loader must not hard-code `LPANEL_*` enum names. The view-type string
is resolved by an **application-supplied resolver**, matching the existing
`resolve_binding`/`resolve_command` pattern in `UILoaderContext`. This was chosen
over a documented static string->ViewType map inside the loader because the map
would bake lpanel/rpanel/state-manager enum names into the generic loader, breaking
the same app-agnostic boundary the command/binding resolvers were created to
preserve. The resolver keeps `ViewType` knowledge in the application (lpanel).

New typedef and context field in `ui_loader.h`:

```c
// Resolve a view-type string (e.g. "LPANEL_DRAW_VIEW") to a ViewType code.
// Returns a defaulted/zero ViewType and sets *resolved=false when unrecognised.
// Kept as a callback so the generic loader never names application ViewType enums.
typedef ViewType (*UIViewTypeResolver)(const char *type_string, bool *resolved,
                                       UILoaderContext *ctx);
```

**Chosen (both halves, unambiguous):** add BOTH the typedef **and** a
`UIViewTypeResolver resolve_view_type;` field to `struct UILoaderContext`, and
thread the resolver through the loader entry-point signatures so each entry point
assigns it into its stack-local `ctx` exactly as it already assigns
`resolve_binding`/`resolve_command`. `BuildView` then reads
`ctx->resolve_view_type`, which is only valid because the field exists. There is
no separate "setter" and no claim that a field is awkward — the context is
stack-local inside each entry point and that is precisely where the other two
resolvers are assigned today (`ctx = { .resolve_binding = resolve_binding,
.resolve_command = resolve_command, ... }` in `ui_loader.c`); the view-type
resolver joins them as `.resolve_view_type = resolve_view_type`.

Both resolver-bearing entry points gain the trailing parameter so the field can
be populated on either path:

```c
UIElement *UILoader_LoadFromFileWithResolvers(const char *filepath, const UIPalette *palette,
                                              UIBindingResolver resolve_binding,
                                              UICommandResolver resolve_command,
                                              UIViewTypeResolver resolve_view_type,
                                              void *user_data);

UIElement *UILoader_LoadFromStringWithResolvers(const char *xml_string, const UIPalette *palette,
                                                UIBindingResolver resolve_binding,
                                                UICommandResolver resolve_command,
                                                UIViewTypeResolver resolve_view_type,
                                                void *user_data);
```

Both definitions assign `ctx.resolve_view_type = resolve_view_type`. The
no-resolver convenience wrappers `UILoader_LoadFromFile`/`UILoader_LoadFromString`
pass `NULL` for the new resolver (and so leave `ctx.resolve_view_type == NULL`,
which `BuildView` treats as "no resolver" — default to the first ViewType with a
warning, per Error handling). The production caller is `InitLPanel`
(`UILoader_LoadFromFileWithResolvers`); the headline integration test uses
`UILoader_LoadFromStringWithResolvers` with a real stub resolver (see
Testability). Both resolver-bearing variants are extended; the extension is cheap
and contained — `UILoader_LoadFromFileWithResolvers` has one caller and
`UILoader_LoadFromStringWithResolvers` has the test caller.

`BuildView` changes:

- Keep extracting `view_id` (needed for Option id->index resolution, §5) and
  `type=`.
- Resolve `type=` via `ctx->resolve_view_type` when present. On success, stash the
  resolved `ViewType` so the post-processor can read it (see "carrying type and id"
  below). On failure/absent resolver, default to the first ViewType value (0 ==
  `LPANEL_STATE_VIEW`) and `LOADER_WARNING(ctx, "Unrecognised view type")`.
- Still set `container->type = UI_ELEMENT_VIEW` so `UILoader_CollectViewContainers`
  finds it.

Carrying `type` and `id` from `BuildView` to the post-processor: the `UIElement`
has no spare `ViewType`/id string field, and adding one to the hot `UIElement`
struct for a load-time concern is wasteful. The cleanest, allocation-light choice
that unifies with the existing extraction pass:

- **Chosen:** change `UILoader_CollectViewContainers` to re-resolve nothing — the
  loader builds, during parsing, a small side table (static-capacity array inside
  `ui_loader.c`, reset at the start of each top-level load) that records, per
  `UI_ELEMENT_VIEW` container pointer: its resolved `ViewType` and its `id` string.
  `BuildView` appends an entry; `UILoader_CollectViewContainers` looks the container
  up to set `view->type` (replacing the hard-coded `LPANEL_STATE_VIEW`) and the
  Option resolver (§5) uses the same table to map `id` -> view index. The table is
  loader-internal, fixed-capacity (reuse `MAX_REGISTERED_BUILDERS`-style cap, e.g.
  `#define MAX_LOADED_VIEWS 32`), and cleared per load so there is no cross-load
  state leak. This avoids a second XML walk and avoids widening `UIElement`.

This replaces the `view->type = LPANEL_STATE_VIEW` line in
`UILoader_CollectViewContainers` with a table lookup keyed on `elem`, defaulting to
the first ViewType with a warning if (defensively) a view container is somehow not
in the table.

### 4. ViewSelector + Option builders

`BuildViewSelector` becomes real: it builds the selector's **container** (the
toggle bar), mirroring `ViewHostSystem_CreateStandardViewSelector`'s container
styling (transparent surface, container border colour, zero inline spacing,
`ui_standard_selector_container_size`). It does **not** itself allocate a
`ViewSelector` struct (that needs the host + the finished views). It records that
this container is a selector so the finalisation pass (§5) can find it; it does
this by tagging the container with a loader-internal marker in the same side-table
style used for views, storing the selector container pointer and its `type=`
(e.g. "enumerate" vs a future "hover").

```c
// Builder for <ViewSelector> elements.
// Attributes: id (optional), type (optional: "enumerate" (default) / "hover").
// Builds the toggle-bar container; Option children become enumerate buttons.
// Records the container + Option targets for application-side finalisation.
static UIElement *BuildViewSelector(mxml_node_t *node, UIElement *parent,
                                    const UIPalette *palette, UILoaderContext *ctx);
```

`BuildOption` becomes real: it creates a `UI_ELEMENT_BUTTON_ENUMERATE` button (as
today) with the `text=` label and `ui_standard_selector_button_size`, but now reads
`view=` and records the (button pointer, target view id) pair in the loader side
table. It deliberately leaves `on_click`/`data_bind`/`user_data` NULL at build time
— those are filled by the finalisation pass once the `ViewSelector` and
`view_indices` backing array exist, exactly matching the pointers that
`ViewHostSystem_CreateViewSelector` sets (`HandleViewHostSelectorClick`,
`&selector->view_indices[i]`, `data_bind=selector`).

```c
// Builder for <Option> elements (inside ViewSelector).
// Attributes: text (required, button label), view (required, target View id).
// Creates an enumerate button and records its target view id for later wiring.
static UIElement *BuildOption(mxml_node_t *node, UIElement *parent,
                              const UIPalette *palette, UILoaderContext *ctx);
```

If `view=` is missing/empty, `LOADER_WARNING(ctx, "Option missing view target")`
and the button is still created but will resolve to index 0 (first view) at
finalisation (documented, non-fatal).

### 5. Finalisation pass: id->index resolution and selector wiring

Because Options appear **before** the Views in document order, binding must happen
after the whole tree is parsed and all views are known — a classic two-pass /
post-process. A new post-processing entry point in the loader performs this using
the side table populated during parsing. It is application-agnostic: it needs a
`ViewHostSystem*` (already populated with the extracted views) and nothing
lpanel-specific.

```c
// Build and register a ViewSelector on `host` from the <ViewSelector>/<Option>
// metadata captured during the last load. Resolves each Option's target view id to
// the index of the matching View in host->views, allocates the ViewSelector + its
// buttons[]/view_indices[] arrays (reusing the host's selector list), and wires each
// enumerate button exactly like ViewHostSystem_CreateViewSelector:
//   button->data.button.on_click   = HandleViewHostSelectorClick
//   button->data.button.user_data  = &selector->view_indices[i]
//   button->data.button.data_bind  = selector
// `callback` is the on_view_selected callback (ViewHostSystem_HandleViewSelected for lpanel).
// Returns the created selector (also pushed to host->selectors), or NULL on error.
ViewSelector *UILoader_BuildSelectorFromMarkup(ViewHostSystem *host,
                                               ViewSelectionCallback callback);
```

Resolution algorithm (per selector recorded in the table):

1. For each recorded Option (button pointer + target view id), find the index `k`
   of the `View` in `host->views` whose source view container has that `id`. The
   view `id`s were recorded in the same table in §3, keyed by container pointer; the
   `host->views[k]->container` pointer identifies the row, so Option-id ->
   container -> `views` index is a direct comparison. **Linchpin invariant:** the
   container pointers recorded in the side table are *identical* to
   `host->views[k]->container`, because `UILoader_CollectViewContainers` stores
   `view->container = elem` (the exact tree pointer) without copying, and
   `InitLPanel` pushes those same `View*` into `host->views`. So Option-id ->
   container -> index is a direct `UIElement*` pointer comparison with no
   intermediate copy — this is what makes the whole id->index resolution sound.
   An Option whose `view=` id matches no recorded `View` id (including the case
   where the referenced View has no `id` attribute — see below) defaults to index
   0 with a `LOADER_WARNING`. A `View` with no `id` attribute (empty string) can
   never match an Option `view=` by `strcmp`, so any Option that targets it falls
   to the index-0 default + warning; this is the symmetric statement of the
   Option-side fallback already specified in §4.
2. Allocate a `ViewSelector` with `count = number of Options`. Allocate its two
   backing arrays with the **same element types and allocator** as
   `AllocatePanelViewSelector` so `DestroyPanelViewSelector`'s
   `sizeof(int) * count` / `sizeof(UIElement*) * count` frees stay correct:
   `selector->view_indices = AllocateBytes(sizeof(int) * count)` (element type
   `int`, matching the `int *view_indices` struct field and the
   `*(int*)user_data` read in `HandleViewHostSelectorClick`) and
   `selector->buttons = AllocateBytes(sizeof(UIElement *) * count)`. Set
   `buttons[i] =` recorded Option button, `view_indices[i] = k` (the resolved
   index as an `int`, **not** a trivial `0..count-1` — this is the key difference
   from `AllocatePanelViewSelector`, which assumes option order equals view
   order). Set `panel = host`, `active_index = count` (unselected sentinel),
   `on_view_selected = callback`.
3. Wire each button's `on_click`/`user_data`/`data_bind` as above.
4. `LArray_Push(&host->selectors, &selector)`. Do **not** style the buttons here
   and do **not** expose a new public styling helper: `UpdatePanelViewSelectorButtons`
   stays `static` in `view_host_system.c`. The single, canonical styling +
   initial-selection point is the `ViewHostSystem_SelectView(selector, initial)`
   call that `InitLPanel` makes exactly once after this function returns (§7 step
   9); `ViewHostSystem_SelectView` already calls `UpdatePanelViewSelectorButtons`
   internally, so the first styling pass happens there and nowhere else.
   `UILoader_BuildSelectorFromMarkup` therefore leaves `active_index == count`
   (the unselected sentinel); that sentinel is only transient and is overwritten
   by the mandatory `SelectView` call. `InitLPanel` **never** calls
   `ViewHostSystem_FinaliseInit` (which would force index 0 and fight the resolved
   `initialView`). The only path on which `SelectView` does not run is selector
   allocation failure (NULL return) — handled explicitly in Error handling by
   disabling all view containers except index 0 so a single view is still visible
   (see Finding-5 fix below).

`view_indices` must be a heap array owned by the `ViewSelector` (as today) because
`user_data` stores `&selector->view_indices[i]`; those addresses must remain stable
for the selector's lifetime. `DestroyPanelViewSelector` already frees
`buttons`/`view_indices`, so ownership and teardown match the existing path with no
new leak. One nuance: `buttons[i]` here point at UIElements owned by the XML tree
(freed by `DisposeUIElement(panel->root)`), identical to the hover-selector reuse
pattern; `DestroyPanelViewSelector` only frees the `buttons` *array*, not the
elements, so this is correct.

### 6. Honouring `initialView`

`ViewHostSystem_FinaliseInit` currently always selects index 0. To honour
`initialView="state_view"`:

- The finalisation path resolves the `initialView` view id to its index in
  `host->views` (same id->index mechanism as Options), and selects that index via
  `ViewHostSystem_SelectView`, falling back to index 0 when the attribute is absent
  or unresolved.
- **Chosen placement (single decision — no new finalise variant):**
  `ViewHostSystem_FinaliseInit` is left generic and is **not called by**
  `InitLPanel` (per the Finding-4 pin in §5). `InitLPanel` instead resolves
  `initialView` and calls `ViewHostSystem_SelectView(selector, initial_index)`
  directly — the same single styling + selection call that §5 step 4 designates.
  Concretely, `InitLPanel` will: build the selector via
  `UILoader_BuildSelectorFromMarkup`, then resolve the initial index through a new
  loader accessor `UILoader_ResolveViewIndexById(host, id_string)` (public, uses the
  side table / host views), then call `ViewHostSystem_SelectView(selector,
  initial_index)` and a final `UpdateUISpace(host->root, host->seed_box)`. The
  `initialView` string is read by `InitLPanel` from the root node — but
  `InitLPanel` no longer holds the mxml node. Therefore the loader records the
  `ViewHost`'s `initialView` string in its side table during `BuildViewHost`, and
  exposes it via `const char *UILoader_GetInitialViewId(void)` (returns the last
  load's value, or NULL). `InitLPanel` passes that into
  `UILoader_ResolveViewIndexById`.

Document the default explicitly: **absent or unresolved `initialView` => first
view (index 0), with a warning on an unresolved non-empty value.** With the current
markup, `initialView="state_view"` resolves to index 0 anyway, so STATE is the
initial active view (matching `ui_system.c`'s
`G_UIState.active_view = LPANEL_STATE_VIEW` default).

### 7. `InitLPanel` rewiring

New `InitLPanel` flow (lpanel-specific resolvers stay here; no hard-coded labels):

```text
1. root = UILoader_LoadFromFileWithResolvers(
       lpanel.xml, &ui_default_palette,
       LPanel_ResolveBinding, LPanel_ResolveCommand,
       LPanel_ResolveViewType,          // NEW: maps "LPANEL_STATE_VIEW"/"LPANEL_DRAW_VIEW"
       NULL);
   if (!root) -> (fallback path, see below)
2. LPanel_AttachToggleSources(root);   // unchanged — STATE toggles get live ON/OFF sources
3. view_count = 0; xml_views = UILoader_ExtractViews(root, &view_count);
     // each View now carries its correct ViewType from the XML type= via the resolver
4. lpanel = ViewHostSystem_Create(&lpanel_viewport, 1.0f, (Vector2d){0.1f,0.1f},
                                  &ui_default_palette, ui_standard_stack_spacing);
5. ViewHostSystem_InitRoot(lpanel);     // builds the real UI_ELEMENT_ROOT + space + seed_box
                                        // (ViewHostSystem_Create left root=NULL, space={0}, seed_box={0})
6. AddElementToTree(root, lpanel->root); // re-parent the XML <ViewHost> container UNDER the real ROOT
                                         // NEVER `lpanel->root = root` — that is the dangling/zeroed-root bug
7. ViewHostSystem_InitViews(lpanel, view_count);
8. for each xml_views[i]: LArray_Push(&lpanel->views, &xml_views[i]);
   Deallocate the xml_views array (not the Views).
9. lpanel_view_selector = UILoader_BuildSelectorFromMarkup(lpanel,
                                   ViewHostSystem_HandleViewSelected);
10. if (lpanel_view_selector) {
       initial = UILoader_ResolveViewIndexById(lpanel, UILoader_GetInitialViewId());
       if (initial < 0) initial = 0;
       ViewHostSystem_SelectView(lpanel_view_selector, (size_t)initial);  // single styling + selection point
   } else {
       // Selector allocation failed. Establish single-view visibility manually,
       // since XML view containers are all enabled=true by default (Finding 5),
       // and SetPanelActiveView is static. Use the public enable/disable primitives.
       for (size_t i = 0; i < lpanel->views.count; i++) {
           View *v = *(View **)LArray_Get(&lpanel->views, i);
           if (i == 0) EnableElement(v->container); else DisableElement(v->container);
       }
   }
   // NOTE: never call ViewHostSystem_FinaliseInit here — it would force index 0
   // and fight the resolved initialView.
11. UpdateUISpace(lpanel->root, lpanel->seed_box);
```

The order of `ViewHostSystem_InitRoot` (step 5) relative to `ViewHostSystem_InitViews`
(step 7) is immaterial, but both must appear and `InitRoot` **must** run so the
host's coordinate space is valid. The dangling/zeroed-root issue is resolved by
never calling `ViewHostSystem_CreateStandard` (which builds a selector on an
about-to-be-replaced root) **and** by never assigning `lpanel->root = root`.
Instead `ViewHostSystem_InitRoot` builds the single real `UI_ELEMENT_ROOT` (with a
populated `space`/`seed_box`), and `AddElementToTree(root, lpanel->root)` re-parents
the adopted XML `<ViewHost>` container under that ROOT. The selector is built *from
that same adopted container's* Option buttons by `UILoader_BuildSelectorFromMarkup`.
There is only ever one root (the `ViewHostSystem`-built ROOT) and one selector, with
everything inside driven by the XML. See the Seed box / coordinate space note below
for the full rationale (this numbered flow IS the chosen Option B).

**Seed box / coordinate space note (important invariant):** today
`ViewHostSystem_CreateStandard` -> `ViewHostSystem_InitRoot` is what computes
`panel->space` and `panel->seed_box` from the viewport and *also* creates a
`UI_ELEMENT_ROOT` element. In the new flow the XML `<ViewHost>` container is adopted
as `lpanel->root`, but the space/seed-box computation must still run. The XML
`BuildViewHost` creates a plain container, **not** a `UI_ELEMENT_ROOT` with a
populated `data.root.space`. Two options:

- **(A)** Make `BuildViewHost` emit a `UI_ELEMENT_ROOT` element and have
  `InitLPanel` compute space/seed-box itself. Rejected: duplicates `InitRoot`'s
  space maths in the application and bakes root-ness into the generic loader.
- **(B, chosen)** Keep `BuildViewHost` producing a container, and have `InitLPanel`
  call `ViewHostSystem_InitRoot(lpanel)` to build the proper `UI_ELEMENT_ROOT` +
  space + seed box, then **re-parent the XML `<ViewHost>` container under that
  root** (append the adopted XML tree as the root's child) rather than overwriting
  `lpanel->root`. This preserves the host's coordinate-space contract (seed box,
  `data.root.space`, `root_child_spacing`) that `ViewHostSystem_Draw`/`UpdateUISpace`
  depend on, while the XML drives everything *inside* the root. Concretely (this is
  exactly steps 5–6 of the numbered flow above): after step 4's
  `ViewHostSystem_Create`, call `ViewHostSystem_InitRoot(lpanel)` (builds `lpanel->root` as a real
  ROOT with space + seed box), then call
  `AddElementToTree(xml_container, lpanel->root)` to link the adopted XML container
  as the ROOT's child. `AddElementToTree` (public in `ui.c`) sets
  `xml_container->parent = lpanel->root` and appends it as the ROOT's youngest
  sibling — exactly the parent + sibling linking required, so no manual pointer
  surgery is needed. Do **NOT** assign `lpanel->root = xml_container` — that is the
  current dangling-root bug this design removes. The views and selector live under
  the XML container, which lives under the ROOT.

  One further constraint restated here next to the attach call: `BuildViewHost`'s
  produced container must **NOT** be a `UI_ELEMENT_ROOT` (it is a plain
  `UI_ELEMENT_CONTAINER` from `CreateUIContainer`, per §2). If it were a ROOT it
  would carry its own uninitialised `data.root.space` and collide with the real
  ROOT built by `ViewHostSystem_InitRoot`, which owns the single valid coordinate
  space.

Option B keeps the `ViewHostSystem` drawing/layout invariants intact (they require
a `UI_ELEMENT_ROOT` with a valid `space`) and keeps the loader generic (it never
needs to know about `ViewportRegion` or `UISpace2d`). The ownership story is
unchanged: `ViewHostSystem_Destroy` disposes `panel->root`, which now transitively
owns the XML subtree.

The commented-out fallback (`InitLPanelStateView`/`InitLPanelEditView` +
`ViewHostSystem_CreateStandard`) is **removed** — see §8. If `root` is NULL
(XML load failed), log an error and leave `lpanel` NULL; `DrawLPanel` already
null-guards, so the panel simply does not render. This is acceptable because the XML
is now the authoritative, in-repo source and a load failure is a build/asset error
to be fixed, not something to silently paper over with a divergent hard-coded panel.

### 8. Stub cleanup (per symbol)

| Symbol | File | Action |
| --- | --- | --- |
| `BuildViewHost` | `ui_loader.c` | **Add** (new real builder, §2). Register for tag `ViewHost` — see the explicit `UILoader_RegisterDefaultBuilders` edits below. |
| `BuildView` | `ui_loader.c` | **Rewrite** to resolve `type=` via `resolve_view_type` and record `(container, ViewType, id)` in the side table (§3). |
| `BuildViewSelector` | `ui_loader.c` | **Rewrite** from empty stub to real selector-container builder that records the selector + Option targets (§4). Remove the TODO. |
| `BuildOption` | `ui_loader.c` | **Rewrite** from inert stub to real enumerate-button builder that reads `view=` and records the (button, view id) pair (§4). Remove the TODO. |
| `BuildPanel` | `ui_loader.c` | **Remove**. `Panel` is unused by `lpanel.xml` (root is now `ViewHost`); the stub misrepresents what it builds. Unregister the `Panel` tag. (Grep confirms no other XML uses `<Panel>`; if a later asset needs it, `ViewHost` now covers the root-container role.) |
| `BuildContainer` | `ui_loader.c` | **Keep but tidy**: it is a legitimate generic container used by `<Container>`. Replace the `// TODO: Extract size and spacing` with the exact extraction `BuildSection` uses — `UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, NULL)` plus `UILoader_ParseLayout` for spacing — defaulting to `ui_standard_container_size` when `size` is absent. This only honours attributes **when present**; it is **not** a behavioural change to existing `<Container>` handling (and `lpanel.xml` has no `<Container>` users to regress). |
| `UILoader_CollectViewContainers` | `ui_loader.c` | **Edit**: replace `view->type = LPANEL_STATE_VIEW` with a side-table lookup for the real `ViewType` (§3). |
| `view_indices` trivial fill in `AllocatePanelViewSelector` | `view_host_system.c` | **Unchanged** for the C-built path; the new `UILoader_BuildSelectorFromMarkup` sets `view_indices[i]` to the *resolved* index instead (does not reuse `AllocatePanelViewSelector`, which assumes identity mapping). |
| `InitLPanelStateView`, `InitLPanelEditView`, `InitEntityCreateDefaults` (as fallback), commented fallback block in `InitLPanel` | `lpanel_system.c` | **Remove.** Grep-gate result recorded: `InitLPanelStateView`/`InitLPanelEditView` are declared/defined in `lpanel_system.c` and referenced **only** from the commented-out fallback block — safe to remove together with their now-unused file-static toggle tables. `InitEntityCreateDefaults` is `static`, called only from the (removed) `InitLPanelEditView` — safe to remove. Removing the edit-view builder leaves the `G_UIState.edit_*_tbox` pointers permanently NULL, which is safe (see the Out-of-Scope "Verified NULL-safe" note: `RefreshEntityEditorFields` -> `RefreshTextboxFields` skips NULL textbox rows). Prefer removal over leaving dead TODO-style stubs, per the project rules. |

#### Explicit `UILoader_RegisterDefaultBuilders` edits (required)

The builder registry is the gate: `UILoader_ParseElementNode` drops any subtree
whose tag has no registered builder (`UILoader_FindBuilder == NULL` logs "Unknown
XML element type"). Because `<ViewHost>` is the root of `lpanel.xml`, failing to
register it drops the **entire** tree. So `UILoader_RegisterDefaultBuilders` must
be edited to:

- **Add** `UILoader_RegisterBuilder("ViewHost", BuildViewHost);`
- **Remove** `UILoader_RegisterBuilder("Panel", BuildPanel);`

The removal is safe: a `grep` for `<Panel` across `**/*.xml` returns no matches,
so no asset depends on the `Panel` tag; `ViewHost` now covers the root-container
role. `MAX_REGISTERED_BUILDERS` (64) is not a constraint — the net builder count
is unchanged (one added, one removed). The remaining registrations
(`ViewSelector`, `Option`, `View`, `Section`, `Container`, `Button`, `Label`,
`TextField`) are untouched.

### Attribute handling summary

| Element | Attribute | Required | Handling |
| --- | --- | --- | --- |
| `ViewHost` | `id` | optional | extracted (documentation / future lookup) |
| `ViewHost` | `viewport`, `scale` | optional | **not resolved** by loader; `InitLPanel` supplies real viewport/scale |
| `ViewHost` | `layout` | optional | parsed via `UILoader_ParseLayout` (`"stacked"` aliased to stack) |
| `ViewHost` | `initialView` | optional | recorded; `InitLPanel` resolves to index; default index 0 |
| `ViewSelector` | `id` | optional | extracted |
| `ViewSelector` | `type` | optional | `"enumerate"` (default). `"hover"` reserved for a future hover selector; unknown => enumerate + warning |
| `Option` | `text` | required | button label (empty string if absent) |
| `Option` | `view` | required | target View `id`; resolved to index at finalisation; unresolved => index 0 + warning |
| `View` | `id` | optional-but-needed | preserved for Option/initialView resolution; a View with no `id` cannot be an Option target (warn if an Option references it) |
| `View` | `type` | optional | resolved via `resolve_view_type`; absent/unknown => first ViewType + warning |
| `View` | `scrollable` | optional | `"true"` enables scroll (unchanged) |

### Error handling (per failure point)

- **ViewHost tag not registered / build returns NULL** (should not happen once
  registered): `UILoader_ParseElementNode` logs "Unknown XML element type" and drops
  the subtree; `root==NULL` propagates to `InitLPanel`, which logs an error and
  leaves `lpanel=NULL`. Fatal-for-panel, recoverable-for-app (no render). Logged at
  error level.
- **`resolve_view_type` absent or returns `resolved=false`**: `BuildView` defaults
  the view to the first `ViewType` (0) and logs `LOADER_WARNING` "Unrecognised view
  type". Recoverable; the view still renders but keys to the wrong `active_view` —
  the warning surfaces the misconfiguration.
- **Option `view=` unresolved to a view index**: `UILoader_BuildSelectorFromMarkup`
  defaults that option to index 0 and logs a warning. Recoverable.
- **`initialView` unresolved**: per §6 (the single normative statement of this
  rule) — default to index 0, warning only on an unresolved *non-empty* value.
  Recoverable.
- **Side table overflow** (`> MAX_LOADED_VIEWS` views or options in one load): log a
  warning and ignore the overflow entries; views beyond the cap won't be
  type-resolved. Non-fatal; cap is set generously (32) above the two views in use.
- **Allocation failure** in `UILoader_BuildSelectorFromMarkup`
  (`buttons`/`view_indices`): free whatever was allocated (mirror
  `AllocatePanelViewSelector`'s cleanup), return NULL; `InitLPanel` then skips
  `ViewHostSystem_SelectView`. **Important:** on the XML path `BuildView` creates
  every view container with `enabled=true` (unlike the old C path, which
  explicitly `DisableElement`'d the DRAW container), so single-view visibility is
  **not** a default — it is *established by selection*
  (`ViewHostSystem_SelectView` -> `SetPanelActiveView` enables the chosen
  container and disables the rest; `SetPanelActiveView` is `static` to
  `view_host_system.c` and is not reachable from `InitLPanel`). Therefore
  selection is REQUIRED, and in this NULL-selector fallback `InitLPanel` must
  explicitly re-create single-view visibility using the public `EnableElement`/
  `DisableElement` primitives (the same ones the old C path used): loop over
  `lpanel->views`, `EnableElement(views[0]->container)` and
  `DisableElement(views[i]->container)` for `i > 0`. Without this, STATE and DRAW
  would both render stacked. Logged at error level.
- **XML parse / file-not-found**: unchanged — `UILoader_LoadFromFileWithResolvers`
  returns NULL, `InitLPanel` logs error, panel inert.

### Validation rules for external inputs (the XML attributes)

- `Option.text`: string, optional (default `""`), no length limit beyond
  `String64` truncation already applied by the button constructor.
- `Option.view` / `View.id` / `ViewHost.initialView`: strings compared by
  `strcmp`; truncated to `MAX_UI_ELEMENT_ID` on extraction. Unmatched => documented
  index-0 fallback + warning.
- `View.type`: string passed verbatim to the app resolver; the app owns the valid
  set.
- `ViewSelector.type`: enumerated `{enumerate, hover}`; unknown => enumerate +
  warning.
- `ViewHost.scale`: not consumed by loader; `InitLPanel` uses a literal `1.0f`
  (matching today). Not validated.

### Invariant ownership

- **"Every View has a correct `ViewType`"**: owned by the loader's parse pass
  (`BuildView` + side table) feeding `UILoader_CollectViewContainers`. This is the
  layer that reads the XML, so it is the correct enforcement point. `InitLPanel`
  does not re-stamp types.
- **"Exactly one root with a valid coordinate space"**: owned by
  `ViewHostSystem_InitRoot` (called from `InitLPanel`), not the loader — the loader
  stays viewport-agnostic. The XML host container is re-parented under this root.
- **"`view_indices[i]` addresses stay stable for the selector's lifetime"**: owned
  by `ViewSelector` (heap arrays, freed in `DestroyPanelViewSelector`). Unchanged
  from the existing C path.
- **"`on_view_selected` sets `G_UIState.active_view`"**: owned by lpanel via passing
  `ViewHostSystem_HandleViewSelected` as the callback. The loader does not touch
  `G_UIState`.

### Known interactions preserved

- **`geometry_editor.c` / `ui_system.c` reading `active_view == LPANEL_DRAW_VIEW`**:
  works because (a) `draw_view`'s `type="LPANEL_DRAW_VIEW"` now resolves to the real
  `LPANEL_DRAW_VIEW` on the `View` struct, and (b) selecting DRAW calls
  `ViewHostSystem_HandleViewSelected`, which sets `G_UIState.active_view =
  view->type`. Previously every view was `LPANEL_STATE_VIEW`, so DRAW never set the
  draw state — this change fixes that.
- **STATE toggle ON/OFF labels**: `LPanel_AttachToggleSources(root)` still runs in
  step 2, before the selector/space work, on the adopted XML tree. The STATE
  buttons' command-sink bindings (built by `BuildButton` from `action=`) are
  untouched, so clicks still dispatch and the query-source still composes
  "<text>: ON/OFF".
- **Size-mode fix**: `BuildView`/`BuildViewHost`/`BuildViewSelector` do not alter the
  Sections, which keep `size-mode="content_fill"` with no `size` attr; the existing
  `UILoader_ExtractCommonAttrs` honours size-mode-without-size. No regression to
  STATE button rendering.
- **Command-sink button bindings**: `BuildButton` unchanged; STATE action buttons
  render and click as before.

### Testability

- **Unit-testable (string-only, no raylib window):** `resolve_view_type` mapping
  (string -> ViewType); the id->index resolution in
  `UILoader_BuildSelectorFromMarkup` given a synthetic `host->views` with known
  container `id`s; `UILoader_ResolveViewIndexById`; the side-table overflow/clear
  behaviour across two sequential loads. These take plain inputs and return codes.
- **Integration-testable (needs the UI tree, headless-constructible):** load
  `lpanel.xml` via `UILoader_LoadFromStringWithResolvers` passing a **real** stub
  view-type resolver (not NULL) that maps `"LPANEL_STATE_VIEW" -> 0` and
  `"LPANEL_DRAW_VIEW" -> 1` (setting `*resolved = true`), relying on the extended
  string-variant signature from §3. Then assert: two `UI_ELEMENT_VIEW` containers
  extracted; `views[0].type == LPANEL_STATE_VIEW`, `views[1].type ==
  LPANEL_DRAW_VIEW` (these pass only because the stub resolver is supplied — with a
  NULL resolver both would default to index 0 and the second assertion would fail,
  which is why §3 extends the string variant too); the selector has 2 buttons with
  `data_bind == selector` and `*(int*)user_data` equal to {0,1}; selecting index 1
  enables the draw container, disables state, and sets `G_UIState.active_view ==
  LPANEL_DRAW_VIEW`; selecting 0 reverses it.
- **Hard to unit-test (manual/visual):** actual pixel rendering of the DRAW
  TextFields and selector button styling — covered by running the app (only on
  explicit request per project rules).

The design is testable without the full game loop because selection, type
resolution, and index mapping are pure data operations on the host/selector structs;
that is a sign the decomposition is sound.

## Out of Scope (explicit, intentional gap)

- **DRAW view field data-wiring.** The `<TextField>` controls in `draw_view`
  (`vertices`, `width`, ..., `moment`) **render** and the view is **selectable**, but
  their `G_UIState.edit_*_tbox` pointers are **not** populated and the fields are
  **not** bound to `entity_create_params`. The loader has no `id -> G_UIState`
  registration mechanism, and building one is a deliberate follow-up — **not** part
  of this pass. `BuildTextField` continues to create labelled fields via
  `CreateUILabeledFieldDefault` and to resolve `binding=` through
  `LPanel_ResolveBinding` for the data *address*, but the named `G_UIState` textbox
  pointers (used by `InitEntityCreateDefaults`/`RefreshEntityEditorFields`) are left
  NULL. Consequently the CREATE button will dispatch `create-entity`, but the fields
  are not pre-seeded with defaults nor read back into `G_UIState.edit_*_tbox`. This
  is a known, intentional limitation to be addressed when an `id->G_UIState`
  registration mechanism is added.
  - **Verified NULL-safe (no DRAW-selection crash).** Leaving the
    `G_UIState.edit_*_tbox` pointers NULL is safe: `ui_system.c`'s
    `RefreshEntityEditorFields` (invoked every frame via `UpdateGlobalUIState`
    when `active_view == LPANEL_DRAW_VIEW`) builds a `TextboxField` array whose
    `.textbox` members are those NULL pointers and passes it to
    `RefreshTextboxFields` (`integration_system.c`), which begins each iteration
    with `if (!field->textbox) continue;`. So every row with a NULL textbox is
    skipped and nothing dereferences the NULL pointers. The per-frame refresh is
    therefore a safe **no-op** on the XML path until the `id->G_UIState`
    registration system exists — this out-of-scope gap introduces no regression,
    not even a crash when DRAW is selected.
- **Building that `id->G_UIState` registration system.** Explicitly not in this pass.
- **Hover-style selectors from XML.** `ViewSelector type="hover"` is reserved in the
  attribute table but only `enumerate` is wired now.
- **rpanel / state-manager / utility-panel migration to XML.** This change targets
  the left panel only; the generic loader additions are reusable by them later but
  they are not touched here.

## Build / git / behaviour statement

- **Git untouched.** This is a design document only. No code is edited, no files are
  staged, committed, rebased, or otherwise altered in git by this phase. Work stays
  in-place on the `ui-overhaul` branch; the user commits manually.
- **Behaviour preserved for the STATE view:** its debug toggle buttons render, click
  (dispatch their `toggle-*` commands), and show live `ON/OFF` labels via
  `LPanel_AttachToggleSources`. Section `size-mode="content_fill"` layout and the
  command-sink bindings are unchanged.
- **DRAW view is selectable:** selecting DRAW enables the draw container, disables
  STATE, renders the TextFields, and sets `G_UIState.active_view ==
  LPANEL_DRAW_VIEW` (keeping `geometry_editor.c` and `ui_system.c` working);
  selecting STATE reverses it. DRAW's fields are **not** data-wired (see Out of
  Scope).

## Summary of files to change (implementation phase, not now)

- `include/system/ui/ui_loader.h`: add the `UIViewTypeResolver` typedef and the
  `UIViewTypeResolver resolve_view_type;` field on `struct UILoaderContext`; extend
  **both** `UILoader_LoadFromFileWithResolvers` **and**
  `UILoader_LoadFromStringWithResolvers` signatures with the trailing
  `UIViewTypeResolver resolve_view_type` parameter; declare
  `UILoader_BuildSelectorFromMarkup`, `UILoader_ResolveViewIndexById`,
  `UILoader_GetInitialViewId`.
- `src/engine/system/ui/ui_loader.c`: in `UILoader_RegisterDefaultBuilders` add
  `UILoader_RegisterBuilder("ViewHost", BuildViewHost);` and remove
  `UILoader_RegisterBuilder("Panel", BuildPanel);`; assign
  `ctx.resolve_view_type = resolve_view_type` in **both**
  `UILoader_LoadFromFileWithResolvers` and `UILoader_LoadFromStringWithResolvers`
  (alongside the existing `.resolve_binding`/`.resolve_command` assignments), and
  pass `NULL` from the no-resolver `UILoader_LoadFromFile`/`UILoader_LoadFromString`
  wrappers; add `BuildViewHost`; rewrite `BuildView`, `BuildViewSelector`,
  `BuildOption`; tidy `BuildContainer`; remove `BuildPanel`; add the per-load side
  table + its clear-on-load; replace the hard-coded `view->type` in
  `UILoader_CollectViewContainers`; implement the three new post-process functions;
  add `"stacked"` alias in the shared `UILoader_ParseLayout`.
- `src/engine/system/ui/lpanel_system.c`: add `LPanel_ResolveViewType`; rewrite
  `InitLPanel` (adopt XML host + selector, resolve `initialView`, re-parent under a
  real root via `ViewHostSystem_InitRoot`); remove the commented fallback block and
  the now-dead `InitLPanelStateView`/`InitLPanelEditView`/`InitEntityCreateDefaults`
  (grep-gated).
- `src/engine/ui/components/lpanel.xml`: already has both views live; keep
  `draw_view` uncommented (defensive requirement 5). No structural change required
  beyond what already exists.
- `src/engine/system/view_host_system.{h,c}`: **no change required.** The new
  selector wiring lives in the loader and reuses the existing public primitives
  `HandleViewHostSelectorClick`, `ViewHostSystem_SelectView`,
  `ViewHostSystem_InitRoot`, `DestroyPanelViewSelector`, and the public
  `ViewSelector`/`ViewHostSystem` structs. `UpdatePanelViewSelectorButtons` and
  `SetPanelActiveView` stay `static`; styling runs via the single mandatory
  `ViewHostSystem_SelectView` call in `InitLPanel` (§5 step 4 / §7 step 9), and the
  NULL-selector fallback uses the public `EnableElement`/`DisableElement`
  primitives rather than the static `SetPanelActiveView`. No new public styling
  helper is introduced.

---

## Responses to design review findings

This section records how each finding in
`.agents/tasks/xml-viewhost-design-review.md` (and its `.json`) was handled. All
HIGH and MEDIUM findings are addressed; the NITs are addressed too.

- **Finding 1 (HIGH) — resolver location contradiction.** ADDRESSED. §3 now states
  one unambiguous plan: add BOTH the `UIViewTypeResolver` typedef and a
  `resolve_view_type` field on `UILoaderContext`, and thread the resolver through
  the entry-point signatures that assign it into the stack-local `ctx` exactly like
  `resolve_binding`/`resolve_command`. `BuildView` reads `ctx->resolve_view_type`,
  which is valid because the field exists. The "a field is awkward" sentence was
  deleted. The file-change summary was aligned.
- **Finding 2 (HIGH) — string-variant lacks a view-type resolver.** ADDRESSED. §3
  extends **both** `UILoader_LoadFromFileWithResolvers` and
  `UILoader_LoadFromStringWithResolvers` with the trailing resolver parameter. The
  Testability section now explicitly passes a real stub resolver
  (`LPANEL_STATE_VIEW->0`, `LPANEL_DRAW_VIEW->1`) through the string variant, so the
  `views[1].type == LPANEL_DRAW_VIEW` assertion is runnable.
- **Finding 3 (MEDIUM) — selector array element types.** ADDRESSED. §5 step 2 now
  states `view_indices = AllocateBytes(sizeof(int) * count)` and
  `buttons = AllocateBytes(sizeof(UIElement *) * count)`, mirroring
  `AllocatePanelViewSelector` so `DestroyPanelViewSelector`'s `sizeof(int)` /
  `sizeof(UIElement*)` frees match, with `user_data = &selector->view_indices[i]`
  (an `int*`) matching `HandleViewHostSelectorClick`.
- **Finding 4 (MEDIUM) — single styling/selection call site.** ADDRESSED. §5 step 4
  now pins the sequence: `InitLPanel` calls `ViewHostSystem_SelectView` exactly
  once (§7 step 9) and never calls `ViewHostSystem_FinaliseInit`. `SelectView` is
  the single styling + initial-selection point; the transient `active_index ==
  count` sentinel is always overwritten except on the explicit NULL-selector
  fallback.
- **Finding 5 (MEDIUM) — both containers enabled without selection.** ADDRESSED.
  The Error-handling bullet and §7 fallback now state that XML view containers are
  all `enabled=true`, so single-view visibility is established by selection; the
  NULL-selector fallback explicitly enables index 0 and disables the rest via the
  public `EnableElement`/`DisableElement` primitives (not the static
  `SetPanelActiveView`).
- **Finding 6 (MEDIUM) — builder registration not explicit.** ADDRESSED. §8 gains a
  dedicated "Explicit `UILoader_RegisterDefaultBuilders` edits" subsection:
  add `UILoader_RegisterBuilder("ViewHost", BuildViewHost);`, remove
  `UILoader_RegisterBuilder("Panel", BuildPanel);` (no `<Panel>` in any XML), and
  it is restated in the file-change summary.
- **Finding 7 (MEDIUM) — container-pointer linchpin + id-less Views.** ADDRESSED.
  §5 step 1 now states the invariant that the side-table container pointers are
  identical to `host->views[k]->container` (because
  `UILoader_CollectViewContainers` sets `view->container = elem` without copying),
  and that an id-less View can never match an Option `view=`, falling to the
  index-0 default + warning.
- **Finding 8 (MEDIUM) — re-parent primitive named.** ADDRESSED. §7 Option B now
  names `AddElementToTree(xml_container, lpanel->root)` after
  `ViewHostSystem_InitRoot(lpanel)`, forbids `lpanel->root = xml_container`, and
  restates that `BuildViewHost`'s container must not be a `UI_ELEMENT_ROOT`.
- **Finding 9 (MEDIUM) — stub removal gate + DRAW NULL-safety.** ADDRESSED.
  Verified against source: `RefreshEntityEditorFields` -> `RefreshTextboxFields`
  skips every row whose `.textbox` is NULL (`if (!field->textbox) continue;`), so
  leaving `edit_*_tbox` NULL is a safe no-op (recorded in Out of Scope). The §8
  table records the grep-gate result confirming
  `InitLPanelStateView`/`InitLPanelEditView`/`InitEntityCreateDefaults` are
  referenced only by the removed fallback and are safe to delete.
- **Finding 10 (NIT) — initialView duplication.** ADDRESSED. §6 is the single
  normative statement; the Error-handling bullet now references it.
- **Finding 11 (NIT) — BuildContainer tidy scope.** ADDRESSED. §8 names the exact
  calls (`UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, NULL)` +
  `UILoader_ParseLayout`, default `ui_standard_container_size`), with no
  default-behaviour change.
- **Finding 12 (NIT) — stacked alias scope.** ADDRESSED. §2 notes the
  `stacked -> stack` alias is a global change to the shared `UILoader_ParseLayout`,
  applying to every element that parses `layout=`.

### Round 2 review (`xml-viewhost-design-review.json` / `-review.md`)

- **R2 Finding 1 (HIGH) — §7 numbered flow (old step 6 `lpanel->root = root`)
  contradicted the chosen Option B and skipped `ViewHostSystem_InitRoot` +
  `AddElementToTree`, leaving the root's `space`/`seed_box` zeroed.** ADDRESSED.
  §7's numbered flow was rewritten so the authoritative step list *is* Option B:
  step 5 now calls `ViewHostSystem_InitRoot(lpanel)` (builds the real
  `UI_ELEMENT_ROOT` with populated `space`/`seed_box`), step 6 calls
  `AddElementToTree(root, lpanel->root)` to re-parent the XML `<ViewHost>`
  container under that ROOT with an explicit "NEVER `lpanel->root = root`" comment,
  and the remaining steps renumbered to 7–11. The prose sentence "we assign the XML
  root directly" was removed and replaced with text that states the ROOT is built
  by `ViewHostSystem_InitRoot` and the XML container is re-parented under it, with
  an explicit note that this numbered flow IS the chosen Option B. The Option B
  note's cross-reference now points at steps 5–6 so the two descriptions agree.
  No other round-2 findings (MEDIUM/NIT) were raised.
