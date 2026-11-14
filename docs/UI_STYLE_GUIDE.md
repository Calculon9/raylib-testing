# UI Style Guide

The single reference for how every UI region looks and how to build it. Every panel, strip,
toolbar and popup follows it. No region chooses its own colours or sizes.

Companions: [`UI_MARKUP_SYNTAX.md`](UI_MARKUP_SYNTAX.md) (tags and attributes),
[`REPOSITORY_GUIDE.md`](REPOSITORY_GUIDE.md) (sizing model, subsystems). Reference look:
`.agents/tasks/ui-mock/final-mock-ROLES.png` (PHYS / FLAGS / COMP).

Every region (toolbar, info and detail strips, popup, State Manager, World Manager, Performance panel) follows it.

---

## 1. Principles

1. **Roles, not colours.** Fill and text always come as a pair from a colour role. Markup and C name the
   role (or a surface that maps to one). They never name a colour.
2. **One dark colour.** The panel background (`panel_background`, Harbor #1C3A43) is the only dark
   decorative colour. Do not add dark strips, bands or boxes as decoration.
3. **One continuous sheet, cards for fields.** Content sits on one sage sheet with no bordered boxes. A group
   of fields is a teal CARD on the sheet (section 5). Everything else (chip grids, buttons) stays on the sheet
   under a title. Blocks are separated by 6 px of space.
4. **Shared constants only.** No per-region pixel sizes or colours. If a size is needed twice, it is a
   constant.
5. **Same control, same look, everywhere.** A read-only field looks the same in every panel.

## 2. Colour roles (Harbor values)

`UIColourRole` in `include/system/ui_system.h`. Resolve with `UIPalette_GetRoleColours(palette, role)`,
which returns `{ fill, text, border }`.

| Role | Fill | Text | Border | Use | Markup |
| --- | --- | --- | --- | --- | --- |
| `UI_ROLE_SHEET` | sage #78AA9E | ink #0A1418 | none | Content area of every panel and popup | `surface="container"` with `border="none"` |
| `UI_ROLE_HEADER` | light sage #9CC4B9 | black | 2 px coral #DA7560 rule below (amber in multi-selection) | Entity / World panel header | `PanelShared_HeaderBind` |
| `UI_ROLE_INPUT` | cream #EFDDB8 | ink | #1C3A43 | Editable text fields | `<TextField io="safe">`, `<Textbox>` |
| `UI_ROLE_READONLY_INPUT` | dimmed cream #CFC5A8 | ink | #6E8F86 | Every read-only field, everywhere | `io="readonly"` (automatic) |
| `UI_ROLE_INACTIVE` | dark teal #3A665E | cream | #1C3A43 | Inactive tab, OFF toggle or chip, scrollbar track. Inside a card it resolves to the sheet fill with ink text | `surface="inactive"` |
| `UI_ROLE_CARD` | dark teal #3A665E (`card_fill`) | cream #EFDDB8 (`text_on_dark`) | #1C3A43 | A group of fields on the sheet | `surface="card"` (`subsection` is a deprecated alias) |
| `UI_ROLE_ACTIVE` | amber #DC9E46 | ink | none | Active tab, ON toggle or chip, primary action | `surface="button"` |
| `UI_ROLE_DANGER` | coral #DA7560 | ink | none | Delete / remove | `surface="danger"` |

Panel background: `panel_background` #1C3A43, behind `ViewHost` and `View` (`surface="panel"`).
`muted_text` (#7C5721) is not a role. Use it only for the x/y prefixes and `unit=` suffixes drawn on cream.

Two-state controls (tabs, chips, toggles, tool buttons) take ACTIVE when on and INACTIVE when off, via
`UIElement_ApplyRoleColours(element, palette, on ? UI_ROLE_ACTIVE : UI_ROLE_INACTIVE)`. Never pair a role's fill with a different role's text.

## 3. Typography

| Element | Font | Colour |
| --- | --- | --- |
| Section title (`style="title"`) | FONT_LARGE, one 16 px row (`ui_title_size`) | `title_text` ink, starts 10 px from the bar's left edge |
| Everything else | FONT_MEDIUM | paired role text |
| Field label | FONT_MEDIUM | `label_text` (ink), or card text inside a card |

The title's accent bar is a 4x16 px amber bar at the left (`UI_TITLE_BAR_WIDTH_PX`, `UI_TITLE_BAR_HEIGHT_PX`,
`UI_TITLE_TEXT_INDENT_PX` in `include/ui/ui.h`).
Section titles are UPPER CASE. The folding happens once, in `CreateUILabelTitleDefault`, so write titles in any case in markup (`<Label text="Physics" style="title" />`) and every title, including C-built ones, renders upper case.

Labels are short and match the code name. Abbreviate consistently: `Accel.`, `Ang. vel.`, `Res`,
`Occu`. Flag and chip labels are upper case. Menu items that name a catalogue entry stay lower case.

## 4. Spacing and sizes

| Constant | Value | Meaning |
| --- | --- | --- |
| `ui_standard_control_size` | 25 px high | Every row, button, tab and field |
| `ui_standard_stack_spacing` | 3 px | Gap between controls inside a section |
| `ui_view_stack_spacing` | 6 px | Gap between a panel's top-level blocks and between sections |
| `ui_section_padding` / `ui_host_padding` | 6 px | Sheet padding / panel inset (root 5 + 1) |
| `ui_standard_textfield_label_width` | 118 px | Label column of every `<TextField>` |
| `ui_standard_tab_padding` | 3 px horizontal | Padding each side of a tab or tab-like option; width = text width + this on both sides. `ui_standard_button_padding` (1 px) stays the padding of ordinary buttons and labels, and is too tight for tabs. |
| `ui_title_size` | 16 px high | Section title row |
| `ui_standard_icon_button_size` | 34 x 25 px | Icon-only tool and toggle buttons; markup writes `size="icon"` |
| `ui_strip_host_padding` | 5 px horizontal | Root inset of the single-row info and detail strips |

All authored lengths are UI pixels. Do not write a pixel literal in markup or C when a constant above
fits. Omit `size=` and let the element default (`ui_standard_control_size`) and `width="fill"` apply.

## 5. Component patterns

**Panel.** `ViewHost` (dark `panel_background`) holds: a header (if the panel has a selection), a tab row,
then one `View` per tab. Each `View` is the sheet: `surface="container" border="none" padding="6"`.
Field groups inside are `surface="card" padding="6"` containers. Chip sections are
`surface="transparent" padding="0"` containers on the sheet.

**Section.** `<Label text="..." style="title" />` followed by its rows. 3 px between rows, 6 px between
sections.

**Tab row.** A `ViewSelector` whose `<Option>`s are `width="hug"`. Active tab ACTIVE, others INACTIVE. Each tab is as wide as its text plus the standard tab padding (`ui_standard_tab_padding`) on both sides, so tabs hug their text. Tabs carry no per-tab pixel `size=`, and the row is not split into equal columns (equal columns made `FLAGS` and `SHAPE` too cramped at 315 px). The same rule applies to any tab-like option.

**Text field.** `<TextField label="..." type=... io=... binding=... />`. 118 px label column, input fills
the rest. Editable = INPUT, `io="readonly"` = READONLY_INPUT automatically.
- Vector (`type="vector2"`): `field-layout="stacked"`, giving x over y with muted prefixes.
- Unit: `unit="rad"` draws a muted suffix inside the field.
- Numbers use fit-to-field formatting (drops decimals, then compact scientific) at draw time.
- Non-numeric read-only text wider than its one-row box is drawn cut short with "..." at draw time (the stored text is unchanged). Editable fields and numeric fields are not affected.

**Chip grid (on/off sets).** `<Container layout="inline_wrap" columns="2" gap="5,3">` of `<Button>`s bound
to a flag. ON = ACTIVE, OFF = INACTIVE. A label too wide for a half cell spans the whole row
automatically.

**Card.** `<Container surface="card" padding="6" layout="stack" width="fill" height="hug">` (CARD role, 6 px
padding on all sides = `ui_section_padding`). The heading (`style="title"`, amber accent bar, cream text) sits
inside the card, then the standard 25 px rows with 3 px gaps. The 118 px label column is measured from the
card's inner left edge. Cards are separated by the standard 6 px view gap. Labels, titles, `<TextField>` labels
and transparent notes inside inherit the cream text. The fields keep the standard INPUT and READONLY_INPUT
styles, and the x/y prefixes and units stay `muted_text`.
- **Card or sheet?** Use a card for a group of fields (PHYS Physics and Transform, SHAPE, TREE, World Manager
  groups, COMP components). Keep chip grids and action buttons (DELETE) on the sheet under a title, never
  inside a card (FLAGS is all sheet).
- **Mixed sections.** A section that mixes fields and chips goes in a card. An OFF chip inside a card resolves
  to the sheet fill with ink text (`UIPalette_GetRoleColoursInContext`, applied by `UIElement_ApplyRoleColours`),
  so it stays visible on the teal. ON chips stay ACTIVE amber. Views never set these colours.
- **Deprecated.** `surface="subsection"` still loads as an alias of `card`. Use `card`, or `inactive` for a
  dark teal control tile.

**Actions.** Primary action `surface="button"` (ACTIVE). Delete / remove `surface="danger"` (DANGER). A
button inside a card that is not itself a card control needs an explicit `surface=` so it does not
inherit cream text.

**Icon tool / toggle button.** `<Button>` with `binding=` and an `on_draw` glyph in `ui_icons.c`. ON =
ACTIVE, OFF = INACTIVE (teal tile, cream glyph). The glyph draws in the button's own font colour (the role
text), never its own. Tiles are `size="icon"`. Groups are separated by a parent with `layout="view_stack"`
(6 px), with no divider element.

