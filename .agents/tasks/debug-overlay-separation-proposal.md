# Debug Overlay System — Separation / Ownership Proposal

Read-only investigation. No code was changed; no build or git action was taken. Every claim below cites `file:line` from the `ui-overhaul` branch.

---

## 1. Summary answer

The system named "debug overlay" (`include/system/debug_overlay_system.h`, `src/engine/system/debug_overlay_system.c`) conflates three unrelated concerns behind one `DebugOverlayId` enum and one `ToggleDebug`/`IsDebugEnabled` facade:

1. **Developer diagnostics** (genuinely debug) — the dashboard and the basis editor.
2. **Object inspection gizmos** — axes / hull / AABB drawn onto entities.
3. **View / display settings** — grids, grid labels, UI borders (presentation preferences, one of them defaulting ON).

The clean seam already exists: a toggle is modelled as a *(query source, command sink)* pair, and the UI binding layer never learns which bucket a toggle belongs to (`src/engine/system/ui/lpanel_system.c:246-278`). The command layer resolves action strings to codes with no domain knowledge beyond a string table (`src/engine/system/command_system.c:84-116`). The one place that *does* carry hidden coupling is `ExecuteCommand`, which converts a `CMD_TOGGLE_*` code to a `DebugOverlayId` by subtracting a base constant (`src/engine/system/command_system.c:235-242`) — this is the single fragile point that breaks if the enum is split.

**Recommendation:** keep the uniform *(query fn, command code)* surface, replace the enum-offset arithmetic with an explicit table so the backing enums can split safely, then re-home each bucket's *state and facade* into its owning subsystem. Diagnostics stay as a true (compile-outable) debug system; gizmos move next to world/object rendering; view settings become a display-settings concept. Stage the work; the arithmetic de-coupling (item in §3) is the mandatory first step and is independently valuable even if the rest is deferred.

---

## 2. Evidence — what each piece actually is and where its state lives

### 2.1 The enum and facade

`DebugOverlayId` (`include/system/debug_overlay_system.h:13-25`):
```
DEBUG_DASHBOARD = 0, DEBUG_VIEWPORT_GRID, DEBUG_WORLD_GRID, DEBUG_WORLD_GRID_LABELS,
DEBUG_UNIVERSE_GRID_LABELS, DEBUG_UI_BORDERS, DEBUG_OBJECT_AXES, DEBUG_OBJECT_HULL,
DEBUG_OBJECT_AABB, DEBUG_COUNT
```
API: `ToggleDebug(DebugOverlayId)` / `IsDebugEnabled(DebugOverlayId)` (`debug_overlay_system.h:28-29`); plus `UpdateDebugOverlayHotkeys(...)`, `DrawUniverseDebugOverlays(void)`, `DrawGlobalDebugOverlays(void)`, and `extern bool ui_borders_enabled` (`debug_overlay_system.h:26,30-37`).

### 2.2 State-ownership map (verified)

`ToggleDebug` / `IsDebugEnabled` are a `switch` over the enum (`debug_overlay_system.c:519-575`). Some cases own a *local* static bool; others *delegate* to the owning subsystem. This split is the key fact the proposal is built on:

