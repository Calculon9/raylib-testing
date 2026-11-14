# UI Markup Syntax

This document describes the XML markup understood by the UI loader
(`src/engine/system/ui/ui_loader.c`). Component markup lives in
`src/engine/ui/components/*.xml` (e.g. `lpanel.xml`, `world_manager.xml`,
`state_manager.xml`) and is loaded at startup, then wired to application behaviour
in C.

It is a companion to [UI_STYLE_GUIDE.md](UI_STYLE_GUIDE.md), which says which roles, titles, fields and chips to author
(this file only says what the attributes mean), and to the sizing model in
[`REPOSITORY_GUIDE.md`](REPOSITORY_GUIDE.md) ("UI Size Modes") — read that for the
per-axis sizing semantics this document references.

---

## How loading works (the short version)

Every element goes through one uniform path:

1. The loader reads the **common attributes** that apply to *every* element (id,
   size, offset, layout, enabled) into one place.
2. It dispatches to the **per-type builder** for the tag (`<Button>`, `<Container>`,
   …), which reads that tag's own special attributes and constructs the element.
3. It applies the common remainder (id, enabled, offset) and recurses into child
   elements.

Practically, this means **the common attributes below work on any element**, and
each tag additionally has its own attributes.

When an attribute is omitted, the element falls back to a sensible default (noted
per attribute). Attribute-less markup renders at each element's standard size.

---

## Units

All authored lengths — `size`, `offset`, `padding`, `gap`, `border-weight` — are in
**UI pixels**: whole numbers that match the design mocks one-to-one. Percent values
(`width/height="percent"`, `offset-mode="percent"`) stay 0..1 fractions.

The UI is drawn at an integer **UI pixel scale**: one UI pixel = N screen pixels, with
N chosen from the window height (1 up to 900 px tall, 2 around 1800 px, and so on).
Boxes, borders, text and text insets all scale together in whole-pixel steps, so the
bitmap fonts and 1 px lines stay crisp. **F9/F10** step the scale down/up (stepping
below 1 returns to automatic). Prefer whole numbers; fractional values are allowed but
land between screen pixels.

---

