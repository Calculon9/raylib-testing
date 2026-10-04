# Design Review: XML-authoritative ViewHost + View selection for the left panel

Reviewed document: `.agents/tasks/xml-viewhost-design.md`
Reviewer: design-review subagent (fresh read, no authoring context)
Verdict: **APPROVED** (0 HIGH, 0 MEDIUM; NITs only)

This design has already been through two prior review rounds (recorded in the
document's "Responses to design review findings" and "Round 2 review" sections).
This review was performed independently against the source, re-verifying the
claims rather than trusting the recorded responses. Every claim I could check
against the code holds. The remaining items are NITs that do not block
implementation.

---

## Gate assessment (all required items)

Each gate item from the task brief, with the verification outcome:

1. **XML authoritative — no hardcoded `labels[]` left in `InitLPanel`.** PASS.
   The current `InitLPanel` hardcodes `const char *labels[] = {"STATE", "DRAW"}`
   and calls `ViewHostSystem_CreateStandard` (verified in
   `lpanel_system.c`). §7 removes both; the selector is built from the XML
   `<Option>`s via `UILoader_BuildSelectorFromMarkup`.
2. **View types parsed per view — no blanket `LPANEL_STATE_VIEW` hardcode.** PASS.
   The hardcode exists today at `UILoader_CollectViewContainers`
   (`view->type = LPANEL_STATE_VIEW;` verified in `ui_loader.c`). §3 + §8 replace
   it with a side-table lookup fed by `BuildView` + the app resolver.
3. **Options wired to `ViewHostSystem_SelectView` with view-id -> view-index
   resolution handling options-before-views ordering.** PASS. §5 is a two-pass
   post-process (`UILoader_BuildSelectorFromMarkup`) that resolves after all views
   are known, keyed on container-pointer identity, mirroring
   `ViewHostSystem_CreateViewSelector`'s wiring
   (`HandleViewHostSelectorClick` / `data_bind=selector` /
   `user_data=&view_indices[i]`), all verified public in
   `view_host_system.{h,c}`.
4. **active_view tracks selection (DRAW works for geometry_editor.c /
   ui_system.c).** PASS. `ViewHostSystem_HandleViewSelected` sets
   `G_UIState.active_view = view->type` (verified), so DRAW's resolved
   `type="LPANEL_DRAW_VIEW"` propagates. `geometry_editor.c` and `ui_system.c`
   both compare `active_view == LPANEL_DRAW_VIEW` (verified).
5. **STATE view behaviour preserved (buttons render + click, ON/OFF toggles via
   `LPanel_AttachToggleSources`, size-mode content_fill).** PASS.
   `LPanel_AttachToggleSources(root)` still runs (§7 step 2) on the adopted tree;
   `BuildButton` is unchanged; the STATE sections keep
   `size-mode="content_fill"` and `UILoader_ExtractCommonAttrs` already honours
   size-mode-without-size (verified).
6. **DRAW selectable with TextFields rendering.** PASS. `BuildTextField`
   unchanged; the `draw_view` is already live in `lpanel.xml` (verified).
7. **Loader stays generic (app supplies ViewType mapping; no lpanel enum names in
   the generic loader).** PASS. §3 uses a `UIViewTypeResolver` callback matching
   the existing `resolve_binding`/`resolve_command` pattern; no `LPANEL_*` names
   are added to `ui_loader.c`.
8. **No dead stub builders left.** PASS. §8 rewrites `BuildView` /
   `BuildViewSelector` / `BuildOption`, adds `BuildViewHost`, removes
   `BuildPanel`, tidies `BuildContainer`, and removes dead lpanel fallbacks. No
   `<Panel>` or `<Container>` exists in any XML (verified via grep), so
   `BuildPanel` removal and `BuildContainer` tidy regress nothing.
9. **Git untouched.** PASS. Design-only; explicitly stated.
10. **Out-of-scope DRAW-field-wiring documented as deliberate follow-up (no
    id->G_UIState registration this pass).** PASS, and the NULL-safety claim is
    independently verified: `RefreshEntityEditorFields` (ui_system.c) builds a
    `TextboxField[]` from the (NULL) `G_UIState.edit_*_tbox` pointers and
    `RefreshTextboxFields` (integration_system.c) begins each row with
    `if (!field->textbox) continue;` before any dereference — confirmed in source.
11. **ViewHost builder / root-handling / dangling-root resolution unambiguous.**
    PASS. §7's numbered flow is the authoritative Option B: `ViewHostSystem_Create`
    -> `ViewHostSystem_InitRoot(lpanel)` (builds the single real `UI_ELEMENT_ROOT`
    with populated `space`/`seed_box`, verified) -> `AddElementToTree(root,
    lpanel->root)` to re-parent the XML `<ViewHost>` container under that ROOT,
    with an explicit "NEVER `lpanel->root = root`". `AddElementToTree(e, parent)`
    sets `e->parent = parent` and appends as youngest sibling (verified in
    `ui.c`), matching the argument order used in the flow.

All gate items pass. No blocking finding exists on root-handling or
dangling-root resolution.

---

## Findings

1. **NIT — `UILoader_ResolveViewIndexById` algorithm is less fully specified than
   `UILoader_BuildSelectorFromMarkup`.** §6 states the function "uses the side
   table / host views" and returns an index (`<0` when unresolved, per the §7
   step 10 `if (initial < 0)` guard), but it does not spell out the exact lookup
   chain the way §5 step 1 does for Options. The intended chain is: id -> matching
   side-table row -> that row's container pointer -> scan `host->views` for the
   `View` whose `container` equals it -> that index. CONCRETE FIX: add one
   sentence to §6 stating that `UILoader_ResolveViewIndexById` resolves the id
   through the same side-table-container-identity path as §5 step 1, and that an
   unmatched id returns a negative sentinel (which §7 step 10 maps to index 0).
   Non-blocking because the mechanism is fully defined in §5 and the invariant in
   §5 step 1 covers the identity comparison.

