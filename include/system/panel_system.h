/**********************************************************************************************
*
*   PANEL SYSTEM - Generic UI Panel Infrastructure
*
*   Provides reusable panel management for left/right panels
*   Eliminates ~400 lines of duplicated code per panel
*
**********************************************************************************************/
#ifndef PANEL_SYSTEM_H
#define PANEL_SYSTEM_H

#include <stddef.h>
#include "math/cvectors.h"
#include "math/coordinate_space.h"
#include "ui/ui.h"
#include "collections/linear_array.h"

typedef struct UIPalette UIPalette;

// Forward declarations
typedef struct ViewportRegion ViewportRegion;
typedef struct ViewSelector ViewSelector;

typedef void (*ViewSelectionCallback)(View *view);

// Shared callback that updates global active panel view from selected view type.
void PanelSystem_HandleViewSelected(View *view);

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct PanelSystem {
    // Core UI Elements
    UIElement *root;
    UIBox seed_box;
    
    // Coordinate Space
    Space2d space;
    ViewportRegion *viewport;
    
    // Configuration
    float space_to_viewport_scale;
    Vector2d default_padding;
    const UIPalette *palette;
    Spacing root_child_spacing;
    
    // Basis Override
    bool basis_override_enabled;
    Vector2d basis_override_u;
    Vector2d basis_override_v;
    
    // Views
    LArray views;  // Array of View*
} PanelSystem;

struct ViewSelector {
    PanelSystem *panel;
    UIElement **buttons;
    int *view_indices;
    size_t count;
    size_t active_index;
    ViewSelectionCallback on_view_selected;
};

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Create a new panel system instance
PanelSystem* PanelSystem_Create(ViewportRegion *viewport, float scale, Vector2d padding,
                                const UIPalette *palette, Spacing root_child_spacing);

// Initialize the panel's root UI element and coordinate space
void PanelSystem_InitRoot(PanelSystem *panel);

// Initialize the views array
void PanelSystem_InitViews(PanelSystem *panel, size_t view_count);

// Register an existing view storage object with its container and type.
bool PanelSystem_AddView(PanelSystem *panel, View *view, UIElement *container, ViewType type);

// Allocate a View, register it with the panel/container/type, and return it in one call.
View *PanelSystem_CreateView(PanelSystem *panel, UIElement *container, ViewType type);

// Create buttons that select views in the panel's view array.
ViewSelector *PanelSystem_CreateViewSelector(PanelSystem *panel, UIElement *parent,
                                                  Size button_size, const char *labels[],
                                                  size_t count,
                                                  ViewSelectionCallback on_view_selected);

// Create hover items that select views when the cursor enters them.
ViewSelector *PanelSystem_CreateHoverViewSelector(PanelSystem *panel, UIElement *parent,
                                                       Size item_size, const char *labels[],
                                                       size_t count,
                                                       ViewSelectionCallback on_view_selected);

// Select a view and update the selector's active button styling.
bool PanelSystem_SelectView(ViewSelector *selector, size_t view_index);

// Draw the panel (call every frame)
void PanelSystem_Draw(PanelSystem *panel);

// Get the panel's coordinate space frame
Frame2d* PanelSystem_GetSpaceFrame(PanelSystem *panel);

// Set custom basis vectors for the panel space
bool PanelSystem_SetSpaceBasis(PanelSystem *panel, Vector2d u, Vector2d v);

// Reset basis to default
void PanelSystem_ResetSpaceBasis(PanelSystem *panel);

// Standard panel initialisation: create, init views array, init root, and
// optionally create + select a view-selector with the supplied labels.
PanelSystem *PanelSystem_CreateStandard(ViewportRegion *viewport, size_t view_count,
                                        const char *selector_labels[], size_t selector_label_count,
                                        ViewSelectionCallback selector_callback,
                                        const UIPalette *palette, Spacing root_child_spacing);

// Create the panel's top-level view container (transparent fill, zero offset,
// standard stack spacing, not draggable, enabled).
UIElement *PanelSystem_CreateRootViewContainer(PanelSystem *panel, ViewType view_type, View *view_storage);

// Create a toggle bar container at the top of the panel and a view selector.
ViewSelector *PanelSystem_CreateStandardViewSelector(PanelSystem *panel,
                                                     const char *labels[], size_t count,
                                                     ViewSelectionCallback callback);

#endif // PANEL_SYSTEM_H