## Common attributes (valid on every element)

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| `id` | any string | empty | Stable identifier so C code can re-acquire the element after load (`UILoader_FindById`). For a `<TextField>` the id addresses the inner textbox (see TextField notes). |
| `width` | `fixed` \| `percent` \| `fill` \| `hug` | element-specific | Per-axis size mode for the horizontal axis (see "Sizing"). |
| `height` | `fixed` \| `percent` \| `fill` \| `hug` | element-specific | Per-axis size mode for the vertical axis. |
| `size` | `"w,h"` or `"v"` | element default | The numbers the size modes use. A single `"v"` applies to both axes. Meaning depends on each axis's mode (see "Sizing"). |
| `offset` | `"x,y"` or `"v"` | `0,0` | Position offset added on top of the element's layout position (see "Offset"). |
| `offset-mode` | `fixed` \| `percent` | `fixed` | `fixed` = UI pixels; `percent` = fraction of the parent content area. |
| `enabled` | `true` \| `false` | `true` | A disabled element is not laid out, drawn, or interacted with. |
| `layout` | `stack` \| `stacked` \| `stack_wrap` \| `inline_wrap` | `stack` | How this element arranges its **children** (container-like elements only; leaf elements ignore it). See "Layout". |
| `gap` | `"x y"` or `"v"` | layout preset gap | Inter-child spacing (UI pixels) that overrides the gap baked into the `layout=` preset. See "Layout". |
| `padding` | `"h,v"` or `"v"` | container's constructor default | Inner inset (UI pixels) between a container's own box edge and its children's content rectangle. A single `"v"` insets all four sides equally; `"h,v"` insets left+right by `h` and top+bottom by `v`. **Containers only** — see "Padding" below. |
| `ink` | `light` \| `dark` | element default | Text colour override for a Label, Button or Textbox caption: `light` = the INACTIVE role text (cream), `dark` = the ACTIVE role ink. Rarely needed: a `button`, `danger`, `inactive` or `card` surface already brings its paired text colour, and everything inside a `card` inherits the card text (filled Textboxes keep their own ink). On a `<TextField>` it recolours the row's label only (the input keeps dark ink on its cream cell). Ignored on elements without text; any other value warns. | Static description shown in the **detail strip** while the pointer is over this element (or any descendant without its own hint). Live values (state, active selection) are appended in C through the element's `on_hint` callback. The detail strip textbox holds 63 characters, so keep hints short; longer text is truncated with `...`. |
| `surface` | `transparent` \| `container` \| `field_row` \| `input` \| `button` \| `panel` \| `danger` \| `inactive` \| `card` \| `subsection` | constructor default | Theme-safe fill/border override: resolves the named surface role from the active palette and replaces the element's constructor-chosen `colour_fill`/`colour_border`. `card` = the CARD role (dark teal with cream text; labels, titles and transparent notes inside inherit the cream); `inactive` = the INACTIVE role (dark teal tile for OFF controls); `subsection` is a **deprecated** alias of `card`; `panel` = dark `panel_background`, no border; `danger` = coral `button_danger_fill` with the 1 px button border (destructive buttons such as DELETE); `subsection` = the INACTIVE colour role (dark teal `subsection_fill`, `input` border, cream text for the element and every caption or field label inside it); `button` = ACTIVE (amber + ink) and `danger` = DANGER (coral + ink) likewise set the element's text. See "Colour roles" in REPOSITORY_GUIDE.md. An absent or unrecognised value leaves the constructor colours unchanged. Borders are drawn whenever the border colour is not transparent (see "Borders"). |
| `border` | `none` \| `transparent` \| `container` \| `field_row` \| `input` \| `button` \| `panel` \| `danger` \| `inactive` \| `card` \| `subsection` | surface/constructor default | Border-colour override only: `none` removes the border, any other value uses that surface role's palette border colour. Applied **after** `surface`, so `surface="container" border="none"` is a filled box with no outline. An absent or unrecognised value leaves the border unchanged. Works on every element. |
| `border-weight` | whole number `0`–`16` | surface role weight (1) | Border line thickness in **UI pixels** (drawn at the UI pixel scale like everything else). `0` hides the line. Applied last, so it wins over the `surface` role's weight. A non-integer or out-of-range value is ignored with a warning. Works on every element. |

Legacy offset spelling (still accepted): `x="…"`, `y="…"`, `offset-type="…"`. The
unified `offset`/`offset-mode` form is preferred and wins if both are present.

---

## Sizing

Sizing is **per-axis**: `width` and `height` are set independently. Each takes one
of four modes:

| Mode | The axis is… | Uses `size`? |
| --- | --- | --- |
| `fixed` | the authored literal (UI pixels) | yes — the value |
| `percent` | a 0..1 fraction of the parent's content extent | yes — the fraction |
| `fill` | all remaining parent space on that axis | no (value ignored) |
| `hug` | shrunk to fit the children + padding | optional — if a `size` value is given it acts as a **max cap**, otherwise uncapped |

Rules:

- **Mode inference.** An explicit `width`/`height` always wins. If you give `size`
  but no mode for an axis, that axis defaults to `fixed`. If you give neither, the
  axis uses the element's builder default.
- `size` only supplies numbers; it never changes the mode by itself (except the
  inference above).
- A `hug` axis is **uncapped** unless you give it a `size` value, which then caps
  it. (`width="hug"` alone grows to fit content; `width="hug" size="225,0"` grows
  to fit but never exceeds 225 px.)
- Supplying a `size` value for a `fill` axis is a mistake — the value is ignored
  and the loader warns.

Examples:

```xml
<Button text="OK" size="150,25" />                    <!-- fixed 150 x 25 px (size=>fixed inferred) -->
<Button size="icon" />                                <!-- the shared icon button size (ui_standard_icon_button_size, 34 x 25 px) -->
<Option text="PHYS" view="phys_view" width="hug" />   <!-- tab: text width + ui_standard_tab_padding each side; buttons with text hug their text -->
<Container layout="inline" width="fill"><Textbox width="fill" /><Textbox width="fill" /></Container> <!-- inline fill siblings share the row equally -->
<Container width="hug" height="fill" />            <!-- width hugs its children, height fills the view -->
<Container width="fill" height="hug" />            <!-- width fills, height hugs its children -->
<Container width="percent" size="0.5,1.0" />          <!-- 50% of parent width, 100% of parent height -->
<View width="fill" height="fill" />                 <!-- fills its host (the default for View/ViewHost) -->
```

---

## Offset

`offset` nudges an element **relative to the position its layout gives it** — it is
additive, not absolute positioning.

```xml
<Button text="OK" offset="25,10" />                           <!-- +25 px x, +10 px y -->
<Container offset="0.1,0" offset-mode="percent" />  <!-- +10% of parent width -->
<Label text="Y" offset="15" />                               <!-- +15 px on both axes -->
```

- `offset-mode="fixed"` (default) is UI pixels; `offset-mode="percent"` is a
  fraction of the parent's content area.
- The parent's `layout` still drives the base placement; the offset is added on
  top. A large offset can push an element out of its expected flow (and possibly
  outside a clipped region), so prefer it for small nudges rather than re-layout.

---

## Padding (inner inset)

`padding` sets an **inner inset** between a container's own box edge and the
content rectangle its children lay out into. It parses exactly like `size`/`offset`
and is measured in UI pixels:

```xml
<Container padding="25" />                    <!-- 25 px inset on all four sides -->
<Container padding="25,12" />                 <!-- 25 px left+right, 12 px top+bottom -->
<Container padding="0" />                     <!-- removes the container's default inset -->
```

- **Containers only.** `padding` applies to `Container`, `View`, `ViewHost` and
  `ViewSelector`. On any other element it is read but ignored (leaf elements such
  as `Button`/`Label`/`TextField`/`Textbox` keep their own built-in text/visual
  padding).
- **Uniform vs per-axis.** A single `"v"` insets all four sides equally; `"h,v"`
  insets left+right by `h` and top+bottom by `v`. The value is a per-side amount on
  each axis.
- **Default.** When `padding` is absent, the container keeps its constructor
  default inset: `<Container>` 3 (a boxed section authors `padding="6"` for
  the 6 px section padding), `<ViewHost>` 1 (added to the panel root's 5, giving a
  6 px panel inset), `<View>` 0 (its host already supplies the inset) and
  `<ViewSelector>` 0. Authoring `padding="0"` explicitly removes that default inset.
- **Composition.** The content rectangle is shrunk by the padding *before* children
  are placed, so:
  - A `fill` child fills the box **minus** the padding.
  - A `hug` container grows by `2 × padding` on the hugged axis (it wraps its
    children plus the inset).
  - `percent`/`fixed` children resolve against the already-padded content extent.
  - Inter-child `gap` and child `offset` still apply **inside** the padded area —
    both compose with padding independently.

### Borders

Every element's border (1 px by default) is drawn whenever its border colour is not fully
transparent; the line sits **inside** the box, so a bordered element keeps its
layout size and padding is measured from the outer edge. The thickness comes from the
`surface` role (palette `*_border_weight`, default 1 px) and can be overridden with
`border-weight`; a weight larger than the padding makes the border overlap children. Which elements carry a
border is decided by their surface (or an explicit `border=`): sections (`container`),
inputs (`input`), buttons/tabs/toggles (`button`) and `danger` show one; `panel`,
`transparent` (plain containers, tab bars, text-field rows and labels) do not. A textbox with a
visible border insets its text by the border weight + 4 px (5 px at the default weight) from each side. The UI-borders debug overlay
(F3) additionally outlines borderless elements faintly so their bounds are visible.

---

## Layout (child arrangement)

`layout` controls how a container arranges its **children**. Leaf elements
(`Button`, `Label`, `TextField`, `Textbox`) ignore it.

