# Implementation Plan: XML-authoritative ViewHost + View selection for the left panel

Source design (approved): `.agents/tasks/xml-viewhost-design.md`. This plan sequences that
design; it does not re-decide the architecture. Follow items in order — each one leaves the
tree in a buildable state, because the header, loader implementation, and caller rewiring are
grouped so no intermediate step dangles an undefined symbol.

UK English in all new identifiers and comments (`colour`, `Initialise`, `Unrecognised`).
Every new function gets a description comment; comment the logic inside, per `AGENTS.md`.

## Key facts grounding this plan (verified in source)

- `ViewType` enum lives in `include/system/ui/ui_state.h`; `LPANEL_STATE_VIEW == 0`,
  `LPANEL_DRAW_VIEW == 1`.
- `ViewHostSystem` / `ViewSelector` structs are public in `include/system/view_host_system.h`.
  `ViewSelector` fields: `panel`, `UIElement **buttons`, `int *view_indices`, `size_t count`,
  `size_t active_index`, `ViewSelectionCallback on_view_selected`.
- `HandleViewHostSelectorClick`, `ViewHostSystem_SelectView`, `ViewHostSystem_InitRoot`,
  `ViewHostSystem_Create`, `ViewHostSystem_InitViews`, `ViewHostSystem_HandleViewSelected`
  are public. `AllocatePanelViewSelector`, `SetPanelActiveView`,
  `UpdatePanelViewSelectorButtons`, `DestroyPanelViewSelector` are `static` in
  `src/engine/system/ui/view_host_system.c` and MUST stay static (design §5 / file-change
  summary) — do NOT expose new public helpers in that file.
- `DestroyPanelViewSelector` frees `buttons` as `sizeof(UIElement *) * count` and
  `view_indices` as `sizeof(int) * count`; the markup selector MUST allocate with the exact
  same element sizes so teardown matches.
- `HandleViewHostSelectorClick` reads selector from `button->data.button.data_bind` and the
  index from `*(int *)button->data.button.user_data`.
- `AddElementToTree(UIElement *element, UIElement *parent)`, `EnableElement`,
  `DisableElement` are public in `include/ui/ui.h`.
- `UILoader_CollectViewContainers` currently hard-codes `view->type = LPANEL_STATE_VIEW`
  (in `src/engine/system/ui/ui_loader.c`); this is the line to replace.
- `lpanel.xml` root is already `<ViewHost ...>` with `<ViewSelector>` + two `<Option>`s and
  two live `<View>`s (`state_view` type `LPANEL_STATE_VIEW`, `draw_view` type
  `LPANEL_DRAW_VIEW`). The "commented-out draw_view" in the task brief is stale; draw_view is
  already uncommented. Item 8 is therefore a defensive confirm, not an edit.