2. **NIT — `active_index == count` sentinel passed to `UpdatePanelViewSelectorButtons`
   is safe but worth an explicit note.** §5 step 4 leaves the freshly built
   selector at `active_index == count` until the mandatory
   `ViewHostSystem_SelectView` call overwrites it. `UpdatePanelViewSelectorButtons`
   (verified `static`) loops `i < count` and compares `i == active_index`, so with
   `active_index == count` no button is marked active and no out-of-bounds access
   occurs — this matches how `AllocatePanelViewSelector` already initialises
   `active_index = count` (verified). CONCRETE FIX: none required for correctness;
   optionally note in §5 that the sentinel is index-safe in
   `UpdatePanelViewSelectorButtons` because it is never used as an array index,
   only an equality test.

3. **NIT — allocation-failure cleanup in `UILoader_BuildSelectorFromMarkup` must
   use the public `Deallocate`, not the static `DestroyPanelViewSelector`.** §5 /
   Error handling says "mirror `AllocatePanelViewSelector`'s cleanup". Since
   `DestroyPanelViewSelector` and `AllocatePanelViewSelector` are both `static` to
   `view_host_system.c` (verified), the new loader function cannot call them; it
   must open-code the `Deallocate((void**)&buttons, sizeof(UIElement*)*count)` /
   `Deallocate((void**)&view_indices, sizeof(int)*count)` / `Deallocate((void**)&
   selector, sizeof(ViewSelector))` sequence itself (all of which are public).
   CONCRETE FIX: the design already implies open-coding ("mirror ... cleanup");
   state explicitly that the mirror is a hand-written `Deallocate` sequence
   because `DestroyPanelViewSelector` is not visible to the loader. Non-blocking —
   the behaviour is correct as described; this only sharpens the wording.

---

## Verified assumptions

- `lpanel.xml` root is `<ViewHost id="left_panel" viewport="lpanel_viewport"
  scale="1.0" layout="stacked" initialView="state_view">` with one
  `<ViewSelector type="enumerate">` (two `<Option>`s: STATE->state_view,
  DRAW->draw_view), a `<View id="state_view" type="LPANEL_STATE_VIEW">`, and a
  `<View id="draw_view" type="LPANEL_DRAW_VIEW">`. `draw_view` is already
  uncommented/live. VERIFIED (file read).
- `ui_loader.c` registers Panel, ViewSelector, Option, View, Section, Container,
  Button, Label, TextField — NOT ViewHost; unknown tags log "Unknown XML element
  type" and drop the subtree in `UILoader_ParseElementNode`. VERIFIED.
- `BuildView` extracts `view_id` and `type=` but discards both and sets
  `container->type = UI_ELEMENT_VIEW`. VERIFIED.
- `BuildViewSelector`, `BuildOption`, `BuildPanel`, `BuildContainer` are
  stub/TODO builders; `BuildOption` creates an inert
  `UI_ELEMENT_BUTTON_ENUMERATE` with `(NULL,NULL,NULL)` handlers and ignores
  `view=`. VERIFIED.
