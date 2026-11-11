# Repository Guide

This document is a working guide to the `raylib-testing` repository. It records the project structure, build workflow, runtime architecture, and UI layout rules that are otherwise spread across source files and configuration.

## Project Overview

This is a C11 raylib application with a 2D physics/world simulation, multiple UI panels, coordinate-space helpers, and an editor-oriented gameplay screen. The project is still based on the raylib game-template structure, so the top-level README contains template placeholders and is not the best source for current application behaviour.

The main dependency is raylib 5.5. CMake downloads it through `FetchContent` unless a local raylib source directory is supplied.

## Repository Layout

- `include/` contains public headers grouped by subsystem.
- `src/` contains the application entry point, screen implementations, and grouped C implementations.
- `src/common/` contains shared low-level utilities such as collections, memory, math, colour, and input helpers.
- `src/engine/` contains the runtime engine modules, including physics, world, camera, editor, and UI systems.
- `src/resources/` contains runtime resources copied beside the built executable.
- `projects/VS2022/` contains the Visual Studio solution and project files from the template.
- `docs/` contains technical and architectural notes.
- `build/` contains generated CMake files, object files, executables, and downloaded dependencies. It is generated output, not source.
- `.vscode/tasks.json` defines the supported CMake tasks.
- `.vscode/launch.json` defines the Visual Studio debugger and the PATH-based debugger configurations.

Important source areas include:

| Area | Responsibility |
| --- | --- |
| `src/raylib_game.c` | Application globals, screen lifecycle, transitions, and the main loop. |
| `src/engine/system/` | Cross-cutting systems such as panels, input routing, viewport management, debugging, and utility functions. |
| `src/engine/ui/` | UI element trees, layout, constructors, text fields, rendering, and input handling. |
| `src/engine/world/` | Universe and world state, world rendering, and entity lookup. |
| `src/engine/physics/` | Newtonoid/entity physics and related simulation data. |
| `src/common/math/` | Vectors, geometry, coordinate spaces, and transforms. |
| `src/common/memory/` and `src/common/collections/` | Allocation wrappers, pools, arrays, and maps. |
| `src/engine/camera/` and `src/common/input/` | Camera transforms and shared pointer/drag input state. |

CMake recursively includes `.c` files under `src/`, excluding generated `CMakeFiles` directories and `DEPRECATED` directories. A new source file under `src/` is therefore normally picked up automatically after CMake reconfiguration.

## Build and Debug

The preferred Windows workflow is the VS Code task `Build (PATH)`. It configures Debug output in `build/Debug` and then builds it:

```text
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build/Debug
cmake --build build/Debug
```

The resulting executable is normally:

```text
build/Debug/raylib-game/raylib-game.exe
```

The `Build` task uses the Visual Studio 2026 generator and writes to `build`, with the executable under `build/raylib-game/Debug/`.

The matching VS Code launch configurations are `Build and Debug (PATH)` and `Build and Debug`. Stop a running debugger before rebuilding if the executable is locked by the active process.

CMake copies `src/resources/` into the executable directory after a native build. The debugger working directory is set to that directory so relative resource paths resolve correctly.

There is currently no dedicated test file or test target in the repository. Use the configured CMake build as the compile/link check and exercise runtime behaviour through the debugger.

## Runtime Structure

`src/raylib_game.c` owns the application-level screen handler table. The application moves through the logo, title, and gameplay screens using paired direct and transition initialisers, update/draw callbacks, and finish codes.

Gameplay initialisation sets up the viewport, world/universe systems, UI systems, and editor state. Each frame generally routes input through the UI and world systems, updates simulation state, and draws the active world and UI layers.

The UI is a tree of `UIElement` nodes. Each node stores:

- authored and resolved offsets;
- child spacing and padding;
- a size mode and dimensions;
- local and screen layout boxes;
- optional type-specific data;
- parent, first-child, and next-sibling links;
- enabled, focus, dirty, draggable, and interaction state.

`UI_LayoutSubtree()` performs layout in this order:

1. Measure enabled descendants and padding.
2. Resolve the current element against its parent box.
3. Distribute immediate children according to child spacing.
4. Recursively lay out each child.

Use the shared constructors in `src/engine/ui/ui_constructors.c` and `CreateUIContainer()` when creating panel controls. They centralise palette colours, spacing, enabled state, dragging, and common text-field/button setup.

## UI Size Modes

> For authoring UI from XML markup (tags, attributes, bindings), see
> [`UI_MARKUP_SYNTAX.md`](UI_MARKUP_SYNTAX.md). This section covers the underlying
> sizing model that markup's `width`/`height`/`size` attributes map onto.