- No unit-test framework exists in the repo (only mxml's own `testmxml.c` under `build/`).
  `src/CMakeLists.txt` uses `file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)` and the
  single `add_executable` target is the whole program. The verification signal for every
  item is therefore the CMake build (compile + link exit 0), plus code inspection for the
  behavioural guarantees. There is no `ctest`/unit target to run; do not invent one.

## GIT CONSTRAINT (applies to EVERY item)

NO git operations occur at any step: no `commit`, `stage`/`add`, `rebase`, `branch`, `merge`,
`reset`, `stash`, worktree creation, or any other git command. All edits stay uncommitted on
the `ui-overhaul` branch. The user commits manually. This constraint is absolute for every
item below and for any review/fix follow-up.

---

# Implementation Plan

- [ ] 1. Extend the loader header: add the view-type resolver typedef + context field and the
      three new post-process declarations.
      In `include/system/ui/ui_loader.h`: (a) add `typedef ViewType (*UIViewTypeResolver)(const char *type_string, bool *resolved, UILoaderContext *ctx);`
      with a doc comment (place near the `UICommandResolver` typedef); (b) add field
      `UIViewTypeResolver resolve_view_type;` to `struct UILoaderContext` (next to
      `resolve_command`); (c) add the trailing `UIViewTypeResolver resolve_view_type` parameter
      to BOTH `UILoader_LoadFromFileWithResolvers` and `UILoader_LoadFromStringWithResolvers`
      declarations (before `void *user_data`), updating their doc comments; (d) forward-declare
      the three new post-process functions with doc comments:
      `ViewSelector *UILoader_BuildSelectorFromMarkup(ViewHostSystem *host, ViewSelectionCallback callback);`,
      `int UILoader_ResolveViewIndexById(ViewHostSystem *host, const char *id_string);` (returns
      -1 when unresolved), and `const char *UILoader_GetInitialViewId(void);`. `ViewType` is
      already visible via the `#include "system/ui_system.h"` chain (it pulls `ui_state.h`);
      add `#include "system/view_host_system.h"` for `ViewHostSystem`/`ViewSelector`/
      `ViewSelectionCallback` if not already transitively available — verify the include graph
      does not create a cycle (view_host_system.h includes ui.h, not ui_loader.h, so it is safe).
      Files: `include/system/ui/ui_loader.h`
      Verify: this header-only change does not build alone cleanly until item 2 updates the
      definitions; defer the build to item 2 (they are one coherent compile unit — the brief
      permits grouping). Confirm the signatures compile as part of item 2's build.

- [ ] 2. Update the two resolver entry-point DEFINITIONS to the new signatures and thread the
      resolver into the stack-local context, and pass `NULL` from the no-resolver wrappers.
      In `src/engine/system/ui/ui_loader.c`: add the matching
      `UIViewTypeResolver resolve_view_type` parameter to the definitions of
      `UILoader_LoadFromFileWithResolvers` and `UILoader_LoadFromStringWithResolvers`; in each,
      add `.resolve_view_type = resolve_view_type` to the `UILoaderContext ctx = { ... }`
      initialiser alongside the existing `.resolve_binding`/`.resolve_command`. In the
      no-resolver wrappers `UILoader_LoadFromFile` and `UILoader_LoadFromString`, the inline
      `ctx` initialisers leave `resolve_view_type` unset (NULL) — leave them as-is (they build
      their own `ctx` with `.resolve_binding = NULL` etc.; `.resolve_view_type` defaults to
      NULL via designated-initialiser zeroing, which `BuildView` treats as "no resolver").
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: this item alone will not link because `InitLPanel` (item 7) still calls the old
      4-resolver-arg signature; it is corrected in item 7. Build after item 7. (Items 1-7 form
      one coherent signature change; the first green build is at item 7.)

- [ ] 3. Add the per-load side table and its clear-on-load, plus the `stacked` layout alias.
      In `src/engine/system/ui/ui_loader.c`: (a) add `#define MAX_LOADED_VIEWS 32`; (b) add a
      file-static side-table struct holding, per captured element: for views —
      `UIElement *container; ViewType view_type; char id[MAX_UI_ELEMENT_ID];`; for options —
      `UIElement *button; char view_id[MAX_UI_ELEMENT_ID];`; for the selector container —
      `UIElement *selector_cont;`; and a single `char initial_view_id[MAX_UI_ELEMENT_ID];` plus
      counts. Use fixed-capacity arrays (cap `MAX_LOADED_VIEWS`) with overflow handled by a
      `LOADER_WARNING` and ignore (design Error-handling). (c) Add a static
      `UILoader_ResetMarkupTable(void)` that zeroes the table, and call it at the start of each
      of the four load entry points (`UILoader_LoadFromFile[WithResolvers]`,
      `UILoader_LoadFromString[WithResolvers]`) BEFORE parsing — so no cross-load state leaks.
      (d) In the shared `UILoader_ParseLayout`, add `else if (!strcmp(layout_str, "stacked")) return &ui_standard_stack_spacing;`
      (global alias; harmless — previously defaulted). Comment that this applies to every
      element reading `layout=`.
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: part of the item-7 build. Independently confirm by inspection that the reset runs
      before `UILoader_ParseElementNode` in all four entry points.

- [ ] 4. Add `BuildViewHost` and register it; remove `BuildPanel` and unregister `Panel`.
      In `src/engine/system/ui/ui_loader.c`: implement `static UIElement *BuildViewHost(...)`
      per design §2 — extract `id` and `layout` (via `UILoader_ExtractCommonAttrs` +
      `UILoader_ParseLayout`), create a fill-size container (`ui_fill_container_size`, parsed
      layout spacing) via `CreateUIContainer(parent, ...)` (parent is NULL at root); it MUST be
      a plain `UI_ELEMENT_CONTAINER`, NOT a `UI_ELEMENT_ROOT` (design §7 invariant). Record the
      node's `initialView` attribute into the side table's `initial_view_id`. Do NOT resolve
      `viewport`/`scale` (lpanel-specific; `InitLPanel` supplies them). In
      `UILoader_RegisterDefaultBuilders`: add `UILoader_RegisterBuilder("ViewHost", BuildViewHost);`
      and REMOVE `UILoader_RegisterBuilder("Panel", BuildPanel);`. Delete the `BuildPanel`
      function (it is now unregistered and unused). Net builder count unchanged (one added, one
      removed) so `MAX_REGISTERED_BUILDERS` (64) is not a concern.
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: part of the item-7 build. Inspection: grep `**/*.xml` for `<Panel` returns no
      matches (confirmed during exploration), so removing the `Panel` tag regresses no asset.

- [ ] 5. Rewrite `BuildView`, `BuildViewSelector`, `BuildOption`; tidy `BuildContainer`;
      replace the hard-coded `view->type` in `UILoader_CollectViewContainers`.
      In `src/engine/system/ui/ui_loader.c`:
      (a) `BuildView` (design §3): keep extracting `view_id` and `type=`; resolve `type=` via
      `ctx->resolve_view_type` when non-NULL (pass a `bool resolved`), on success stash the
      resolved `ViewType`, on failure/absent default to the first `ViewType` (0) and
      `LOADER_WARNING(ctx, "Unrecognised view type")`; append `(container, view_type, view_id)`
      to the side table; still set `container->type = UI_ELEMENT_VIEW`.
      (b) `BuildViewSelector` (design §4): build the toggle-bar CONTAINER mirroring
      `ViewHostSystem_CreateStandardViewSelector`'s styling (transparent surface,
      `colour_border = palette->container_border`, zero inline spacing,
      `ui_standard_selector_container_size`); record the container pointer in the side table;
      remove the TODO. Do NOT allocate a `ViewSelector` struct here.
      (c) `BuildOption` (design §4): create a `UI_ELEMENT_BUTTON_ENUMERATE` button with `text=`
      and `ui_standard_selector_button_size` (handlers NULL at build time); read `view=` and
      record `(button, view_id)` in the side table; if `view=` missing,
      `LOADER_WARNING(ctx, "Option missing view target")` (still create the button); remove the
      TODO.
      (d) `BuildContainer` (design §8, Finding 11): replace the `// TODO` with the exact
      extraction `BuildSection` uses — `UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, NULL)`
      + `UILoader_ParseLayout` for spacing, defaulting to `ui_standard_container_size` when
      absent. No behavioural change when attributes absent.
      (e) In `UILoader_CollectViewContainers`, replace `view->type = LPANEL_STATE_VIEW;` with a
      side-table lookup keyed on the `elem` container pointer, assigning the recorded
      `ViewType`; if (defensively) not found, default to the first `ViewType` with a
      `LOADER_WARNING`.
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: part of the item-7 build. Inspection: the container pointer stored in the side
      table (step a) is the SAME `UIElement *` that `UILoader_CollectViewContainers` sets as
      `view->container` (no copy), so the lookup in (e) and the id->index resolution in item 6
      are direct pointer comparisons.

