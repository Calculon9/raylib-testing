# XML-authoritative ViewHost and view selection for the left panel

The left panel previously declared its structure twice: `lpanel.xml` described a `<ViewHost>`/`<ViewSelector>`/`<View>` tree that the loader mostly discarded (unknown `ViewHost` tag dropped the subtree, `BuildViewSelector`/`BuildOption` were inert stubs, `BuildView` threw away `type=`), while the real panel was assembled imperatively in `InitLPanel` with hard-coded `{"STATE","DRAW"}` labels and a selector built on a root that was then overwritten (the dangling-root bug). This change makes the XML the single authoritative source: a new `BuildViewHost` builder, a real `BuildViewSelector`/`BuildOption`, a per-load side table that carries each `<View>`'s resolved `ViewType` and `id` plus each `<Option>`'s target, and three post-process loader functions that wire a working selector after all views are known. `InitLPanel` now adopts the XML tree under a real `ViewHostSystem` root, builds the selector from markup, and honours `initialView`. View-type strings are resolved by an application-supplied callback so the generic loader never names `LPANEL_*` enums.

Watch for: a pre-existing staged rename (`panel_system.{h,c}` -> `view_host_system.{h,c}`) sits in the index — not produced by this task but present in the tree the human will commit (confirmed, non-blocking). The DRAW field data-wiring gap is explicitly out of scope and must not be treated as a defect. No functional defects were found in the task's four files.

**Verdict**: APPROVED

## High-level view

The core design choice is a two-pass build: the loader's single parse pass cannot wire the selector because `<Option>`s precede `<View>`s in document order and the host's view array does not exist until after the whole tree is built. The builders therefore record metadata into a loader-internal, fixed-capacity side table (cleared at the start of every load), and `UILoader_BuildSelectorFromMarkup` consumes it afterwards to resolve each option's target id to a view index and wire the enumerate buttons. This is sound because the container pointers stored in the table are the exact `UIElement*` pointers that end up as `host->views[k]->container`, so id-to-index resolution is a direct pointer comparison with no copy.

The generic/application boundary is respected. The loader never names `LPANEL_*` values: `<View type=>` is resolved through a new `UIViewTypeResolver` callback threaded through the two resolver-bearing entry points, exactly mirroring the existing binding/command resolver pattern. `lpanel` supplies `LPanel_ResolveViewType`. Viewport and scale attributes stay as documentation in the markup and are supplied by the application, as before.

The dangling-root bug is fixed structurally rather than patched. `InitLPanel` no longer calls `ViewHostSystem_CreateStandard` (which built a selector on an about-to-be-discarded root) and never assigns `lpanel->root = root`. Instead it builds the real `UI_ELEMENT_ROOT` with a valid coordinate space via `ViewHostSystem_InitRoot`, then re-parents the adopted XML `<ViewHost>` container under it with `AddElementToTree`. There is exactly one root and one selector.

Behaviour preservation holds: `LPanel_AttachToggleSources` still runs on the adopted tree so STATE toggle ON/OFF labels compose, DRAW's resolved type propagates to the consumers that gate on `active_view == LPANEL_DRAW_VIEW`, and the `size-mode="content_fill"` Sections are untouched by the loader changes.

Two adjacent refactors ride along in the same files: the toggle-source wiring now resolves overlay ids through `CommandSystem_ResolveToggleOverlay` instead of enum-order arithmetic, and `LPanel_QueryDebugEnabled` returns a `BindingValue` struct. Both are needed to keep the STATE toggles building and working after the imperative builders were removed.

<details>
<summary>Issues (2)</summary>

1. **Pre-existing staged rename** — `panel_system.{h,c}` -> `view_host_system.{h,c}` are staged (R100, pure renames) in the index from the earlier terminology-rename work, not this task. All four task files are unstaged. Non-blocking, but the human should know the index is non-empty before committing. (confirmed)
2. **Bundled adjacent changes** — the toggle-overlay resolution refactor (`CommandSystem_ResolveToggleOverlay`) and the `BindingValue`-returning query shim live in `lpanel_system.c` alongside the view-host work. Correct and necessary for the build, but worth noting as scope beyond the strict view-host wiring. (confirmed)

