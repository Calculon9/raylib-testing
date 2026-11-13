# Design: unify UI colours on the universal palette

Worktree `.worktrees/palette-unify`, branch `feature/palette-unify`, base `43e7568`. Audit: `.agents/tasks/ui-style-coverage-findings.md`. Paths are relative to the worktree root. Stack is unchanged: C11, CMake, raylib, the project's own bitmap font. No new files other than this document.

## Overview

Almost everything is a palette-value change or a small edit in the shared layer. Three structural changes carry the rest.

1. **Toolbar icon toggles join the shared toggle path.** The ten toolbar buttons all get a `binding=`, so the existing per-frame `UIElement_RefreshBinding` swaps their fill and ink. The glyphs stop choosing their own ink and just use the button's current font colour.
2. **Palette threading.** `UIElement_RefreshBinding` and `PanelShared_Header*` receive the owning host's palette. Every `ViewHostSystem_Create` and loader call takes `UIPalette_GetActive()`. No file outside `ui_system.c` reads `ui_default_palette` directly.
3. **`UIPalette_SetActive` / `UIPalette_GetActive`.** `ui_default_palette` stays as storage. A switch copies into it and then calls `ResetUI()`.

## Confirmation of audit items against the current code

I read every cited location in the worktree. All line numbers match (same base commit). Findings:

| Item | Status |
|---|---|
| A1, A2 | Confirmed. `InitUIPalettes` is `ui_system.c:140`. Values at lines 162/165 (classic), 189/192 (earth), 216/220 (harbour), 244/247 (meadow). `InitUIPalettes` is guarded by `ui_palettes_initialized`, so `ResetUI` does not reset the palette (needed for SetActive). |
| A3 | Confirmed with a correction. The four tool buttons (`tool_nudge`, `tool_cell_paint`, `tool_spawn`, `toggle_geometry_edit`) have **no `binding=`**, so the refresh never touches them and their ink comes from `UIState_IsToolActive` / `UIState_IsGeometryEditActive` inside the glyph. They need bindings (below) to take the tile fill. After A2 the ON ink ratio is 8.01 (harbour), not the 7.31 quoted in the audit. |
| A4 | Confirmed (`view_host_system.c:57-58`, `100-101`). |
| A5 | Confirmed (`diagnostics.c:462-469`). Also `DrawDashboardLine:215` uses `FONT_BASIC` (see Extra 2). |
| B6 | Confirmed. `StyleViewContainer` (`view_host_system.c:933`) writes colours directly and sets no `border_weight`. The utility panel is its **only** caller (grep), so changing it affects nothing else. |
| B7 | Confirmed (`popup.xml:59, 63, 75, 90`). |
| B9 | Confirmed, plus extra sites the audit did not list: `state_manager_system.c:1216` (`StateManager_SetActionState`), the `ui_loader.c:745,1824,1873,1928,1980` and `ui_constructors.c` NULL fallbacks, `view_host_system.c:57-58,100-101,135,291`, and the creation sites in `*_system.c` that pass `&ui_default_palette`. |
| B10 | Confirmed. `CreateViewSection*` have no callers. `lpanel_system.c` **cannot be removed**: `diagnostics.c:17,46-97` uses `GetLPanelSpaceFrame`, `SetLPanelSpaceBasis`, `ResetLPanelSpaceBasis` for the debug-basis target, and `systems.h` still declares `InitLPanel`/`DrawLPanel`. Leave `lpanel_system.*` and `lpanel.xml` alone. |
| Extra 2 | Only one real use of the plain font macros: `diagnostics.c:215`. `FONT_DEFAULT` (`screen.h:17`, `ui_renderer.h:17`) and `FONT_BASIC_S` (`cfont.h:19`) are defined but unused (`FONT_DEFAULT` appears only in a comment at `ui_renderer.c:406`). `COLOUR_UI_INK_RGBA` is defined twice in `common.h` (lines 104 and 168); only `cfont.h` reads it. |
| Extra 3 | Confirmed. The detail strip fallback only feeds the `RGB r,g,b` hint text, never a drawn colour. |
| Extra 4 | Confirmed. Element fills, borders and fonts are copied **by value** at construction (`CreateUIContainer`, `CreateUIButton`, `ui_loader`), so they do not follow the palette. |

## Palette changes (`src/engine/system/ui_system.c`, `include/system/ui_system.h`)

Contrast uses the formula in the brief. I calculated all numbers for this document. Thresholds: 4.5 for text, 3 for the scrollbar thumb.