| Value | Arrangement |
| --- | --- |
| `stack` | Vertical stack in sibling order (default). |
| `stacked` | Alias of `stack`. |
| `stack_wrap` | Vertical columns; wraps to a new column when a column fills the available height. |
| `inline_wrap` | Horizontal rows; wraps to a new row when a row fills the available width. |
| `view_stack` | A `stack` column with the 6 px `ui_view_stack_spacing` gap between children (the panel block spacing), for non-panel parents such as the toolbar's button groups. |

`stack_wrap` pairs naturally with `width="hug" height="fill"`: the height fills the
parent (giving columns a boundary to wrap against) and the width grows as columns
are added.

`gap="x y"` tunes the spacing inserted **between** successive children (not before
the first or after the last), overriding the gap baked into the chosen `layout=`
preset. A `<ViewHost>` or `<View>` whose layout resolves to `stack` and has no
`gap=` uses a 6 px gap between its top-level blocks (header, tab row,
sections); other containers keep the preset's 3 px gap. Values are space-separated (comma also accepted); a single value applies to
both axes. Units are UI pixels (no percent gap yet). The relevant axis depends on the
flow — `gap.y` spaces a `stack`/`stack_wrap`, `gap.x` spaces an `inline`/`inline_wrap`.

**Button grid (`columns="N"`, `<Container>` only).** With `layout="inline_wrap"`, `columns="N"` (1..8) sizes every direct `<Button>` child to one of N equal columns: cell width = (content width − (N−1) × `gap.x`) / N. A button whose text run is wider than a cell minus 6 px (`UI_GRID_CHIP_TEXT_MARGIN_PX`) spans the whole row instead, so it takes a row of its own. Placement is the ordinary `inline_wrap` flow (left to right, wrapping when the row is full), so `gap="5,3"` gives a 5 px column gap and 3 px row gap. Widths are recomputed every layout pass from the container width, so no per-button size is authored. Put a section title in the parent, not inside the grid, so it stays on its own row. Used by the FLAGS chip grid in `state_manager.xml`.

---

## Elements

All elements accept the common attributes above. Listed below are each element's
**additional** attributes and its default size.