</details>

<details>
<summary>Details</summary>

## Two-pass selector build via the per-load side table

`<Option>`s appear before `<View>`s in `lpanel.xml`, and the host's `views` array is only populated after `UILoader_ExtractViews` walks the finished tree, so the selector cannot be wired during parsing. The implementation records, during the parse, a fixed-capacity (`MAX_LOADED_VIEWS == 32`) loader-internal table: per `<View>` its container pointer, resolved `ViewType` and `id`; per `<Option>` its button pointer and target view `id`; plus the selector container and the `<ViewHost>` `initialView`. `UILoader_ResetMarkupTable` zeroes it at the top of all four load entry points, so no state leaks between loads.

The linchpin is pointer identity. `BuildView` stores `container` in the table and sets `container->type = UI_ELEMENT_VIEW`; `UILoader_CollectViewContainers` later sets `view->container = elem` for that same `elem` (no copy) and now reads the recorded `ViewType` back via `UILoader_LookupViewType` instead of the old hard-coded `LPANEL_STATE_VIEW`. `UILoader_ResolveViewIndexById` then matches `g_markup_table.views[t].container == view->container` and compares the recorded `id`, so option-id -> container -> index is a direct `UIElement*` comparison. The loop bounds (`int k < host->views.count`) are consistent with the `int count` field on `LArray`, matching the signed-int iteration used everywhere else in `view_host_system.c`.

`UILoader_BuildSelectorFromMarkup` allocates the `ViewSelector` and its `buttons[]`/`view_indices[]` arrays with `sizeof(UIElement *) * count` and `sizeof(int) * count` — the exact element sizes `DestroyPanelViewSelector` frees, so teardown stays correct. Each option resolves to its view index (unmatched -> index 0 with a warning), `view_indices[i]` holds the resolved index (not a trivial `0..count-1`), and each button is wired `on_click = HandleViewHostSelectorClick`, `user_data = &selector->view_indices[i]`, `data_bind = selector` — identical to `ViewHostSystem_CreateViewSelector`. Allocation and `LArray_Push` failures free what was allocated and return NULL. `active_index` is left at the `count` sentinel, overwritten by the single `ViewHostSystem_SelectView` call in `InitLPanel`.

## Dangling-root fix and InitLPanel rewiring

`InitLPanel` follows the design's Option B flow: load with resolvers, `LPanel_AttachToggleSources`, extract views, `ViewHostSystem_Create`, `ViewHostSystem_InitRoot` (builds the real `UI_ELEMENT_ROOT` + space + seed_box), then `AddElementToTree(root, lpanel->root)` to re-parent the XML container under the real root — never `lpanel->root = root`. Views are pushed, the array (not the Views) freed, the selector built from markup, `initialView` resolved to an index (fallback 0), and `ViewHostSystem_SelectView` called exactly once. A failed load logs an error and returns with `lpanel` NULL (`DrawLPanel` null-guards). A failed `ViewHostSystem_Create` disposes `root` and frees the view array before returning. If selector allocation fails, single-view visibility is established via the public `EnableElement`/`DisableElement` primitives rather than the static `SetPanelActiveView`, and `ViewHostSystem_FinaliseInit` is never called (it would force index 0 and fight `initialView`).

The dead imperative path is genuinely removed, not left misrepresenting behaviour: the commented-out fallback, `InitLPanelStateView`, `InitLPanelEditView`, `InitEntityCreateDefaults`, the `LPanelDebugToggle` tables, and the stale view-container pointers are all gone. `BuildPanel` is deleted and the `Panel` tag unregistered, with `ViewHost` registered in its place (net builder count unchanged). `DestroyLPanel`'s clears were pruned to match the removed statics.

## STATE behaviour preserved via the toggle-source rewiring