- [ ] 6. Implement the three post-process functions.
      In `src/engine/system/ui/ui_loader.c` (declarations added in item 1):
      (a) `UILoader_ResolveViewIndexById(host, id_string)`: for each `host->views[k]`, find the
      side-table view row whose `container == host->views[k]->container` and compare its
      recorded `id` to `id_string` (`strcmp`); return the first matching `k`, else -1. An
      id-less View (empty recorded id) never matches.
      (b) `UILoader_GetInitialViewId()`: return the side table's `initial_view_id` (or NULL if
      empty).
      (c) `UILoader_BuildSelectorFromMarkup(host, callback)` (design §5): allocate a
      `ViewSelector` with `count = number of recorded Options`; allocate
      `selector->view_indices = AllocateBytes(sizeof(int) * count)` and
      `selector->buttons = AllocateBytes(sizeof(UIElement *) * count)` (exact element sizes so
      `DestroyPanelViewSelector` frees match). For each Option `i`: resolve its recorded
      `view_id` to index `k` via the same container-pointer mechanism as (a) (direct lookup
      against `host->views`), default `k = 0` + `LOADER_WARNING` on no match; set
      `buttons[i] = recorded Option button`, `view_indices[i] = k` (the RESOLVED index, not
      `0..count-1`); wire `buttons[i]->data.button.on_click = HandleViewHostSelectorClick`,
      `buttons[i]->data.button.user_data = &selector->view_indices[i]`,
      `buttons[i]->data.button.data_bind = selector`. Set `selector->panel = host`,
      `selector->count = count`, `selector->active_index = count` (unselected sentinel),
      `selector->on_view_selected = callback`. `LArray_Push(&host->selectors, &selector)`. Do
      NOT style buttons here and do NOT call `ViewHostSystem_SelectView` here (that is
      `InitLPanel`'s single call, item 7). On allocation failure free what was allocated
      (mirror `AllocatePanelViewSelector`'s cleanup) and return NULL. Match `button->data.button`
      field names to the actual `UIElement` struct — confirm `on_click`/`user_data`/`data_bind`
      member names by reading the button data struct in `include/ui/ui.h` before writing.
      Files: `src/engine/system/ui/ui_loader.c`
      Verify: part of the item-7 build.

- [ ] 7. Rewire `InitLPanel` and add `LPanel_ResolveViewType`; remove the dead fallback and
      the now-unused hard-coded view builders.
      In `src/engine/system/ui/lpanel_system.c`:
      (a) Add `static ViewType LPanel_ResolveViewType(const char *type_string, bool *resolved, UILoaderContext *ctx)`
      that maps `"LPANEL_STATE_VIEW" -> LPANEL_STATE_VIEW` and `"LPANEL_DRAW_VIEW" -> LPANEL_DRAW_VIEW`
      (set `*resolved = true`), else `*resolved = false` and return `LPANEL_STATE_VIEW`.
      (b) Rewrite `InitLPanel` to the design §7 numbered flow (chosen Option B):
      1. `root = UILoader_LoadFromFileWithResolvers(lpanel.xml path, &ui_default_palette, LPanel_ResolveBinding, LPanel_ResolveCommand, LPanel_ResolveViewType, NULL);`
         (new 6th arg). If `!root`, log an error and return (leave `lpanel` NULL; `DrawLPanel`
         already null-guards). Keep the existing absolute XML path string.
      2. `LPanel_AttachToggleSources(root);` (unchanged — wires STATE toggle sources).
      3. `view_count = 0; xml_views = UILoader_ExtractViews(root, &view_count);`
      4. `lpanel = ViewHostSystem_Create(&lpanel_viewport, 1.0f, (Vector2d){0.1f, 0.1f}, &ui_default_palette, ui_standard_stack_spacing);`
      5. `ViewHostSystem_InitRoot(lpanel);` (builds the real `UI_ELEMENT_ROOT` + space + seed_box).
      6. `AddElementToTree(root, lpanel->root);` — re-parent the XML `<ViewHost>` container
         UNDER the real root. NEVER `lpanel->root = root` (that is the dangling-root bug).
      7. `ViewHostSystem_InitViews(lpanel, view_count);`
      8. `for (i < view_count) LArray_Push(&lpanel->views, &xml_views[i]);` then
         `Deallocate((void **)&xml_views, sizeof(View *) * view_count);` (free the array, not
         the Views).
      9. `lpanel_view_selector = UILoader_BuildSelectorFromMarkup(lpanel, ViewHostSystem_HandleViewSelected);`
      10. If `lpanel_view_selector`: `int initial = UILoader_ResolveViewIndexById(lpanel, UILoader_GetInitialViewId()); if (initial < 0) initial = 0; ViewHostSystem_SelectView(lpanel_view_selector, (size_t)initial);`
          Else (selector alloc failed): loop `lpanel->views`, `EnableElement(views[0]->container)`
          and `DisableElement(views[i]->container)` for `i > 0` (XML containers default
          enabled; establish single-view visibility via the public primitives — do NOT call the
          static `SetPanelActiveView`). NEVER call `ViewHostSystem_FinaliseInit` (it forces
          index 0 and fights `initialView`).
      11. `UpdateUISpace(lpanel->root, lpanel->seed_box);`
      (c) Remove the commented-out fallback block in `InitLPanel`, and delete the now-dead
      `InitLPanelStateView`, `InitLPanelEditView` (and their forward declarations), and the
      `static InitEntityCreateDefaults` — they are referenced ONLY from the removed fallback
      (grep-gated in design §8). Leave `DestroyLPanel` and the `G_UIState.edit_*_tbox`
      null-clears intact. Removing the edit-view builder leaves `edit_*_tbox` NULL, which is
      safe (design Out-of-Scope: `RefreshTextboxFields` skips NULL textbox rows). If removing
      `InitLPanelStateView`/`InitLPanelEditView` leaves file-static toggle tables or helpers
      unreferenced (e.g. `lpanel_state_view_cont`, `lpanel_edit_view_cont`,
      `lpanel_debug_*_toggles`, `HandleLPanelDebugToggleClickInternal`,
      `create_entity_section_size`, `debug_section_size`), remove only those that become
      genuinely unused — verify each with a grep before deleting; keep any still referenced
      elsewhere (e.g. `DestroyLPanel` clears `lpanel_state_view_cont`/`lpanel_edit_view_cont`,
      so either keep those declarations or also clean the clears consistently). Prefer a
      clean, warning-free compile.
      Files: `src/engine/system/ui/lpanel_system.c`
      Verify (FIRST GREEN BUILD — this closes the item-1..7 signature change): run the
      canonical build (see VERIFICATION). Expected: configure succeeds and
      `cmake --build --preset debug` exits 0 with no errors. Fix any compile/link errors before
      proceeding.

- [ ] 8. Confirm `lpanel.xml` has `draw_view` live (defensive — requirement 5).
      Read `src/engine/ui/components/lpanel.xml` and confirm the `<View id="draw_view"
      type="LPANEL_DRAW_VIEW">` block is present and uncommented (it already is per
      exploration). If — and only if — it is found commented out, uncomment it. Make no other
      structural change; the Sections keep `size-mode="content_fill"` with no `size` attr.
      Files: `src/engine/ui/components/lpanel.xml` (edit only if draw_view is commented)
      Verify: if edited, re-run the build (CONFIGURE_DEPENDS is irrelevant for an XML-only edit,
      but a rebuild confirms nothing broke). If unchanged, no build needed — note "already
      live, no change".

- [ ] 9. Confirm the preserved interactions by inspection (no code change expected).
      Confirm in source that: (a) `src/engine/editor/geometry_editor.c` and
      `src/engine/system/ui_system.c` still compare `G_UIState.active_view == LPANEL_DRAW_VIEW`
      and now receive the correct value because `draw_view`'s resolved `type` propagates through
      `BuildView` -> `UILoader_CollectViewContainers` -> `view->type`, and selecting DRAW fires
      `ViewHostSystem_HandleViewSelected` (set as the callback in item 7 step 9), which sets
      `G_UIState.active_view = view->type`; (b) `LPanel_AttachToggleSources` still runs on the
      adopted XML tree (item 7 step 2) so STATE toggle ON/OFF labels compose; (c) the
      `size-mode="content_fill"` Sections are untouched by the loader changes. These are
      guarantees to re-read and confirm, not edits.
      Files: (read-only) `src/engine/editor/geometry_editor.c`,
      `src/engine/system/ui_system.c`, `src/engine/system/ui/lpanel_system.c`
      Verify: no build needed beyond item 7's; record the confirmation in the item notes.

- [ ] 10. Final whole-build verification.
      Run the canonical build end to end (see VERIFICATION). Expected: `cmake --preset debug`
      reconfigures (required because item 2-7 do not add/rename `.c` files, but `BuildPanel`
      removal and new static functions are within the existing `ui_loader.c`/`lpanel_system.c`
      so no new TU is created — a reconfigure is still cheap and safe), then
      `cmake --build --preset debug` exits 0. If the only failure is the EXE link lock, apply
      the EXE LOCK procedure in VERIFICATION. Clean up any temporary files created during
      verification.
      Files: none (verification only)
      Verify: `cmake --build --preset debug; "EXIT=$LASTEXITCODE"` prints `EXIT=0`.

---

## OUT OF SCOPE (explicit boundary — do NOT implement this pass)

- DRAW view field data-wiring. The `<TextField>` controls in `draw_view` RENDER and the view
  is SELECTABLE, but their `G_UIState.edit_*_tbox` pointers are NOT populated and the fields
  are NOT bound to `entity_create_params`. `BuildTextField` is unchanged (still resolves
  `binding=` for the data address via `LPanel_ResolveBinding`), but the named `G_UIState`
  textbox pointers stay NULL. This is NULL-safe: `RefreshEntityEditorFields` ->
  `RefreshTextboxFields` begins each row with `if (!field->textbox) continue;`, so DRAW
  selection does not crash and the per-frame refresh is a safe no-op.
- Building any `id -> G_UIState` registration system. Explicitly deferred.
- Hover-style selectors from XML (`ViewSelector type="hover"` is reserved, only `enumerate`
  wired).
- rpanel / state-manager / utility-panel migration to XML.

---

## VERIFICATION (Windows PowerShell, canonical toolchain only)

Toolchain: MSYS2 UCRT64 (gcc) + Ninja. NEVER use npm / yarn / make / MSVC, and never invent
build commands. Use ONLY the CMake presets.

### Configure (required when `.c` files are added/renamed; harmless otherwise)

`src/CMakeLists.txt` uses `file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS *.c)`, so a new or
renamed `.c` file needs a reconfigure to enter the build. This plan edits existing `.c` files
only (no new TU), but reconfiguring is cheap and safe:

```powershell
cmake --preset debug
```

### Build and capture the authoritative exit code

```powershell
cmake --build --preset debug; "EXIT=$LASTEXITCODE"
```

- Success is `EXIT=0`. Do NOT judge success through a `Select-String` pipe — piping corrupts
  `$LASTEXITCODE`. Read the printed `EXIT=` line.
- The `ninja: warning: premature end of file; recovering` prefix is NOT an error; ignore it.
- Build output executable: `build/Debug/raylib-game/raylib-game.exe`.

### EXE LINK LOCK caveat (environment, not code)

If EVERY `.c` compiled and ONLY the final link failed with
`ld.exe: cannot open output file raylib-game\raylib-game.exe: Permission denied`, that is an
ENVIRONMENT LOCK (the game is running), not a code error.

```powershell
Get-Process -Name raylib-game
```

- If the process is present, the user must close the game. Do NOT kill it; do NOT run the exe.
- Definitive link check despite the lock (from `build/Debug`): gather the object files and link
  to a TEMP output with the full lib list, then delete the temp exe:

```powershell
# from build/Debug
Get-ChildItem -Recurse -Filter *.obj CMakeFiles\raylib-game.dir
# link all .obj to a temp name with cc.exe (full lib list), then:
Remove-Item <temp-exe>
```

- A STALE orphan `.obj` from a renamed/removed source (none expected this pass, but watch for a
  leftover such as `panel_system.c.obj`) can cause bogus multiple-definition errors; delete the
  stale orphan `.obj` and relink. Prefer the normal `cmake --build` as the primary signal.

### What verification can and cannot prove here

- The build (compile + link exit 0) is the automated pass/fail signal: it proves the new
  signatures, the three post-process functions, the rewired `InitLPanel`, and the stub cleanup
  all type-check and link against the real tree.
- There is NO unit/integration test harness in this repo, so the design's behavioural
  assertions (two views extracted with correct `type`, selector buttons wired to indices
  {0,1}, DRAW selection sets `active_view`) are confirmed by CODE INSPECTION (item 9), not by a
  test runner. Do NOT fabricate a test target.
- Running the game is MANUAL and only on explicit user request (per `AGENTS.md`); do NOT run
  `raylib-game.exe` as part of verification.

---

## GIT (restated): no git operations at any step. All edits remain uncommitted on `ui-overhaul`.
```