### A1 `toggle_off_fill`, A2 `text`, earth `button_danger_fill`

| Role | classic | earth | harbour | meadow |
|---|---|---|---|---|
| `toggle_off_fill` | (122,100,55) | (112,74,41) unchanged | (124,87,33) | (122,100,55) |
| `text` | (10,16,12) | (0,0,0) | (10,20,24) | (10,16,12) |
| `button_danger_fill` | unchanged | (214,96,52) | unchanged | unchanged |

Ratios after the change (`ond` is `text_on_dark`):

| Pair | classic | earth | harbour | meadow |
|---|---|---|---|---|
| inactive tab/chip/OFF/MIXED: `ond` on `toggle_off_fill` | 5.02 | 5.44 | 4.84 | 5.02 |
| `text` on `container_fill` (notes, empty states, popup captions) | 5.05 | 4.54 | 4.79 | 5.05 |
| `text` on `button_fill` = `toggle_on_fill` (button captions, ON tile) | 11.30 | 4.87 | 8.01 | 11.30 |
| `text` on `input_fill` (field values, OFF icon ink) | 17.03 | 14.69 | 13.95 | 17.03 |
| `text` on `button_danger_fill` | 4.98 | 5.58 | 5.94 | 4.55 |

`text` is used as: input ink (`input_fill`), OFF icon ink (`input_fill`), button captions (`button_fill`/`danger`), default labels and notes (`container_fill`), hover items. Every pair above is at or over 4.5, and every one is better than before. Residual caveat: `text` on `subsection_fill` is 2.9 to 3.0, but nothing uses it there by design (COMP cards use `ink="light"` = `text_on_dark`, 4.84 to 5.67). Earth `text` changes from teal to black, so earth input text becomes black (visible side effect).

Earth `button_danger_fill` equals earth `container_border` (191,78,36) today. Only the danger fill changes. `container_border` stays.

### New roles (set in all four palettes)

Add two fields at the end of `UIPalette` (after `warning`) and set them in `InitUIPalettes` for classic, earth, harbour and meadow. `ui_default_palette = ui_harbor_palette` then copies them.

| Role | Value (all four) | Used by |
|---|---|---|
| `overlay_fill` | (8,12,18,215) | dashboard backing |
| `debug_outline` | (255,255,255,80) | UI-borders debug outline |

Dashboard ink over the worst-case light background (white composited under overlay alpha 215 gives bg (47,50,55); parchment gives a darker bg):

| Ink | classic | earth | harbour | meadow |
|---|---|---|---|---|
| title/body `text_on_dark` over white | 11.40 | 9.00 | 9.62 | 11.40 |
| title/body `text_on_dark` over parchment (232,224,198) | 12.26 | 9.68 | 10.35 | 12.26 |
| headings `warning` (255,255,176) over white | 12.36 | 12.36 | 12.36 | 12.36 |

The audit's numbers are confirmed (all at least 4.5 over white, so the overlay passes everywhere). A dark panel under the overlay is darker still, so contrast only rises there.

### A4 scrollbars

Track = `subsection_fill`, thumb = `text_on_dark`. Thumb-on-track is 5.67 (classic), 5.08 (earth), 4.84 (harbour), 5.67 (meadow), so the 3:1 requirement is met with margin and meadow's thumb is no longer invisible.

## Code changes by item

### A3 toolbar icon ON state: filled tile

Goal: ON = `toggle_on_fill` tile with `palette->text` glyph ink (`UIPalette_GetToggleColours(active=true)`). OFF = the authored flat `input_fill` tile with `text` ink.