UI layout values (sizes, offsets, padding, gaps, border weights) are authored in **UI pixels**.
Each panel's coordinate space is built in `ViewHostSystem_InitRoot` so one layout unit is one UI
pixel, drawn at the integer UI pixel scale (`GetViewportUIPixelScale()`, chosen from the window
height; F9/F10 step it). Bitmap fonts and border weights are multiplied by the same scale at draw
time, so text and lines stay crisp. The docked panel regions themselves are still sized in game
units by `viewport_system.c`.

Sizing is **per-axis**. `Size` holds one authored `dimensions` pair plus an independent
`AxisSizeMode` for each axis — `width_mode` and `height_mode`. Every element always has a
mode on both axes; the only question is whether it is authored in markup or supplied by a
default (see "Where sizes come from" below).

```c
typedef enum { AXIS_FIXED, AXIS_PERCENT, AXIS_FILL, AXIS_HUG } AxisSizeMode;

typedef struct {
    Vector2d dimensions;   // Per-axis authored value; its meaning depends on that axis's mode.
    AxisSizeMode width_mode;
    AxisSizeMode height_mode;
} Size;
```

| Axis mode | Meaning on that axis | Reads `dimensions`? |
| --- | --- | --- |
| `AXIS_FIXED` | The authored literal (UI pixels). | Yes — it is the value. |
| `AXIS_PERCENT` | A 0..1 fraction of the parent content extent. | Yes — as the fraction. |
| `AXIS_FILL` | Take all remaining parent content extent on that axis. | No. |
| `AXIS_HUG` | Shrink to fit children + padding. If the authored dimension on that axis is `> 0` it is a **max cap** (hug but never exceed it); `0`/absent means uncapped. | Optionally — as a cap. |

Because the two axes are independent, a single element can mix modes. The common
combinations have convenience macros in `include/ui/ui.h`:

```c
#define UI_SIZE_FIXED(w, h)      ((Size){{(w),(h)}, AXIS_FIXED,   AXIS_FIXED})
#define UI_SIZE_PERCENT(w, h)    ((Size){{(w),(h)}, AXIS_PERCENT, AXIS_PERCENT})
#define UI_SIZE_FILL             ((Size){{0,0},     AXIS_FILL,    AXIS_FILL})
#define UI_SIZE_HUG              ((Size){{0,0},     AXIS_HUG,     AXIS_HUG})    // fit children, both axes
#define UI_SIZE_HUG_CAPPED(w, h) ((Size){{(w),(h)}, AXIS_HUG,     AXIS_HUG})    // fit children, capped per axis
#define UI_SIZE_HUG_WIDTH        ((Size){{0,0},     AXIS_HUG,     AXIS_FILL})   // width hugs, height fills
#define UI_SIZE_HUG_HEIGHT       ((Size){{0,0},     AXIS_FILL,    AXIS_HUG})    // width fills, height hugs
```

The "hug" macros name the **hugged (content-driven) axis**; the other axis fills the parent.
`AXIS_HUG` with a cap replaces the former dedicated "content max" mode — a capped hug is
just `AXIS_HUG` plus a `dimensions` value.

The layout helpers in `src/engine/ui/ui.c` resolve each axis independently: `width_mode`
drives horizontal decisions and `height_mode` drives vertical decisions, through the two
pure resolvers `ResolveAxisExtent` and `ResolveMeasurementAxisAvailable`. For a `HUG` axis,
changing a `0` placeholder dimension to another `0` must not change layout; a non-zero value
turns it into a cap.

Content measurement follows the UI tree: it measures enabled children and the element's
padding. A leaf label or textbox does not become text-metric-sized merely by hugging; give
leaf controls an explicit size or place them inside a measured container.

### Authoring size in XML

Markup sets the mode with two attributes and supplies numbers with `size`:

```xml
<Container width="hug"  height="fill" layout="stack_wrap" />  <!-- width hugs content, height fills -->
<Container width="fill" height="hug" />                             <!-- width fills, height hugs content -->
<Button  size="161,25" width="fixed" height="fixed" />          <!-- explicit fixed box -->
<Button  size="161,25" />                                       <!-- same: size= with no mode infers fixed -->
```

- `width` / `height` take `fixed` | `percent` | `fill` | `hug`.
- `size="w,h"` supplies the numbers; their meaning depends on each axis's mode (literal for
  fixed, fraction for percent, **max cap for hug**, ignored for fill). A single `size="v"`
  applies to both axes.
- `offset="x,y"` + `offset-mode="fixed"|"percent"` position the element (legacy `x=`/`y=`/
  `offset-type=` are still accepted).