**Read-only strip.** One or more `<Textbox io="readonly">` fields in an `inline` row, each `width="fill"`
(the row is shared equally) at the standard 25 px height, with the `ui_strip_host_padding` inset.
READONLY_INPUT. Content is plain text composed in C.

**Popup menu.** Floating over the world. Dark panel frame, tab row, sage sheet. Rows are `width="fill"`
at 25 px, so the only authored width is the popup ViewHost. Category rows are INACTIVE and become ACTIVE
while their submenu is open. Leaf actions are ACTIVE. Submenus are sheets placed with a percent offset
(`offset="1,0" offset-mode="percent"`). Floating sheets are the one place a sheet has a border: 1 px in
the `panel_background` colour, so the sheet has an edge over the world.

**Scrollbar.** Track INACTIVE fill, thumb INACTIVE text colour (cream). Drawn by `view_host_system.c`.

**Panel header.** `PanelShared_HeaderBind`: HEADER fill, black text, 2 px coral rule, amber in
multi-selection.

**No-selection state.** A panel with no target keeps its full structure (header, tabs, views, cards).
Every field shows the placeholder `UI_NO_VALUE_TEXT` (`"-"`, never `0`, because zero is real data), in
both boxes of a stacked vector, and is locked read-only (`PanelShared_SetFieldsLocked`, so it takes the
READONLY_INPUT style). Flag chips read OFF and do nothing. Lists show their existing empty-state lines.
Actions with nothing to act on (DELETE, ADD) are hidden, not greyed out, and there is no disabled-button
role.

