# Repository Guide

This document is a working guide to the `raylib-testing` repository. It records the project structure, build workflow, runtime architecture, and UI layout rules that are otherwise spread across source files and configuration.

## Project Overview

This is a C11 raylib application with a 2D physics/world simulation, multiple UI panels, coordinate-space helpers, and an editor-oriented gameplay screen. The project is still based on the raylib game-template structure, so the top-level README contains template placeholders and is not the best source for current application behaviour.

The main dependency is raylib 5.5. CMake downloads it through `FetchContent` unless a local raylib source directory is supplied.

## Repository Layout

- `include/` contains public headers grouped by subsystem.
- `src/` contains the C implementations and the application entry point.
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
| `src/system/` | Cross-cutting systems such as panels, input routing, viewport management, debugging, and utility functions. |
| `src/ui/` | UI element trees, layout, constructors, text fields, rendering, and input handling. |
| `src/world/` | Universe and world state, world rendering, and entity lookup. |
| `src/physics/` | Newtonoid/entity physics and related simulation data. |
| `src/math/` | Vectors, geometry, coordinate spaces, and transforms. |
| `src/memory/` and `src/collections/` | Allocation wrappers, pools, arrays, and maps. |
| `src/camera/` and `src/input/` | Camera transforms and shared pointer/drag input state. |

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

Use the shared constructors in `src/ui/ui_constructors.c` and `CreateUIContainer()` when creating panel controls. They centralise palette colours, spacing, enabled state, dragging, and common text-field/button setup.

## UI Size Modes

`Size` contains `dimensions` and a `SizeMode`. The dimensions are meaningful for fixed, percentage, and fill modes. Content modes use named presets because their dimensions are placeholders.

| Mode | Meaning |
| --- | --- |
| `SIZE_FIXED` | Use the authored width and height, clamped to the available parent area. |
| `SIZE_PERCENT` | Use the authored width and height as proportions of the parent content area. |
| `SIZE_FILL` | Use the remaining parent width and height. In a stacked parent, remaining height is shared between fill children. |
| `SIZE_CONTENT` | Use measured enabled-child content and padding for both width and height. |
| `SIZE_CONTENT_FILL` | Fill the available parent width while retaining measured content height. |

The content presets are defined in `include/ui/ui.h`:

```c
#define UI_SIZE_CONTENT ((Size){{0.0f, 0.0f}, SIZE_CONTENT})
#define UI_SIZE_CONTENT_FILL ((Size){{0.0f, 0.0f}, SIZE_CONTENT_FILL})
```

For `SIZE_CONTENT` and `SIZE_CONTENT_FILL`, changing the `{0.0f, 0.0f}` placeholder must not change layout. The layout helpers in `src/ui/ui.c` resolve content modes from `measured_content_size` during stacked, inline, wrapped, and final box layout.

Content measurement currently follows the UI tree: it measures enabled children and the element's padding. A leaf label or textbox does not automatically become text-metric-sized merely by using `SIZE_CONTENT`; give leaf controls an explicit size or place them inside a measured container.

## UI Spacing and Offsets

`Spacing` combines a spacing vector, a number form (`NONE` or `PERCENT`), and a layout type. Active layout types include stacked, inline, stacked-wrap, inline-wrap, and normal/manual distribution.

`SPACING_NONE` does not distribute children. Its vector and number form are irrelevant, so use the shared preset:

```c
#define UI_SPACING_NONE ((Spacing){{0.0f, 0.0f}, NONE, SPACING_NONE})
```

The `Offset` mode still matters even for a zero offset. `OFFSET_FIXED` and `OFFSET_PERCENT` affect how positions are interpreted during layout, and dragging converts percentage offsets to fixed offsets before applying movement. Do not replace all zero offsets with one universal preset without considering that behaviour.

`SPACING_OVERLAYED` and `ALIGNED_CENTRE` are declared in the public types, but they do not currently have an active layout implementation. Treat them as reserved until their resolver behaviour is added.

## State and Ownership

The UI and world systems use global state, including `G_UIState`, the universe, panel instances, and shared frame counters. Selection should go through the central UI selection setters so the selected entity ID, pointer, cell, and callback remain consistent.

The project uses manual memory management. UI elements are allocated from a pool when possible, while collections may own separate internal buffers. Read [OWNERSHIP.md](../OWNERSHIP.md) before changing allocation or collection lifetimes, especially for embedded collections and shallow disposal functions.

New allocation and copy operations in module code should use the wrappers in `memory/cmemory.h` where an equivalent wrapper exists.

## Working Conventions

Follow [CONVENTIONS.md](../CONVENTIONS.md) for C naming, indentation, initialisation, spacing, and file naming. Keep public declarations in the appropriate `include/` module header and implementations in the matching `src/` area.

Prefer existing subsystem helpers over ad-hoc state or duplicated UI construction. Keep generated build output under `build/` and do not add generated CMake files to source directories.

## Existing Documentation

- [README.md](../README.md) contains the original raylib-template setup notes and still has application placeholders.
- [CONVENTIONS.md](../CONVENTIONS.md) contains the repository's C and file naming conventions.
- [OWNERSHIP.md](../OWNERSHIP.md) documents manual memory and collection ownership rules.
- [PERFORMANCE_TECHNICAL_ANALYSIS.md](PERFORMANCE_TECHNICAL_ANALYSIS.md) records the performance optimisation analysis and reported results.
- [AGENTS.md](../AGENTS.md) contains repository-specific instructions for automated coding work.