**Per-axis mode inference** (applied to width and height independently, in
`UILoader_ExtractCommonAttrs`): an explicit `width`/`height` wins; otherwise if `size`
provides a value for that axis the mode defaults to `fixed`; otherwise the axis takes the
builder's natural default. The loader emits a `LOADER_WARNING` when a `size` value is
supplied to a `fill` axis (the value is dead) or a `percent` value falls outside `0..1`.

### Where sizes come from

Each element's effective size resolves through three layers, highest priority first:

1. **Explicit** markup — `width`/`height` (and `size` for the numbers).
2. **Inferred** — `size` with no mode implies `fixed` per axis.
3. **Builder default** — neither given, so the builder supplies a `Size`. Buttons default to
   `ui_standard_button_size`, text fields to `ui_standard_control_size`, containers
   to `ui_standard_container_size`, and `<View>`/`<ViewHost>` to `ui_fill_container_size`
   (fill/fill). Buttons and text-field rows are **fill width x fixed 0.5 height**, so they
   span their container's inner width. All default constants live in
   `src/engine/system/ui_system.c`.

**Text-field label column.** A `UI_ELEMENT_TEXTFIELD` row (built by
`CreateLabeledFieldElement`) holds a fixed-width label column and a fill-width input placed
at a fixed x offset equal to the column. `AlignTextFieldLabelColumns` in `ui.c` runs at the
start of each element's measure: it sizes one column for all text-field rows that share a
parent, from the longest label's bitmap-font run + 2 px (capped at 60 % of the parent's
content width), so labels such as "Restitution" are never truncated. Text runs are measured in
UI pixels directly, so the column is exact from the first layout; `ui_standard_textfield_label_width`
(118 px) is only the constructor's initial value.

### UI style (surfaces, borders, spacing)

The editor style is set once in the shared layer (`ui_system.c` palette and constants,
`ui_constructors.c`, `ui_loader.c`, `view_host_system.c`, `ui_renderer.c`):

- **Surfaces.** `ViewHost`/`View` use `UI_PALETTE_SURFACE_PANEL` (dark `panel_background`,
  no border). Titled containers and `CreateViewSection*` are boxed sections
  (`container_fill`, 1 px `container_border`, `ui_section_padding` = 6 px).
  Untitled containers are transparent layout boxes. Text-field rows and their labels are
  transparent; inputs use the `input` surface; buttons the `button` surface; destructive
  buttons the `danger` surface (`button_danger_fill`, set in every palette).
- **Two-state colours.** Tabs, kind chips and bound text toggles all use
  `UIPalette_GetToggleColours`: active = `toggle_on_fill` + dark `text`, inactive =
  `toggle_off_fill` + `text_on_dark`.
- **Borders.** The renderer draws a border (inside the box, `border_weight` UI px, default 1) whenever
  `colour_border.a > 0`; constructors decide which elements carry one. The UI-borders debug
  overlay (F3) adds a faint outline to borderless elements. Bordered textboxes inset their
  text by the border weight + 4 px from each side.
- **Spacing.** Panel inset 6 px (root 5 + `ui_host_padding` 1), 6 px between a panel's
  top-level blocks (`ui_view_stack_spacing`), 3 px between controls inside a section
  (`ui_standard_stack_spacing`), titles one 16 px FONT_LARGE row (`ui_title_size`).

`<View>` and `<ViewHost>` default to fill/fill but now also honour authored
`width`/`height`/`size`/`offset`. The concrete magnitudes enter at the application-built
root and propagate down a fill chain:

```
panel->space (columns × rows, set by the application)
  -> UI_ELEMENT_ROOT        (view_host_system.c, AXIS_FILL both axes)
    -> <ViewHost> container  (fill/fill, fills the root)
      -> <View> container    (fill/fill, fills the host)
        -> <Container>       (e.g. width=hug / height=fill: hugs content, fills vertically)
          -> fields / buttons
```

Everything from the root down to the containers is `fill`, carrying the panel's size downward
in UI pixels (the renderer draws one UI pixel as `GetViewportUIPixelScale()` screen pixels); the containers then switch to `hug`
on one axis to size to their own content.

## UI Spacing and Offsets

`Spacing` combines a spacing vector, a number form (`NONE` or `PERCENT`), and a layout type. Active layout types include stacked, inline, stacked-wrap, inline-wrap, and normal/manual distribution.

`SPACING_NONE` does not distribute children. Its vector and number form are irrelevant, so use the shared preset:

```c
#define UI_SPACING_NONE ((Spacing){{0.0f, 0.0f}, NONE, SPACING_NONE})
```