### `<Button>` — a clickable button (leaf)
Default size: standard button — `width="fill"` (spans its container's inner width)
and a fixed 25 px height. Author `size="w,h"` for a fixed-width button. Bound toggle
buttons show ON as the ACTIVE role (amber with ink) and OFF as the INACTIVE role (dark teal with cream text) (the same pair as tabs and kind chips). Inside a `card` an OFF toggle resolves to the sheet fill with ink text so it stays visible on the teal; ON stays amber.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `text` | string | Button label. |
| `type` | `simple` (default) \| `enumerate` \| `submit` | Button kind. `enumerate` is used for view-selector toggles; `submit` for commit-style actions. |
| `binding` | resolver name | A display **source**: makes the button show live state (resolved by the application's binding resolver). |
| `action` | resolver name | A click **sink**: what the button does when clicked (resolved by the application's action/command resolver). |

A button with only `action` behaves as a plain command button; with only
`binding` it is a read-only live label; with both it shows state and acts on click.

### `<Label>` — static text (leaf)
Default size: standard control.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `text` | string | The label text. |
| `style` | `title` | Optional. `title` makes a section heading: a 4x16 px amber accent bar (palette `button_fill`) at the left edge, then the title in the large font and dark ink (`title_text`) starting 10 px right of the bar's left edge (`UI_TITLE_*` in `ui.h`). The width hugs the measured text (including the indent) with zero padding. An authored `size`/`width`/`height` overrides the title size. |

A `<Label>` is a text leaf and, having no `layout=`, keeps `child_spacing` at
`SPACING_NONE`. It is still a tree node, so the generic layout and draw passes will
lay out and draw any child elements authored inside it — but because its spacing is
`SPACING_NONE`, those children are positioned purely by their own `offset=` relative
to the label's content box (no stacked/inline distribution is applied). This is used
by the popup creation menu, where each ENTITY/WORLD label hosts its fly-out submenu as
an `overflow="true"` child offset to the right of the item's row. Do not add `layout=`
to such a label: that would switch it to a stacking parent and displace the child by
the preceding content's height.

### `<Textbox>` — standalone input field (leaf)
Default size: standard control. This is the same input control `<TextField>` uses
internally, without a label or row wrapper.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `type` | `integer` \| `float` (default) \| `vector2` | How the value is parsed/formatted. |
| `io` | `readonly` \| `safe` \| `raw` (default) | Read-only, edit-with-validation, or raw-editable. |
| `binding` | resolver name | Value source (and optionally a write sink for editable fields). |
| `value` | string | Optional initial value. Parsed per `type` and seeded into the bound target (when the binding has a writable address) and shown in the field. |

### `<TextField>` — a labelled input row (composite)
Default size: standard control (applied to the **row**): `width="fill"`, fixed 25 px
height. The label is a fixed-width column and the input fills the rest of the row
to the container's inner right edge. All `<TextField>`s that share a parent get one
label column, sized at layout time to the longest label among them (+2 px, capped
at 60 % of the parent's content width), so labels are never truncated. Convenience
sugar for a label + input laid out side by side. Roughly equivalent to:

```xml
<Container layout="inline"><Label text="…"/><Textbox …/></Container>
```

| Attribute | Values | Meaning |
| --- | --- | --- |
| `label` | string | The field label (left side). |
| `type` | `integer` \| `float` (default) \| `vector2` | Value parse/format type. |
| `io` | `readonly` \| `safe` \| `raw` (default) | Input editability. |
| `binding` | resolver name | Value source (and optional write sink). |
| `value` | string | Optional initial value. Parsed per `type` and seeded into the bound target (when the binding has a writable address) and shown in the field. |
| `field-layout` | `inline` (default) \| `stacked` | `stacked` is the stacked field layout (see below). |
| `unit` | string (max 7 chars) | Optional suffix (for example `rad`) drawn inside the field, right-aligned, in the muted palette colour (`muted_text`). |

Stacked field layout (`field-layout="stacked"`): the label column is fixed at
`ui_standard_textfield_label_width` (118 px) and skipped by the longest-label pass, so every stacked
field in a view shares one column. A `type="vector2"` field is two control rows plus the stack gap
tall (25 + 3 + 25 px): the label sits level with the first row and the field draws its **x** box on
that row and its **y** box on the next, in the same column and width. Each box shows a muted `x` / `y`
prefix with the value starting 16 px after it. It stays ONE textbox (one text buffer, binding and
hit area; the `(x,y)` text is only split for drawing), so bindings and editing are unchanged. While
focused, the field draws as one tall box showing the full `(x,y)` edit text. Scalars are one
full-width box with no prefix.

Fit-to-field: an unfocused numeric value (`float`, `integer`, or a stacked vector component) that is
wider than its box is drawn with fewer decimal places, then in compact scientific notation (for
example `1.23e5`). The stored text is not changed, so a focused field always shows the full value.

Read-only fields: every `io="readonly"` text field with a visible box (and any field locked at run
time) is drawn with the palette `readonly_fill` / `readonly_border` (dimmed cream with a muted
border) in place of the editable cream fill and dark border; the geometry is identical.

TextField notes:
- `id` and `offset` apply to the **row** as a whole, but `id` is resolvable to the
  **inner textbox** via `UILoader_FindById` (so C code that looks the field up by
  id gets the textbox, as expected).
- To position or address the input independently of a label, use a standalone
  `<Textbox>` instead.

### `<Container>` — a grouping box
Default size: standard container. Groups its children per `layout`. It has no tag-specific attributes; it is a **transparent**, borderless layout box (3 px padding) until styled with the generic `surface`, `border` and `padding` attributes.

A boxed, titled **section** is authored explicitly:

```xml
<Container id="physics" width="fill" height="hug" surface="container" padding="6">
    <Label text="Physics" style="title" />
    ...
</Container>
```

(This tag supersedes the former `<Section>`. There is no `title=` attribute: the heading is an ordinary `<Label style="title">` child.)

A **continuous sheet** (no boxed sections) puts the fill on the `<View>` instead
(`surface="container" border="none" padding="6"`) and uses `surface="transparent" padding="0"`
containers purely to group each titled section, as the PHYS view of `state_manager.xml` does.

### `<View>` — a swappable view surface within a ViewHost
Default size: fill/fill. Children are its containers/content.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `type` | app view-type name | Resolved by the application to a `ViewType` (names an application view). |
| `scroll` | `none` (default) \| `x` \| `y` \| `both` | Which axes the view scrolls. Must be authored; absent means no scrolling. |

A `<View>` typically has no authored size (it fills its host); it may still set
`width`/`height`/`size` if a non-filling view is wanted.

### `<ViewHost>` — root of a multi-view panel
Default size: fill/fill. Hosts a `<ViewSelector>` and one or more `<View>`s. Both
`<ViewHost>` and `<View>` draw the dark `panel` surface (no border).

| Attribute | Values | Meaning |
| --- | --- | --- |
| `initial-view` | a child View's `id` | Which view is shown first (resolved by the application to an index). |

### `<ViewSelector>` — a toggle bar that switches views
Default size: standard selector container. Contains `<Option>` children.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `type` | `enumerate` | The only supported selector type (others reserved). |

### `<Option>` — one entry in a ViewSelector (leaf)
Default size: standard selector button.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `text` | string | Button caption. |
| `view` | a View's `id` (required) | Which `<View>` this option switches to. |

---

## Bindings and actions (brief)

`binding` and `action` attributes name **resolvers** that the application supplies
when it loads the markup — the loader itself is domain-free and never hard-codes
application names. A `binding` is a value **source** (what to display, pulled each
frame); an `action` is a **sink** (what to do on interaction). The resolver names
you can use are defined by the panel that owns the markup (see its C `Init…` code
and its resolver functions). Field values in `state_manager.xml` are currently
driven by the per-frame C refresh rather than declarative `binding=` names.

`world_manager.xml` is the right panel (the World Manager), loaded by `world_manager_system.c` exactly like `state_manager.xml`. It resolves `wm.*` bindings (query sources, with a write sink on the editable gravity, restitution, friction, basis and cell-warp fields). The panel shows the world and cell members of the selection; with neither selected it shows the **context world**: the world of the selection's primary (the owning world of a selected entity), or the root world (shown as `ROOT WORLD`) when nothing is selected. There are no world-navigation actions (`select-world-prev` / `select-world-next` no longer exist); the selection is the only way to change which world is shown.

---

## A worked example

```xml
<ViewHost id="demo_panel" layout="stacked" initial-view="main_view">
    <ViewSelector id="demo_views" type="enumerate">
        <Option text="MAIN" view="main_view" />
        <Option text="INFO" view="info_view" />
    </ViewSelector>

    <View id="main_view" type="DEMO_MAIN_VIEW" layout="inline_wrap">
        <Container id="flags" width="hug" height="fill" layout="stack_wrap" surface="container" padding="6">
            <Label text="Flags" style="title" />
            <Button type="simple" text="ALPHA" binding="demo.alpha" action="toggle-alpha" />
            <Button type="simple" text="BETA"  binding="demo.beta"  action="toggle-beta" />
        </Container>
        <Container id="values" width="hug" height="fill" layout="stack_wrap" surface="container" padding="6">
            <Label text="Values" style="title" />
            <TextField id="demo_mass"  label="Mass"  type="float"   io="safe" />
            <TextField id="demo_count" label="Count" type="integer" io="readonly" />
        </Container>
    </View>

    <View id="info_view" type="DEMO_INFO_VIEW" enabled="false" layout="inline_wrap">
        <Container id="about" width="fill" height="hug" surface="container" padding="6">
            <Label text="About" style="title" />
            <Label text="A demo panel." />
        </Container>
    </View>
</ViewHost>
```

---

## Notes and limits

- Unknown tags are skipped with a warning; unknown attribute *values* (e.g. an
  unrecognised `width` mode) warn and fall back to a default.
- `layout` values `stack_smart_wrap` / overlay / aligned-centre are reserved in the
  engine types but have no active implementation yet — do not rely on them.
- This document reflects the loader as implemented in
  `src/engine/system/ui/ui_loader.c`. If you add a builder or attribute there,
  update this file to match.