| Enum id | Backing state | Owner today | Bucket |
|---|---|---|---|
| `DEBUG_DASHBOARD` | `dashboard_overlay_enabled` | **local** `debug_overlay_system.c:169` | 1 Diagnostics |
| (basis editor, F5) | `basis_editor_enabled`, `basis_editor_target`, `basis_editor_editing_u` | **local** `debug_overlay_system.c:175-177` | 1 Diagnostics |
| `DEBUG_VIEWPORT_GRID` | `debug_grid_enabled` via `ToggleViewportDebugGrid()` / `IsViewportDebugGridEnabled()` | **delegated → viewport** `viewport_system.c:20,248-255` | 3 View |
| `DEBUG_WORLD_GRID` | `world_grid_overlay_enabled` (**default `true`**) | **local** `debug_overlay_system.c:171` | 3 View |
| `DEBUG_WORLD_GRID_LABELS` | `world_grid_debug_labels_enabled` | **delegated → world** `world_system.c:31`, decl `include/world/world.h:161` | 3 View |
| `DEBUG_UNIVERSE_GRID_LABELS` | `grid_labels_overlay_enabled` | **local** `debug_overlay_system.c:169` | 3 View |
| `DEBUG_UI_BORDERS` | `ui_borders_enabled` | **delegated → UI** `ui_system.c:37` | 3 View |
| `DEBUG_OBJECT_AXES` | `object_axes_overlay_enabled` | **local** `debug_overlay_system.c:170` | 2 Gizmo |
| `DEBUG_OBJECT_HULL` | `object_hull_overlay_enabled` | **local** `debug_overlay_system.c:171` | 2 Gizmo |
| `DEBUG_OBJECT_AABB` | `object_aabb_overlay_enabled` | **local** `debug_overlay_system.c:172` | 2 Gizmo |

So the facade is **already a partial facade** over state owned elsewhere for viewport grid, world grid labels and UI borders. The proposal moves the *remaining* local state into the owning subsystem rather than duplicating it.

Note the correction to the brief: `DEBUG_UNIVERSE_GRID_LABELS` is **not** delegated — its state (`grid_labels_overlay_enabled`) is local to `debug_overlay_system.c:169`. Only viewport-grid, world-grid-labels and UI-borders currently delegate.

### 2.3 Bucket 1 — Developer diagnostics (the only honest "debug")

- Dashboard: `DrawDebugDashboard()` and its section helpers draw cursor coords, universe coords, selected/hovered world, camera focus/zoom/rotation, viewport pixel/local bounds, px/unit, and the `UniverseDebugSnapshot` (`debug_overlay_system.c:` dashboard section funcs ~`DrawDashboardControlsSection`/`DrawDashboardUniverseSection`/`DrawDashboardViewportBasisSection`, and `DrawDebugDashboard`). Snapshot populated in `DrawUniverseDebugOverlays()` (end of file).
- Basis editor (F5): `DebugBasisTargetOps` table over 6 viewport/space targets (`debug_overlay_system.c:21-166`), driven by `HandleBasisEditorHotkeys()` (TAB target, U/V vector, I/J/K/L nudge, O/P scale, BACKSPACE reset) and `HandleBasisVectorMutation()`.
- These are the only candidates for release compile-out.

### 2.4 Bucket 2 — Object inspection gizmos

Gated in `src/engine/world/world_renderer.c`:
- `DrawNewtonoidHull(...)` early-returns unless `IsDebugEnabled(DEBUG_OBJECT_HULL)` (`world_renderer.c:148`).
- `DrawNewtonoidAABB(...)` early-returns unless `IsDebugEnabled(DEBUG_OBJECT_AABB)` (`world_renderer.c:171`).
- `DrawNewtonoidAxes(...)` early-returns unless `IsDebugEnabled(DEBUG_OBJECT_AXES)` (`world_renderer.c:369`).

These annotate entities (collision hull, bounding box, orientation vectors) — editor-grade inspection, not dev crutches.

### 2.5 Bucket 3 — View / display settings

- World grid: `world_renderer.c:429` early-returns unless `IsDebugEnabled(DEBUG_WORLD_GRID)`; **default TRUE** (`debug_overlay_system.c:171`).
- World grid labels: `world_renderer.c:462` reads `world_grid_debug_labels_enabled` **directly** (bypasses the facade).
- Universe grid labels: `universe_renderer.c:85` gates on `IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS)`.
- Viewport grid: `DrawViewportDebugGrid()` self-gates on `debug_grid_enabled` (`viewport_system.c:231-250`); always *called* from `DrawGlobalDebugOverlays()`.
- UI borders: `ui_renderer.c:102` and `ui_renderer.c:134` gate border alpha on `IsDebugEnabled(DEBUG_UI_BORDERS)` (they do **not** read `ui_borders_enabled` directly — they go through the facade). `DEBUG_WORLD_GRID` defaulting TRUE is the tell that these are normal presentation, not debugging.