`LPanel_AttachToggleSources` still runs on the adopted XML tree, so STATE action buttons keep their command-sink bindings and ON/OFF query sources. The derivation of the `DebugOverlayId` from the sink's command code changed from enum-order arithmetic (`code - CMD_TOGGLE_DEBUG_DASHBOARD`) to the explicit `CommandSystem_ResolveToggleOverlay` mapping, and `LPanel_QueryDebugEnabled` now returns a canonical `BindingValue{BIND_INT, 0|1}` instead of a raw int. These are adjacent to the view-host task but are required for the XML-authored toggles to build and compose their labels after the imperative builders were removed. The `size-mode="content_fill"` Sections are untouched by the loader changes.

## Bonus loader hardening

`BuildContainer`'s `// TODO` stub was replaced with the same attribute extraction `BuildSection` uses (`UILoader_ExtractCommonAttrs` + `UILoader_ParseLayout`), defaulting to `ui_standard_container_size` so attribute-less `<Container>` markup is unaffected. `UILoader_ParseLayout` gained a `"stacked"` -> stack alias; this is a global change affecting every element that reads `layout=`, but since `stacked` previously fell through to the stack default it changes nothing observable.

## Git state

`git status` shows two staged renames (`include/system/panel_system.h` -> `include/system/view_host_system.h`, `src/engine/system/ui/panel_system.c` -> `src/engine/system/ui/view_host_system.c`), both pure R100 renames with their content edits sitting unstaged on top. These belong to the earlier Panel->ViewHost terminology rename, not this XML task. This task's four files (`ui_loader.h`, `ui_loader.c`, `lpanel_system.c`, `lpanel.xml`) are all unstaged and uncommitted, and the verification doc attests no git mutation by this task. No commit, branch, worktree, or reset occurred. The acceptance criterion that this task leaves git unmutated is met; the staged renames are a pre-existing index state the human should be aware of before committing, not a mutation introduced here.

## Build evidence

Per `xml-viewhost-verification.md` the canonical UCRT64 + Ninja build configured and built to `EXIT=0` (`[110/111] Linking ... raylib-game.exe`), including the three changed translation units. One intermediate failure (implicit declaration of `UILoader_LogWarning` because the side-table helpers using `LOADER_WARNING` were defined above the logging definitions) was fixed by forward-declaring the logging helpers; this forward declaration is present in the diff. The `ninja: warning: premature end of file` prefix is environmental, not a code error.

</details>

<details>
<summary>File map</summary>

- `include/system/ui/ui_loader.h` — `UIViewTypeResolver` typedef + context field; `resolve_view_type` param on both resolver entry points; three new post-process declarations (`UILoader_BuildSelectorFromMarkup`, `UILoader_ResolveViewIndexById`, `UILoader_GetInitialViewId`); include of `view_host_system.h`.
- `src/engine/system/ui/ui_loader.c` — per-load side table + reset; `BuildViewHost` (replaces deleted `BuildPanel`); real `BuildViewSelector`/`BuildOption`; `BuildView` type resolution; `BuildContainer` attribute extraction; `stacked` layout alias; `CollectViewContainers` type lookup; three post-process functions; forward-declared logging helpers.
- `src/engine/system/ui/lpanel_system.c` — `LPanel_ResolveViewType`; rewired `InitLPanel` (Option B flow); removed imperative builders/stubs/tables; toggle-source rewiring via `CommandSystem_ResolveToggleOverlay`; `BindingValue` query shim; `PanelSystem`->`ViewHostSystem` symbol renames.
- `src/engine/ui/components/lpanel.xml` — `<ViewHost>` root uncommented, `<ViewSelector>`/`<Option>` live, `draw_view` uncommented with correct `type=`.

Full diff: `git diff HEAD -- include/system/ui/ui_loader.h src/engine/system/ui/ui_loader.c src/engine/system/ui/lpanel_system.c src/engine/ui/components/lpanel.xml`

</details>