The `Offset` mode still matters even for a zero offset. `OFFSET_FIXED` and `OFFSET_PERCENT` affect how positions are interpreted during layout, and dragging converts percentage offsets to fixed offsets before applying movement. Do not replace all zero offsets with one universal preset without considering that behaviour.

`SPACING_OVERLAYED` and `ALIGNED_CENTRE` are declared in the public types, but they do not currently have an active layout implementation. Treat them as reserved until their resolver behaviour is added.

## Collision Pipeline

`UpdateWorld` splits collision handling into DETECT and SOLVE (`src/engine/world/world.c`, `world_physics.c`, `world_contacts.c`; the pure solver is `include/world/contact_cache.h`):

1. `PhysicsUpdateJob` integrates, refreshes bounds and fills the occupancy store.
2. DETECT: `DetectContainerContacts`, `DetectSolidCellContacts` and `DetectCollisionPair` run the gates, SAT, sensor/projectile callbacks and collision markers once per contact, and write one cached contact each (`ContactCache`). Nothing is pushed here.
3. SOLVE: `SolveWorldContacts` sorts contacts bottom-up, warm starts from last frame's impulses (matched by `ContactKey_*`), runs K sequential-impulse velocity iterations with accumulated clamped normal impulses and Coulomb friction, then a separate NGS position phase that moves bodies without changing velocity. Container and solid-cell contacts use the solver's immovable body slot 0. Only the container snaps to full depth; solid cells are ordinary soft static contacts. `ApplySolverResults` is the only place entities are written, and sleepers wake only on a real impulse.
4. `ClampEntitiesToContainer` (centre safety net), then `RecoverEntitiesFromAbsentCells`.

Tunables live in `ContactSolverConfig` (`ContactSolverConfig_Default()`); `PROF_PAIRS` times detection and `PROF_SOLVE` the solve.

## State and Ownership

The UI and world systems use global state, including `G_UIState`, the universe, panel instances, and shared frame counters. Selection should go through the central UI selection setters so the selected entity ID, pointer, cell, and callback remain consistent.

There is no separate "current world" state. The context world is derived on every call from the `SelectionSet` primary by `UIState_GetContextWorldIndex()` / `UIState_GetContextWorld()` (`ui_system.c`): a world primary is that world, an entity or cell primary resolves to its owning world, and an empty set or stale primary resolves to the root world (`UNIVERSE_ROOT_WORLD_INDEX`, never `-1`). The World Manager (right panel), debug spawn, world pause hotkey, geometry editor and world-flag fallback all read it. Creating a world (`CreateNewWorld(true)`) replaces the selection with the new world, so it becomes the context world.

The project uses manual memory management. UI elements are allocated from a pool when possible, while collections may own separate internal buffers. Read [OWNERSHIP.md](../OWNERSHIP.md) before changing allocation or collection lifetimes, especially for embedded collections and shallow disposal functions.

New allocation and copy operations in module code should use the wrappers in `memory/cmemory.h` where an equivalent wrapper exists.

## World Growth

Painting a cell with the cell-paint tool just outside a nested world's grid grows that world's dense row-major `Cell[]` (all four directions, capped at `MAX_WORLD_GRID_DIMENSION` = 512 per axis). Negative growth shifts the integer `Space2d.grid_origin` so cells and entities keep their local coordinates. Cells, commands, `TargetRef` and selection are identified by ABSOLUTE lattice coordinates (world + `lattice_x`/`lattice_y`), which survive growth; the dense index is resolved at the point of use. Growth happens ONLY in the command drain (`Universe_GrowWorldToInclude`); never call it elsewhere because it reallocates the cell buffer. Pure helpers live in `include/world/world_growth.h`.

## Working Conventions

Follow [CONVENTIONS.md](../CONVENTIONS.md) for C naming, indentation, initialisation, spacing, and file naming. Keep public declarations in the appropriate `include/` module header and implementations in the matching `src/` area.

Prefer existing subsystem helpers over ad-hoc state or duplicated UI construction. Keep generated build output under `build/` and do not add generated CMake files to source directories.

## Existing Documentation

- [README.md](../README.md) contains the original raylib-template setup notes and still has application placeholders.
- [CONVENTIONS.md](../CONVENTIONS.md) contains the repository's C and file naming conventions.
- [OWNERSHIP.md](../OWNERSHIP.md) documents manual memory and collection ownership rules.
- [PERFORMANCE_TECHNICAL_ANALYSIS.md](PERFORMANCE_TECHNICAL_ANALYSIS.md) records the performance optimisation analysis and reported results.
- [AGENTS.md](../AGENTS.md) contains repository-specific instructions for automated coding work.

