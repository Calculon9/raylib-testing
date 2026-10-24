# UI Markup Syntax

This document describes the XML markup understood by the UI loader
(`src/engine/system/ui/ui_loader.c`). Component markup lives in
`src/engine/ui/components/*.xml` (e.g. `lpanel.xml`, `rpanel.xml`,
`state_manager.xml`) and is loaded at startup, then wired to application behaviour
in C.

It is a companion to the sizing model in
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

## Common attributes (valid on every element)

| Attribute | Values | Default | Meaning |
| --- | --- | --- | --- |
| `id` | any string | empty | Stable identifier so C code can re-acquire the element after load (`UILoader_FindById`). For a `<TextField>` the id addresses the inner textbox (see TextField notes). |
| `width` | `fixed` \| `percent` \| `fill` \| `hug` | element-specific | Per-axis size mode for the horizontal axis (see "Sizing"). |
| `height` | `fixed` \| `percent` \| `fill` \| `hug` | element-specific | Per-axis size mode for the vertical axis. |
| `size` | `"w,h"` or `"v"` | element default | The numbers the size modes use. A single `"v"` applies to both axes. Meaning depends on each axis's mode (see "Sizing"). |
| `offset` | `"x,y"` or `"v"` | `0,0` | Position offset added on top of the element's layout position (see "Offset"). |
| `offset-mode` | `fixed` \| `percent` | `fixed` | `fixed` = world units; `percent` = fraction of the parent content area. |
| `enabled` | `true` \| `false` | `true` | A disabled element is not laid out, drawn, or interacted with. |
| `layout` | `stack` \| `stacked` \| `stack_wrap` \| `inline_wrap` | `stack` | How this element arranges its **children** (container-like elements only; leaf elements ignore it). See "Layout". |
| `gap` | `"x y"` or `"v"` | layout preset gap | Inter-child spacing (literal units) that overrides the gap baked into the `layout=` preset. See "Layout". |
| `surface` | `transparent` \| `container` \| `field_row` \| `input` \| `button` | constructor default | Theme-safe fill/border override: resolves the named surface role from the active palette and replaces the element's constructor-chosen `colour_fill`/`colour_border`. An absent or unrecognised value leaves the constructor colours unchanged. |

Legacy offset spelling (still accepted): `x="…"`, `y="…"`, `offset-type="…"`. The
unified `offset`/`offset-mode` form is preferred and wins if both are present.

---

## Sizing

Sizing is **per-axis**: `width` and `height` are set independently. Each takes one
of four modes:

| Mode | The axis is… | Uses `size`? |
| --- | --- | --- |
| `fixed` | the authored literal (world units) | yes — the value |
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
  it. (`width="hug"` alone grows to fit content; `width="hug" size="4.5,0"` grows
  to fit but never exceeds 4.5.)
- Supplying a `size` value for a `fill` axis is a mistake — the value is ignored
  and the loader warns.

Examples:

```xml
<Button text="OK" size="3.0,0.5" />                   <!-- fixed 3.0 x 0.5 (size=>fixed inferred) -->
<Container title="Flags" width="hug" height="fill" /> <!-- width hugs its children, height fills the view -->
<Container title="Info" width="fill" height="hug" />  <!-- width fills, height hugs its children -->
<Container width="percent" size="0.5,1.0" />          <!-- 50% of parent width, 100% of parent height -->
<View width="fill" height="fill" />                 <!-- fills its host (the default for View/ViewHost) -->
```

---

## Offset

`offset` nudges an element **relative to the position its layout gives it** — it is
additive, not absolute positioning.

```xml
<Button text="OK" offset="0.5,0.2" />                        <!-- +0.5 x, +0.2 y in world units -->
<Container title="X" offset="0.1,0" offset-mode="percent" />  <!-- +10% of parent width -->
<Label text="Y" offset="0.3" />                              <!-- +0.3 on both axes -->
```

- `offset-mode="fixed"` (default) is world units; `offset-mode="percent"` is a
  fraction of the parent's content area.
- The parent's `layout` still drives the base placement; the offset is added on
  top. A large offset can push an element out of its expected flow (and possibly
  outside a clipped region), so prefer it for small nudges rather than re-layout.

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

`stack_wrap` pairs naturally with `width="hug" height="fill"`: the height fills the
parent (giving columns a boundary to wrap against) and the width grows as columns
are added.

`gap="x y"` tunes the spacing inserted **between** successive children (not before
the first or after the last), overriding the gap baked into the chosen `layout=`
preset. Values are space-separated (comma also accepted); a single value applies to
both axes. Units are literal (no percent gap yet). The relevant axis depends on the
flow — `gap.y` spaces a `stack`/`stack_wrap`, `gap.x` spaces an `inline`/`inline_wrap`.

---

## Elements

All elements accept the common attributes above. Listed below are each element's
**additional** attributes and its default size.

### `<Button>` — a clickable button (leaf)
Default size: standard button.

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
Default size: standard control (applied to the **row**). Convenience sugar for a
label + input laid out side by side. Equivalent to:

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

TextField notes:
- `id` and `offset` apply to the **row** as a whole, but `id` is resolvable to the
  **inner textbox** via `UILoader_FindById` (so C code that looks the field up by
  id gets the textbox, as expected).
- To position or address the input independently of a label, use a standalone
  `<Textbox>` instead.

### `<Container>` — a grouping box (optional title)
Default size: standard container. Groups its children per `layout`.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `title` | string | Optional heading. When set, the container renders a title label child and is drawn with the section edge tint. When absent, it is a plain box with the default container border (and no title slot). |

(This tag supersedes the former `<Section>`: a titled `<Container>` is what used to be
a `<Section>`.)

### `<View>` — a swappable view surface within a ViewHost
Default size: fill/fill. Children are its containers/content.

| Attribute | Values | Meaning |
| --- | --- | --- |
| `type` | app view-type name | Resolved by the application to a `ViewType` (names an application view). |
| `scroll` | `none` (default) \| `x` \| `y` \| `both` | Which axes the view scrolls. Must be authored; absent means no scrolling. |

A `<View>` typically has no authored size (it fills its host); it may still set
`width`/`height`/`size` if a non-filling view is wanted.

### `<ViewHost>` — root of a multi-view panel
Default size: fill/fill. Hosts a `<ViewSelector>` and one or more `<View>`s.

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

---

## A worked example

```xml
<ViewHost id="demo_panel" layout="stacked" initial-view="main_view">
    <ViewSelector id="demo_views" type="enumerate">
        <Option text="MAIN" view="main_view" />
        <Option text="INFO" view="info_view" />
    </ViewSelector>

    <View id="main_view" type="DEMO_MAIN_VIEW" layout="inline_wrap">
        <Container id="flags" title="Flags" width="hug" height="fill" layout="stack_wrap">
            <Button type="simple" text="ALPHA" binding="demo.alpha" action="toggle-alpha" />
            <Button type="simple" text="BETA"  binding="demo.beta"  action="toggle-beta" />
        </Container>
        <Container id="values" title="Values" width="hug" height="fill" layout="stack_wrap">
            <TextField id="demo_mass"  label="Mass"  type="float"   io="safe" />
            <TextField id="demo_count" label="Count" type="integer" io="readonly" />
        </Container>
    </View>

    <View id="info_view" type="DEMO_INFO_VIEW" enabled="false" layout="inline_wrap">
        <Container id="about" title="About" width="fill" height="hug">
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
