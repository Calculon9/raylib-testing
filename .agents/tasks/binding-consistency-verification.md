# Verification: Binding-consistency implementation (ui-overhaul)

First iteration (no `binding-consistency-impl-review.json` present). Implemented
from the plan `.agents/tasks/binding-consistency-plan.md`, grounded in the design
`.agents/tasks/binding-consistency-design.md`. UK English throughout. Git was NOT
touched (no stage/commit/branch/rebase/checkout). All changes left uncommitted.

## Acceptance-criterion amendment (second iteration — orchestrator decision)

A review (`binding-consistency-impl-review.json`, verdict CHANGES_REQUESTED)
raised ONE blocking finding — Finding 1, "binding.h changed (gate failure)" —
asserting the gate "the `Binding` struct in `include/ui/binding.h` must be
UNCHANGED" was violated. On escalation, the orchestrator ruled this gate INVALID
and AMENDED it. Rationale and the corrected criterion are recorded here so the
review re-gates against the corrected rule.

### Root cause (the original gate compared against a stale tree)

The brief/design falsely asserted the symmetric binding core already existed as a
COMMITTED prerequisite and listed `binding.h` / `binding.c` / `ui.c` as "do NOT
edit". That is incorrect. `HEAD` (`fd66fff`) is STALE: a large body of agreed work
lives UNCOMMITTED in the `ui-overhaul` working tree — the symmetric binding core,
the `PanelSystem`→`ViewHostSystem` rename, the textbox dynamic binding, the lpanel
DRAW-view XML migration, the view-selector machinery, etc. So "binding.h UNCHANGED
vs HEAD" never compared against a valid baseline; it compared against a tree that
predates all recent work. The symmetric core this refactor builds on is part of
that uncommitted body, not of `HEAD`.

### Amended criterion (replaces "binding.h UNCHANGED")

The additive binding-core widening THIS refactor requires is EXPLICITLY PERMITTED
AND EXPECTED — it was an agreed part of this design (resolvers must be able to
yield a query source and a callback sink). Specifically permitted:

- `BindingQueryFn` retyped `int` → `BindingValue` (query owns the returned type;
  the core passes it through unchanged).
- `BIND_SINK_CALLBACK` added to `BindingSinkKind`; `BindingSinkFn` typedef added.
- `BindingSink` grown with `write` / `write_key` members.
- The textbox refresh arm in `UIElement_RefreshBinding` (+ `TextBoxData.binding`
  and the `DisposeUIElement` free) that lets a migrated textbox pull via the
  generic walk.

The REAL (narrower) constraint that must hold — and does:

- The binding-core changes are ADDITIVE / non-regressive: existing address
  bindings, command sinks, and binding-less widgets keep working; the `Binding`
  struct's existing fields are NOT removed or repurposed (growing `BindingSink`
  with the callback-sink fields and retyping the query return are additive
  widenings, confirmed below). The legacy `data_bind` / `binder` textbox fields
  are retained alongside the new `binding` carrier.
- Everything else in the acceptance list stands unchanged.

Verified additive/non-regressive against the uncommitted tree:
- `binding.h`: diff vs HEAD only RETYPES `BindingQueryFn`'s return and APPENDS
  `BIND_SINK_CALLBACK` / `BindingSinkFn` / `BindingSink.write` / `.write_key`. No
  existing enumerator, field, or struct member is removed or re-ordered; the
  `Binding { source; sink; precision; }` shape is unchanged.
- `binding.c`: adds a `BIND_SINK_CALLBACK` dispatch arm and makes the
  `BIND_SRC_QUERY` arm pass the query's returned `BindingValue` through unchanged.
  The address/command arms are untouched → address bindings and command sinks
  unregressed.
- `ui.c` / `ui.h`: add a textbox binding carrier + its refresh/free; textboxes
  WITHOUT a binding stay on the legacy `RefreshTextboxFields` path (early-return
  on `BIND_SRC_NONE`), and the button `"<text>: ON/OFF"` display arm is reused
  unchanged (no new button display path).

### Finding 2 (unrelated work bundled in the tree) — NOT a defect, NOT in scope

Per the orchestrator: the view-selector machinery, lpanel DRAW-view XML
migration, textbox dynamic binding, and the `PanelSystem`→`ViewHostSystem` rename
are the accumulated UNCOMMITTED work from prior tasks this session — they are
SUPPOSED to be in the tree. They are not separated, reverted, or committed here.
Commit separation is the user's job; this task performs NO git operations.

### Git

NO git mutation of any kind was performed in either iteration. `HEAD` remains
`fd66fff` (unchanged); all edits (feature + core widening) remain uncommitted in
the `ui-overhaul` working tree. The user commits manually.

## Environment

- OS: Windows, PowerShell
- Toolchain: MSYS2 UCRT64 gcc + Ninja (per `.kiro/steering/build.md`)
- Build: CMake presets only (`debug`)

## Build commands run

No `CMakeLists.txt` was changed and no new translation unit was added, so a
reconfigure was not required. Built directly:

```powershell
cmake --build --preset debug; "EXIT=$LASTEXITCODE"
```