- **`toolbar.xml` / `toolbar_system.c`**: add `binding=` to the four buttons that lack one: `tool_nudge` -> `tool.nudge` (the resolver already supports it), `tool_cell_paint` -> `tool.cell-paint`, `tool_spawn` -> `tool.spawn`, `toggle_geometry_edit` -> `tool.geometry-edit`. In `Toolbar_ResolveBinding`, replace the lone `tool.nudge` branch with a small table of `{name, ActiveTool}` that returns the existing `Toolbar_QueryToolActive` with `query_key = tool`. Add `tool.geometry-edit` as a second small query, `Toolbar_QueryGeometryEdit`, returning `UIState_IsGeometryEditActive()` as 0/1 BIND_INT. This is safe: a bound button with empty `text=` just copies the empty authored text to `display_text` (`ui.c:1559-1560`), so nothing draws. `action=` is unaffected, because for buttons the sink comes from `action=` (`UILoader_AssembleBindingSource(..., allow_write_sink=false)`). Update the XML comments that say these buttons have no binding.
- **`ui.c` `UIElement_RefreshBinding(UIElement *e, const UIPalette *palette)`**: remove the `on_draw == NULL` exception in the ON path. The rule becomes one expression:
  - text toggle (no `on_draw`): `UIPalette_GetToggleColours(palette, on, ...)` as before.
  - icon toggle (`on_draw` set): ON takes `UIPalette_GetToggleColours(palette, true, ...)`. OFF is restored every frame to `UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_INPUT, NULL, &fill)` with ink `palette->text`. The restore is necessary because the fill is written on the live element each frame, so an ON-to-OFF change would otherwise keep the amber.
  - The MIXED branch drops its `on_draw == NULL` guard. Icon toggles never report MIXED, so behaviour is unchanged.
  - Rewrite the stale header comment (lines ~1535-1540) and the two inline comments.
- **`ui_icons.c`**: delete `UIIcon_StateInk` and its `Binding_ReadSource` logic. Replace every ink with one tiny static helper `UIIcon_Ink(const UIElement *e)` returning `ToRaylibColor(e->data.button.font.colour)`. That ink is, by construction, `text` on both fills (OFF `input_fill`: 13.95 harbour; ON `toggle_on_fill`: 8.01 harbour). The sites are: the nine toggle glyphs (`UIIcon_DrawDashboard` ... `UIIcon_DrawCollisions`), and the four inline `active ? button_fill : text` expressions in Nudge (~140), CellPaint (~203), Spawn (~251), GeometryEdit (~285). Those inline expressions also remove the `UIState_Is*Active` reads from the glyph code, because state now arrives through the binding. Update the file header and the per-function comments that describe the amber ink.
- **Cell-paint swatch**: keep `TilePalette_ColourForType(UIState_GetActiveTileType(), fallback)` as the tile colour (fill = tile colour, unchanged). The fallback `ui_default_palette.text` becomes the button's own ink (`e->data.button.font.colour`).
- **Glyphs affected (all 13 draw paths)**: Nudge, CellPaint, Spawn, GeometryEdit, Dashboard, ViewportGrid, WorldGrid, WorldGridLabels, UniverseGridLabels, UIBorders, ObjectAxes, ObjectHull, ObjectAABB, Collisions. Every one is monochrome except the CellPaint swatch, which is a tile colour and keeps its own colour on both tiles. Check on screen that the CellPaint swatch (arbitrary tile colour, e.g. amber-ish) is still distinguishable from the amber tile when ON. The frame and dividers are `text`-dark and separate it.
- Check during implementation that button hover handling in `ui_input.c` does not overwrite `colour_fill` in a way that fights the per-frame refresh (the existing text toggles already live with this, so none is expected).

### A4 `view_host_system.c`