- `UILoader_CollectViewContainers` allocates a `View` per `UI_ELEMENT_VIEW`,
  sets `view->container = elem` and hard-codes `view->type = LPANEL_STATE_VIEW`.
  VERIFIED.
- `UILoader_ExtractViews(root, &count)` returns the `View*` array. VERIFIED.
- `ViewSelector { ViewHostSystem *panel; UIElement **buttons; int *view_indices;
  size_t count; size_t active_index; ViewSelectionCallback on_view_selected; }`.
  VERIFIED (header).
- `HandleViewHostSelectorClick` is public, reads selector from
  `button->data.button.data_bind` and index from `*(int*)user_data`, then calls
  `ViewHostSystem_SelectView`. VERIFIED.
- `ViewHostSystem_SelectView` -> `SetPanelActiveView` (static; enables chosen
  container, disables rest) -> `UpdatePanelViewSelectorButtons` (static) ->
  `on_view_selected(view)`. VERIFIED.
- `ViewHostSystem_HandleViewSelected` sets `G_UIState.active_view = view->type`.
  VERIFIED.
- `AllocatePanelViewSelector` (static) sets `view_indices[i] = (int)i`,
  `active_index = count`, uses `AllocateBytes`. `DestroyPanelViewSelector` (static)
  frees `buttons` (`sizeof(UIElement*)*count`) and `view_indices`
  (`sizeof(int)*count`). VERIFIED — so the selector-array element-type pinning in
  §5 step 2 is correct and teardown stays balanced.
- `ViewHostSystem_InitRoot` builds the single `UI_ELEMENT_ROOT`, computes
  `space`/`seed_box` from the viewport, and sets `root->data.root.space`.
  `ViewHostSystem_Create` leaves `root=NULL`, `space={0}`, `seed_box={0}`.
  VERIFIED — so the Option B requirement that `InitRoot` must run is real.
- `AddElementToTree(UIElement *e, UIElement *parent)` sets `e->parent = parent`
  and appends `e` as the parent's youngest sibling. The §7 call
  `AddElementToTree(root, lpanel->root)` matches this (element first, parent
  second). VERIFIED.
- `EnableElement` / `DisableElement` are public (set `is_enabled`), usable by the
  NULL-selector fallback in §7 step 10. VERIFIED.
- `ViewType` enum's first value is `LPANEL_STATE_VIEW` (== 0), then
  `LPANEL_DRAW_VIEW`. So "default to first ViewType" == `LPANEL_STATE_VIEW`.
  VERIFIED (`ui_state.h`).
- `ui_system.c` initialises `G_UIState.active_view = LPANEL_STATE_VIEW`;
  `geometry_editor.c` and `ui_system.c` gate on `active_view == LPANEL_DRAW_VIEW`.
  VERIFIED.
- `RefreshEntityEditorFields` runs every frame via `UpdateGlobalUIState` and
  `RefreshTextboxFields` skips NULL `.textbox` rows with `continue;` before any
  dereference. So leaving `edit_*_tbox` NULL on the XML path is crash-safe.
  VERIFIED.
- `UILoader_LoadFromFileWithResolvers` has exactly one caller (`InitLPanel`);
  `UILoader_LoadFromStringWithResolvers` has no current callers. So extending both
  signatures with a trailing `UIViewTypeResolver` is safe (grep). VERIFIED.
- No `<Panel>` or `<Container>` tag appears in any `*.xml`. So removing
  `BuildPanel`/unregistering `Panel` and tidying `BuildContainer` regress nothing.
  VERIFIED (grep).
- `UILoaderContext` currently holds `resolve_binding`/`resolve_command` assigned
  as struct-literal fields in each entry point; adding `resolve_view_type`
  alongside them is a direct, consistent extension. VERIFIED (header + .c).
- `UILoader_ParseLayout` is the single shared layout parser (used by
  `BuildSection`, `BuildContainer`, and to-be `BuildViewHost`) and does NOT
  currently recognise `"stacked"` (falls through to the stack default). So the
  `stacked -> stack` alias is genuinely a global change, as §2/§12 state.
  VERIFIED.

## Unverified / wrong assumptions

- None found. Every claim the design relies on was checkable against the source
  and held. The design even corrects the task brief's stale "draw_view is
  commented-out" note (the file has it live), which matches what I observed.