### 2.6 The uniform toggle surface (the seam to preserve)

- String → code: `CommandSystem_ResolveString` maps `"toggle-object-aabb"` etc. to `CMD_TOGGLE_*` (`command_system.c:84-116`). Pure string table, no domain knowledge.
- `CMD_TOGGLE_*` codes (`command_system.h:21-29`), base `CMD_TOGGLE_DEBUG_DASHBOARD = 100`, declared in the **same order** as `DebugOverlayId`.
- UI binding wiring: lpanel attaches a *query source* to any button whose *command sink* code is in the `CMD_TOGGLE_*` range (`lpanel_system.c:252-278`), deriving the `DebugOverlayId` via the same offset `ExecuteCommand` uses: `query_key = code - CMD_TOGGLE_DEBUG_DASHBOARD` (`lpanel_system.c:273`). The query shim is `LPanel_QueryDebugEnabled(int)` → `IsDebugEnabled((DebugOverlayId)key)` (`lpanel_system.c:246-249`). The binding core only knows "call this fn with this int key" — it is already bucket-agnostic.
- Per-frame label refresh composes `"<text>: ON/OFF"` from the query automatically (`PanelSystem_RefreshBindings`, described `lpanel_system.c:252-258`).

### 2.7 The fragile coupling

`ExecuteCommand` (`command_system.c:235-242`):
```c
if (type >= CMD_TOGGLE_DEBUG_DASHBOARD && type <= CMD_TOGGLE_OBJECT_AABB) {
    DebugOverlayId overlay_id = (DebugOverlayId)(type - CMD_TOGGLE_DEBUG_DASHBOARD);
    ToggleDebug(overlay_id);
    return;
}
```
This is correct **only** while `CMD_TOGGLE_*` ordinals and `DebugOverlayId` ordinals stay in lock-step. The identical arithmetic is duplicated in `lpanel_system.c:273`. Splitting `DebugOverlayId` into three enums breaks both unless the arithmetic is removed first.

---

## 3. Answers to the required questions

### Q1 — Target structure (which system each bucket becomes, where its state lives)

- **Bucket 1 — Diagnostics → stays as a true debug system.** Keep `debug_overlay_system.{h,c}` as the home for the dashboard + basis editor only. Its local state (`dashboard_overlay_enabled`, `basis_editor_*`) already lives here and stays here. This is the only module wrapped in a release compile-out guard (§Q4).
- **Bucket 2 — Gizmos → near world/object rendering.** New small module owned by the world layer, e.g. `object_gizmos` beside `world_renderer.c` (`include/world/object_gizmos.h` / `src/engine/world/object_gizmos.c`), holding `object_axes/hull/aabb` state and a `GizmoId { GIZMO_AXES, GIZMO_HULL, GIZMO_AABB }` enum with `ToggleGizmo`/`IsGizmoEnabled`. `world_renderer.c:148/171/369` call the new owner directly. Rationale: these are drawn by the world renderer and are entity annotations — they belong to the world/object subsystem, matching AGENTS.md "domain functionality lives in the owning subsystem".
- **Bucket 3 — View / display settings.** The three *already-delegated* flags stay where they live (viewport grid in `viewport_system.c`, world grid labels in `world_system.c`, UI borders in `ui_system.c`); the facade simply stops being the toggling entry point for them. The two *locally-held* view flags move to their natural owner: `DEBUG_WORLD_GRID` (default TRUE) → `world_system.c` next to `world_grid_debug_labels_enabled`; `DEBUG_UNIVERSE_GRID_LABELS` → universe/universe-renderer ownership (a `universe_display` flag). Treat the set as a "view/display settings" concept; it is defensible to make the grid/label flags per-viewport later, but that is a larger change — not required by this refactor. **Move the facade; do not duplicate state.**