In `DrawContainerScrollbars` and `DrawViewScrollbar(s)` set `track_colour = subsection_fill`, `thumb_colour = text_on_dark`. Replace the `palette ? palette : &ui_default_palette` fallbacks with `palette ? palette : UIPalette_GetActive()` (in practice `palette` is always the host's).

### A5 dashboard (`diagnostics.c`)

`DrawDebugDashboard` fetches `const UIPalette *p = UIPalette_GetActive()` (the dashboard is global and draws outside any host). Then: `title_color = body_color = p->text_on_dark`, `accent_color = p->warning`, fill `p->overlay_fill`, border `p->text_on_dark`. The `DrawRectangle` calls take raylib `Color`, so convert with the existing `ToRaylibColor` (check its include in `diagnostics.c`).

### B6 utility panel

`ViewHostSystem_StyleViewContainer` becomes:
`UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_CONTAINER, &container->colour_border, &container->colour_fill); container->border_weight = UIPalette_GetSurfaceBorderWeight(palette, UI_PALETTE_SURFACE_CONTAINER);` plus the existing padding and `is_draggable`. In `InitUtilityStatsView`, call `CreateUILabelTitleDefault(stats_container, "Performance", ui_title_size, ui_standard_field_padding, utility_panel->palette)` before `InitUIFields` so the title is the first child. Fix the stale comment above `StyleViewContainer` ("lpanel, rpanel, utility"). Its only caller is the utility panel, so no other panel is affected. The utility viewport height must fit the extra 16 px title row plus gap; check `utility_panel_viewport` on screen (the `ui_utility_view_size` is percent-based, so the fields may be clipped if the strip is tight).

### B7 popup

`popup.xml`: add `style="title"` to the "Shapes", "Functional" and "Small" labels (`BuildLabel` routes to `CreateUILabelTitleDefault`). "No recent actions" is unchanged and is fixed by A2 (4.79 harbour). Check the popup label widths: titles use `ui_title_size` (hug) so no clipping.

### B9 palette threading

The host already owns the palette: `ViewHostSystem.palette` (set in `ViewHostSystem_Create`). The changes:

- `UIElement_RefreshBinding(UIElement *e, const UIPalette *palette)` (`ui.h`, `ui.c`). `ViewHostSystem_RefreshBindings(element)` becomes `(element, panel->palette)` and passes it through the recursion. This is the only caller. It replaces both `ui.c:1577,1589` reads and is how the toolbar's toggles get their palette.
- `PanelHeader` gets a `const UIPalette *palette` member set in `PanelShared_HeaderBind(header, root, palette, box_id, title_id, sub_id)`; `HeaderSetTint` reads `header->palette`. Callers pass `state_manager_panel->palette` (`state_manager_system.c:971`) and `s_wm_panel->palette` (`world_manager_system.c:672`). Replaces `panel_shared.c:304,319-327`.
- `StateManager_SetActionState` (`state_manager_system.c:1216`) uses `state_manager_panel->palette`. The chip update at `world_manager_system.c:792` uses `s_wm_panel->palette`.
- `ui_icons.c` needs no palette at all after A3 (ink from the element font).
- Creation sites (`toolbar`, `info_strip`, `detail_strip`, `popup_menu`, `state_manager`, `world_manager`, `utility_panel`, `lpanel`) pass `UIPalette_GetActive()` instead of `&ui_default_palette`. It is the same pointer, so behaviour is identical today.
- NULL-palette fallbacks in `ui_loader.c` and `ui_constructors.c` use `UIPalette_GetActive()`. Inside `ui_system.c` the fallbacks stay on the storage itself.
- `extern UIPalette ui_default_palette` stays in the header (storage must keep a stable address), but after this change nothing outside `ui_system.c` reads it. Verify with a final grep.

### B10

Delete `CreateViewSection` and `CreateViewSection_Stack` from `ui_constructors.c` (306-362) and their declarations at `ui_constructors.h:72-78`. Fix any comment that points at them (`ui_constructors.c` header comments at 331/352 go with the functions).

## Extra 2: font default ink

**Decision: remove the colourless macros.** Delete `FONT_BASIC`, `FONT_MEDIUM`, `FONT_LARGE` from `cfont.h`, keeping only the `_WITH_COLOUR` forms. Alternatives considered:

- Default to the active palette's `text`: needs a call to `UIPalette_GetActive()` inside a macro that expands in a header, which forces `cfont.h` to depend on `ui_system.h` and gives hidden palette lookups at every use. Rejected.
- Require the colour at every site: only one site exists, so the cost is one line, and the compiler then rejects any future colourless use.

Changes:
- `cfont.h`: remove the three macros, `FONT_BASIC_S` (unused alias), and update the comment.
- `screen.h:17`, `ui_renderer.h:17`: remove the unused `FONT_DEFAULT` defines. Fix the dead comments mentioning it (`ui_renderer.c:406`, `ui_input.h:17`, `cfont.c:15`).
- `diagnostics.c:DrawDashboardLine`: `Bitmap_Font font = FONT_BASIC_WITH_COLOUR(color); DrawTextCustom(text, origin, font.scale, font, color);`. Every other use of a font in the tree already uses `_WITH_COLOUR(palette->...)` (`ui_constructors.c`, `ui_loader.c:778`, `panel_shared.c`).
- `common.h`: delete both `COLOUR_UI_INK_RGBA` lines (104 and 168) and `COLOUR_UI_HARBOR_AMBER_RGBA` (117). Nothing else references them (grep of `src` and `include`).

## Extra 3: remaining literals

- `ui_renderer.c`: delete `UI_DEBUG_BORDER_COLOUR`; `ResolveDrawnBorderColour` uses `UIPalette_GetActive()->debug_outline`. The renderer has no host handle (it is a global tree draw), so the active palette is the correct source. Add `#include "system/ui_system.h"` if it is not already there.
- `detail_strip_system.c:164`: change the `(ColourRgba){0,0,0,255}` fallback to the existing `COLOURLESS_RGBA` (same `RGB 0,0,0` text, and it is not a drawn colour, so no palette role is justified). The implementer should add a one-line comment to say so.

## Extra 4: active palette API

In `ui_system.h` / `ui_system.c`:

```c
// Returns the palette every UI element is built from (stable pointer to the storage).
const UIPalette *UIPalette_GetActive(void);
// Switches the live UI to `src` (copied by value) and repaints. NULL is ignored.
void UIPalette_SetActive(const UIPalette *src);
```

- `GetActive` calls `InitUIPalettes()` (idempotent) and returns `&ui_default_palette`, so a call before `InitUI` still returns the harbour default.
- `SetActive`: ignore NULL and `src == &ui_default_palette`; `InitUIPalettes()`; `ui_default_palette = *src`; then, if the UI is live, `ResetUI()`. "Live" is `GetUIElementPool() != NULL` (the pool is created on first allocation and set to NULL at the end of `DestroyUI`), so calling before `InitUI` just stores the palette. Every pointer already handed out stays valid because the storage address never changes, including `ViewHostSystem.palette`.
- **Refresh mechanism: `ResetUI()`.** Colours are copied by value at construction into `colour_fill`, `colour_border`, each text element's `Bitmap_Font.colour` (button/label/textbox), `PanelHeader.default_fill`, and the popup/XML post-load patches. Only the pull paths are live (bindings refresh in `UIElement_RefreshBinding`, tab/chip state in `UpdatePanelViewSelectorButtons`, scrollbars, dashboard, renderer outline). A "re-apply" pass would have to know each element's surface role, which is not stored, so it would need a new per-element surface field and duplicate loader logic. `ResetUI` already does a full `DestroyUI` + `InitUI` (as `debug_overlay_system.c:58,73,82,197` use it) and `InitUIPalettes`' guard keeps the new palette. The cost is UI state that is rebuilt (selection panels, tab choices) resets, as for the existing callers.
- Contract documented in the header comment: do not call from inside a UI event handler or during the UI draw traversal (elements are freed); call from update/input code, as the debug overlay hotkeys do. No hotkey or menu is added.
- The `ui_classic_palette`, `ui_earth_palette`, `ui_harbor_palette` and `ui_meadow_palette` globals remain the selectable sources, e.g. `UIPalette_SetActive(&ui_earth_palette)`.

## Error handling and invariants

- `UIPalette_SetActive(NULL)`: no-op, silent. A palette is a plain struct with no resources, so no failure exists beyond that. The palette owns all colours (UI colours come only from `UIPalette`); the shared constructors and loader own that rule, and panels may not hard-code colours.
- New bindings: an unresolved name keeps the existing behaviour (loader warns, button stays unbound and flat). A missing `ViewHostSystem.palette` is impossible (`ViewHostSystem_Create` defaults it), but `RefreshBinding` with a NULL palette falls back to `UIPalette_GetActive()` for safety.
- Header with NULL palette: `PanelShared_HeaderBind` stores `palette ? palette : UIPalette_GetActive()`.
- `ResetUI()` memory rule is unchanged. This change adds no `LArray`. If an `LArray` value member were added anywhere, it would be released with `ClearLArray`, never `DisposeLArray`.

## Testing

There is no unit test target for the UI layer, so verification is a build plus a visual pass.

- Unit-testable without raylib: the palette contrast table can be asserted in a small test (compute ratios for the 4 palettes for the pairs above), and `UIPalette_SetActive` / `GetActive` (copy semantics, NULL ignored, same pointer) can be tested if a test harness exists. I found none, so this is optional.
- Build once at the end with `cmake --preset debug` then `cmake --build --preset debug` (the code changes are broad: signatures change in `ui.h`, `panel_shared.h`, `cfont.h`). A removed macro or a missed caller is a compile error, which is the intended safety net for Extra 2 and the signature changes.
- Visual (user-run): each of the four palettes via a temporary `UIPalette_SetActive` call; check toolbar ON/OFF tiles, tabs, chips, flag toggles, scrollbars, popup headings, utility panel box and "Performance" title, and dashboard over the world.

## Out of scope

World and gameplay colours, C11 to C14 opportunities, bulk value edits or renames, the lowercase "x" glyph, the raylib text calls in `world_renderer.c` and `universe_renderer.c`, any hotkey/menu for palette switching, and removing `lpanel_system.*` / `lpanel.xml` (needed by `diagnostics.c`).