## 6. Do and don't

| Do | Don't |
| --- | --- |
| Name a role or surface; let the loader pair the text | Set `colour_fill` and `font.colour` independently |
| Use `io="readonly"` for read-only fields | Draw your own dimmed field |
| One sage View per tab, field groups as cards | Wrap sections in bordered sage boxes, or put chip grids in cards |
| `style="title"` for every heading | Make a dark strip or band behind a heading |
| Stack vectors, put units inside the field | Put "(m/s)" in labels or inline x,y pairs |
| 2-column chip grid for on/off sets | A vertical list of full-width toggle buttons |
| Reuse `ui_standard_*` constants | Write `size="263,25"`, `padding="8"` or `offset="175,0"` |
| Size tabs to their text plus the standard padding | Give tabs pixel widths or equal columns |
| Short labels matching code names | Long or reworded labels |
| Use `muted_text` only for prefixes and units | Use brown for toggles, tabs or text |

## 7. How to apply it

**Markup**
- Panel view: `<View ... surface="container" border="none" padding="6">` then card containers
  (`surface="card" padding="6"`) for fields and transparent containers (`surface="transparent" padding="0"`)
  for chip grids.
- Heading: `<Label text="Physics" style="title" />`.
- Field: `<TextField label="Ang. vel." type="float" io="readonly" binding="..." />`; vector:
  add `field-layout="stacked"`; unit: add `unit="rad"`.
- Chips: `<Container layout="inline_wrap" columns="2" gap="5,3">` with `<Button type="simple" binding=... />`.
- Card or action colour: `surface="card"`, `"inactive"`, `"button"` or `"danger"`. Do not use `ink=`.

**C**
- Role colours: `UIPalette_GetRoleColours(palette, UI_ROLE_xxx)` returns fill, text and border together.
- Two-state controls: `UIElement_ApplyRoleColours(element, palette, active ? UI_ROLE_ACTIVE : UI_ROLE_INACTIVE)`.
- Surface text: `UIPalette_GetSurfaceText(palette, surface, &text)`.
- Setting a whole element to a role: `UIElement_ApplyRoleColours(element, palette, role)` (fill, border and role text; the only way a panel recolours a live element).
- Headers: `PanelShared_HeaderBind`.
- New palettes must define every role. Never read palette fields in a panel to build a pair by hand.

## 8. Intentional exceptions

- **Diagnostics dashboard and debug overlays** (`diagnostics.c`, UI-borders outline, viewport debug grid) are
  developer tools drawn over the world with raylib primitives. They are not part of the standard.
- **World-space drawing** (selection boxes, gizmos, tile colours, the cell-paint swatch) is game content,
  not UI chrome. The cell-paint tool glyph shows the active tile's own colour by design.