### Q2 — Uniform toggle interface to preserve

Keep the external contract exactly: an action string resolves to a *(query fn, command code)* pair, and the UI binding + command layers stay bucket-agnostic. Concretely:
- Keep `CommandSystem_ResolveString` as the single string→code map (`command_system.c:84-116`) — unchanged in shape; it already has no domain knowledge.
- Keep the `CMD_TOGGLE_*` code range as the stable, UI-facing identifier. The UI binding keeps keying its query source off the command code (`lpanel_system.c:252-278`); it must not learn about three enums.
- Replace the single `IsDebugEnabled`/`ToggleDebug` with per-bucket query/toggle fns, but expose them to the command/UI layers through **one dispatch table keyed by `CMD_TOGGLE_*`** (see Q3). The query shim `LPanel_QueryDebugEnabled` becomes a shim that calls that table's query fn, so the binding core still only sees "call fn with int key".

### Q3 — Breaking the fragile offset coupling

Replace the arithmetic in `ExecuteCommand` (`command_system.c:238`) and in `lpanel_system.c:273` with an **explicit lookup table** indexed by `CMD_TOGGLE_*`. Each entry holds `{ void (*toggle)(void); int (*is_enabled)(void); }` (or a `{bucket, backing-id}` pair). Then:
- `ExecuteCommand` finds the entry for `type` and calls `entry->toggle()` — no enum subtraction, no assumption that two enums share ordering.
- The lpanel query source stores the `CMD_TOGGLE_*` code as its `query_key` and the query shim calls `entry->is_enabled()` — the offset derivation at `lpanel_system.c:273` is deleted.
This removes the only ordering dependency, so the three backing enums can be defined independently and reordered freely. Put the table wherever the command system can see all three subsystems (it already includes them transitively); a thin registration API per subsystem is the cleanest, but a single static table in one translation unit is the simplest correct option and matches AGENTS.md "simplest solution".

### Q4 — Release compile-out (bucket 1 only)

Only diagnostics (dashboard + basis editor) should be compile-out-able. Guard the *bodies* of `DrawDebugDashboard`, `DrawUniverseDebugOverlays`, the basis-editor hotkey/mutation handlers, and the dashboard state behind a build macro (e.g. `KIRO_DIAGNOSTICS`/`NDEBUG`-driven). Keep the public functions as no-op stubs when disabled so callers in `raylib_game.c` (`DrawGlobalDebugOverlays` at `raylib_game.c:394`, `UpdateDebugOverlayHotkeys` at `raylib_game.c:399`) need no `#ifdef`. Buckets 2 and 3 are **not** compiled out — gizmos are editor features and view settings (grid default TRUE) are normal presentation.

### Q5 — Complete dependents list and per-dependent change

Every site that calls `IsDebugEnabled`/`ToggleDebug`, names a `DEBUG_*` id, or uses a `CMD_TOGGLE_*` code:

| File:function (line) | What it does today | Under the split |
|---|---|---|
| `command_system.c` `CommandSystem_ResolveString` (84-116) | string → `CMD_TOGGLE_*` | unchanged |
| `command_system.c` `ExecuteCommand` (235-242) | offset `type - base` → `ToggleDebug` | replace with dispatch-table lookup (Q3) |
| `command_system.h` enum (21-29) | `CMD_TOGGLE_*` codes | unchanged (stable UI-facing ids) |
| `lpanel_system.c` toggle tables (47-67) | `LPanelDebugToggle{DebugOverlayId,label}` arrays for fallback builder | retype to per-bucket ids, or key off `CMD_TOGGLE_*`; note this is the (commented-out) hardcoded fallback path — XML path is live |
| `lpanel_system.c` `HandleLPanelDebugToggleClickInternal` (69-82) | `ToggleDebug(toggle->id)` + `IsDebugEnabled` for label | call per-bucket toggle/query via table |
| `lpanel_system.c` `LPanel_QueryDebugEnabled` (246-249) | `IsDebugEnabled((DebugOverlayId)key)` | query via dispatch table keyed by `CMD_TOGGLE_*` |
| `lpanel_system.c` `LPanel_AttachToggleSources` (252-278) | derives `query_key = code - base` | store `code` as key; delete offset arithmetic |
| `world_renderer.c` `DrawNewtonoidHull` (148) | `IsDebugEnabled(DEBUG_OBJECT_HULL)` | `IsGizmoEnabled(GIZMO_HULL)` |
| `world_renderer.c` `DrawNewtonoidAABB` (171) | `IsDebugEnabled(DEBUG_OBJECT_AABB)` | `IsGizmoEnabled(GIZMO_AABB)` |
| `world_renderer.c` `DrawNewtonoidAxes` (369) | `IsDebugEnabled(DEBUG_OBJECT_AXES)` | `IsGizmoEnabled(GIZMO_AXES)` |
| `world_renderer.c` world grid (429) | `IsDebugEnabled(DEBUG_WORLD_GRID)` | view-settings query owned by world (`world_system.c`) |
| `world_renderer.c` grid labels (462) | reads `world_grid_debug_labels_enabled` directly | unchanged (already owned by world) |
| `universe_renderer.c` (85) | `IsDebugEnabled(DEBUG_UNIVERSE_GRID_LABELS)` | universe view-settings query |
| `ui_renderer.c` (102, 134) | `IsDebugEnabled(DEBUG_UI_BORDERS)` | UI view-settings query owned by `ui_system.c` |
| `viewport_system.c` (231-255) | owns `debug_grid_enabled`, `DrawViewportDebugGrid` | unchanged (already owner); view-settings label |
| `world_system.c` (31) + `world.h` (161) | owns `world_grid_debug_labels_enabled` | unchanged; also receives moved `DEBUG_WORLD_GRID` state |
| `ui_system.c` (37) | owns `ui_borders_enabled` | unchanged (already owner) |
| `debug_overlay_system.c` `ToggleDebug`/`IsDebugEnabled` switches (519-575) | one switch over all 9 ids | split: diagnostics keep local cases; delete gizmo/view cases (moved to owners) |
| `debug_overlay_system.c` dashboard rows (`DrawDashboardControlsSection`) | reads `IsDebugEnabled(DEBUG_VIEWPORT_GRID/…)` to print ON/OFF | read via the new per-owner query fns (dashboard is a *consumer* of all three buckets — acceptable, it is diagnostics) |
| `debug_overlay_system.c` `HandleDebugToggleHotkeys` (F1-F12) | toggles across all buckets | route each hotkey to the owning subsystem's toggle (or via the dispatch table) |
| `raylib_game.c` (394) | `DrawGlobalDebugOverlays()` | unchanged signature (diagnostics stub when compiled out) |
| `raylib_game.c` (399) | `UpdateDebugOverlayHotkeys(...)` | unchanged signature; internally routes to owners |
| `lpanel.xml` buttons | `action="toggle-*"` strings | unchanged (action strings are the stable contract) |
| `command_system.c`/`universe_system.c`/`ui_system.c`/`world_system.c` `#include "system/debug_overlay_system.h"` | includes for the facade | gizmo/view includes repoint to new owners; diagnostics include stays only where diagnostics are used |

Dead/commented code noted, not targeted: the hardcoded `InitLPanel` fallback and `InitLPanelStateView` builder (`lpanel_system.c` fallback block is commented out; the live path is XML). The `LPanelDebugToggle` tables feed only `InitLPanelStateView`, which the live XML path does not call — flag but do not over-invest.

### Q6 — Migration ordering (behaviour-preserving) and risk