## Build evidence

### Attempt 1 (FAILED - one compile error, since fixed)

`ui_loader.c` referenced a `STRING` DataType enumerator that does not exist. The
project's `DataType` (in `include/system/systems.h`) uses `STRING64` / `STRING128`
/ `STRING256`, mirroring `integration_system.c`'s `ResolveBindingType`:

```
C:/Projects/raylib-testing/src/engine/system/ui/ui_loader.c:298:14:
  error: 'STRING' undeclared (first use in this function); did you mean 'STRING64'?
```

Fix: `UILoader_MapDataType` now maps the three STRING variants
(`STRING64`/`STRING128`/`STRING256`) to `BIND_STRING`, matching `ResolveBindingType`.

### Attempt 2 (PASSED)

- `ninja: warning: premature end of file; recovering` prefixed the build (noise,
  not an error, as documented).
- All 111 build objects compiled, including the five touched translation units:
  `ui_loader.c`, `lpanel_system.c`, `state_manager_system.c` (plus `integration_system.c`
  and `ui.c` recompiled from the header change in `ui_loader.h`).
- Final link succeeded:
  `[109/111] Linking C executable raylib-game\raylib-game.exe` ->
  `[110/111] Linking C executable raylib-game\raylib-game.exe`
- `EXIT=0`.

The output exe was NOT locked (the link completed normally); the temp-link
(`linkcheck.exe`) fallback was therefore not needed. The produced exe was NOT run
(per the brief; the user runs/verifies the game).

## What was implemented (summary)

1. **Resolver widening (`include/system/ui/ui_loader.h`, additive/append-only):**
   `UIBinding` grown by appending `kind` / `query` / `query_key` / `value_type`
   after the unchanged `address` (slot 0) / `data_type` (slot 1) so
   `LPanel_ResolveBinding`'s positional `{NULL, FLOAT}` init still compiles. Added
   `UIBindingSourceKind`, `UIBindingSinkKind`, the `UIAction` sink descriptor, the
   `UIActionResolver` typedef, and a new opt-in `resolve_action` field on
   `UILoaderContext` placed next to the RETAINED `resolve_command`.

2. **`BuildButton` (`src/engine/system/ui/ui_loader.c`):** assembles ONE Binding
   from BOTH `binding=` (source, via `resolve_binding` with the legacy
   non-NULL-address shim) and `action=` (sink, via `resolve_action` -> else legacy
   `resolve_command` -> else integer-parse). Added file-static `UILoader_MapDataType`
   (DataType -> BindingValueType, the single intentional duplication of
   `ResolveBindingType`). Click handler attached only when `have_sink`; a Binding is
   attached when either half resolved. Action-only buttons unchanged.

3. **lpanel debug toggles:**
   - `lpanel.xml`: `binding="debug.*"` added to each of the nine toggle `<Button>`s
     (dashboard, viewport-grid, world-grid, world-grid-labels, universe-grid-labels,
     ui-borders, object-axes, object-hull, object-aabb).
   - `lpanel_system.c`: `LPanel_ResolveBinding` gained a `debug.*` branch (placed
     BEFORE the physics dot-parse, guarded only on `binding_string`), returning a
     query source via the new `LPanel_ResolveDebugOverlayName` mapper + the retained
     `LPanel_QueryDebugEnabled`. `LPanel_AttachToggleSources` and its call in
     `InitLPanel` were DELETED outright. `CommandSystem_ResolveToggleOverlay` left
     untouched (still used by `ExecuteCommand`).

4. **state-manager flag buttons (`state_manager_system.c`, in-C, no XML migration):**
   added the five entity-family is-set queries (guard `o != NULL` ONLY, no id term),
   the world/cell queries (plain non-NULL gates), and
   `StateManager_QueryComponentAttached` (stricter `o && o->id != INVALID_ENTITY_ID`
   + bounds guard, reading `EntityRegistry_Describe(o).components[t] != NULL`).
   `CreateFlagButtons` gained a `query` param and attaches a query-source Binding;
   the component loop attaches its own query-source Binding keyed on type. The seven
   `UpdateFlagButtons` call sites were removed; `RefreshComponentsSection`'s loop was
   edited in place to keep only `is_enabled = is_valid` (label line + `attached`
   local removed); `desc` stays declared. Click `UIEventHandler`s preserved verbatim;
   sink stays `BIND_SINK_NONE`.

5. **LEGACY block (`state_manager_system.c` only):** appended a fully-commented,
   inert block under the exact header
   `// === LEGACY (pre-binding-consistency) — retained for easy revert ===`
   containing (verbatim, `//`-wrapped): the retired `UpdateFlagButtons` definition,
   the old `CreateFlagButtons` signature/body, the seven removed call-site lines with
   context, and the removed component `UpdateString64` label line + its `attached`
   local. The header notes a revert is ALL-OR-NOTHING.

6. rpanel BIND_STRING pattern: documentation only (design §5); rpanel NOT migrated.

## Git status

No git operation of any kind was performed. All edits remain uncommitted in the
working tree on the `ui-overhaul` branch.