Order (each step leaves the build green and behaviour identical):
1. **De-couple the arithmetic (Q3).** Introduce the `CMD_TOGGLE_*`→`{toggle,is_enabled}` dispatch table; point `ExecuteCommand` and `LPanel_AttachToggleSources` at it. Backing enum unchanged. Pure refactor, zero behaviour change. *This is independently valuable and the prerequisite for everything else.*
2. **Extract gizmos (bucket 2).** Create `object_gizmos`, move the three local bools and the three `world_renderer.c` call sites, update the dispatch table entries. World renderer behaviour identical.
3. **Re-home view settings (bucket 3).** Move `DEBUG_WORLD_GRID` (preserve default **TRUE**) and `DEBUG_UNIVERSE_GRID_LABELS` state to world/universe owners; repoint `ui_renderer.c`, `universe_renderer.c`, dashboard rows and the dispatch table. The three already-delegated flags need only the facade removed.
4. **Shrink diagnostics + add compile-out (Q4).** `debug_overlay_system` now owns only dashboard + basis editor; wrap bodies in the build guard with no-op stubs.

Risk/effort: **taxonomy + ownership refactor — LOW behavioural risk, BROAD surface** (touches the command system, hotkeys, 3 renderers, UI bindings, 4 subsystems). Things that could *silently* change behaviour, to guard explicitly:
- `DEBUG_WORLD_GRID` default **TRUE** (`debug_overlay_system.c:171`) must remain the default in its new home or the grid silently vanishes at startup.
- The initial values of every moved bool must be preserved (dashboard/gizmos default false; world grid true).
- The `CMD_TOGGLE_*` code values and the `"toggle-*"` action strings are the UI contract — must stay byte-identical so `lpanel.xml` and the binding wiring keep resolving.
- `HandleDebugToggleHotkeys` key mappings (F1 axes, F2 world-grid-labels, F3 UI-borders, F4 universe-grid-labels, F5 basis editor, F6 viewport-grid, F11 dashboard, F12 hull / Shift+F12 AABB, Ctrl+F1 ResetUI) must map to the same toggles after routing changes.
- `DrawViewportDebugGrid()` is always *called* and self-gates — keep that call in whatever replaces `DrawGlobalDebugOverlays`, or the viewport grid stops drawing.

### Q7 — Scope / sequencing recommendation

**Stage it; do not split all three at once.** Do step 1 (de-couple arithmetic) on its own and merge — it is low-risk, removes the only fragile coupling, and unblocks the rest. Then do buckets 2 and 3 as a second change; they are independent of each other (gizmos touch only `world_renderer.c`; view settings touch the three renderers + owners) so either can go first. Do the diagnostics compile-out (step 4) last, once the module is already slimmed.

**Leave genuinely alone:** the three *already-delegated* flags (viewport grid, world grid labels, UI borders) need no new home — only the facade indirection removed, and even that is optional cleanup. The basis editor and dashboard are already cohesive and correctly placed; they only need the compile-out guard, not restructuring. The commented-out lpanel hardcoded builder should not be migrated — it is dead relative to the live XML path.

---

## 4. Conclusions

- The architecture the user wants is *mostly already present*: the UI/command layers are bucket-agnostic through a *(query, command)* pairing, and three of the nine toggles already delegate state to their owning subsystem.
- The single real defect is the enum-ordinal arithmetic in `command_system.c:238` (duplicated in `lpanel_system.c:273`). Fixing it with an explicit `CMD_TOGGLE_*` dispatch table is a self-contained, zero-behaviour-change change and the correct first step.
- After that, re-homing is mechanical: gizmos to the world layer, the two locally-held view flags to world/universe, diagnostics kept and compile-out-guarded. The uniform surface (`CMD_TOGGLE_*` codes + `"toggle-*"` strings) stays fixed throughout, so UI XML and bindings never change.
- The only silent-behaviour trap is `DEBUG_WORLD_GRID`'s default TRUE; preserve it on move.
