/**********************************************************************************************
*
*   VIEW HOST SYSTEM - Generic multi-view UI host infrastructure
*
*   Provides reusable management for a host that owns multiple Views and exposes the active one
*   (selected via a ViewSelector). Used by the left/right panels, the utility panel, the popup
*   menu, and the state manager. Eliminates ~400 lines of duplicated code per host.
*
**********************************************************************************************/
#ifndef VIEW_HOST_SYSTEM_H
#define VIEW_HOST_SYSTEM_H

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
void ViewHostSystem_HandleViewSelected(View *view);

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct ViewHostSystem {
    // Core UI Elements
    UIElement *root;
    UIBox seed_box;
    
    // Coordinate Space
    UISpace2d space;
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
    LArray selectors; // Array of ViewSelector*
} ViewHostSystem;

struct ViewSelector {
    ViewHostSystem *panel;
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
ViewHostSystem* ViewHostSystem_Create(ViewportRegion *viewport, float scale, Vector2d padding,
                                const UIPalette *palette, Spacing root_child_spacing);

// Destroy a panel system instance and everything it owns.
void ViewHostSystem_Destroy(ViewHostSystem *panel);

// Initialize the panel's root UI element and coordinate space
void ViewHostSystem_InitRoot(ViewHostSystem *panel);

// Initialize the views array
void ViewHostSystem_InitViews(ViewHostSystem *panel, size_t view_count);

// Create a View struct with its container UIElement.
// Independent of any panel; can be used standalone or registered with a panel later.
View *View_Create(UIElement *parent, const UIPalette *palette, ViewType type);

// Create a standard view container, allocate its View, register it, and return the panel-owned View.
View *ViewHostSystem_CreateView(ViewHostSystem *panel, ViewType view_type);

// Configure vertical scrollability for a view.
void View_SetScrollableY(View *view, bool is_scrollable);

// Configure horizontal scrollability for a view.
void View_SetScrollableX(View *view, bool is_scrollable);

// Set the horizontal scroll offset directly, clamped to [0, max_scroll_x].
void View_SetScrollX(View *view, float scroll_x);

// Scroll the view horizontally by a delta in local units, clamped to [0, max_scroll_x].
void View_ScrollX(View *view, float delta);

// Retrieve the current horizontal scroll offset.
float View_GetScrollX(const View *view);

// Set the vertical scroll offset directly, clamped to [0, max_scroll_y].
void View_SetScrollY(View *view, float scroll_y);

// Scroll the view vertically by a delta in local units, clamped to [0, max_scroll_y].
void View_ScrollY(View *view, float delta);

// Retrieve the current vertical scroll offset.
float View_GetScrollY(const View *view);

// Recompute content height, max scroll bounds, and clamp current scroll offset.
void View_UpdateScrollBounds(View *view);

// Retrieve the currently active view in a panel system (the first enabled view container).
View *ViewHostSystem_GetActiveView(ViewHostSystem *panel);

// Create buttons that select views in the panel's view array.
ViewSelector *ViewHostSystem_CreateViewSelector(ViewHostSystem *panel, UIElement *parent,
                                                  Size button_size, const char *labels[],
                                                  size_t count,
                                                  ViewSelectionCallback on_view_selected);

// Create hover items that select views when the cursor enters them.
ViewSelector *ViewHostSystem_CreateHoverViewSelector(ViewHostSystem *panel, UIElement *parent,
                                                       Size item_size, const char *labels[],
                                                       size_t count,
                                                       ViewSelectionCallback on_view_selected);

// Select a view and update the selector's active button styling.
bool ViewHostSystem_SelectView(ViewSelector *selector, size_t view_index);

// Draw the panel (call every frame)
void ViewHostSystem_Draw(ViewHostSystem *panel);

// Get the panel's coordinate space frame
Frame2d* ViewHostSystem_GetSpaceFrame(ViewHostSystem *panel);

// Set custom basis vectors for the panel space
bool ViewHostSystem_SetSpaceBasis(ViewHostSystem *panel, Vector2d u, Vector2d v);

// Reset basis to default
void ViewHostSystem_ResetSpaceBasis(ViewHostSystem *panel);

// Standard panel initialisation: create, init views array, init root, and
// optionally create + select a view-selector with the supplied labels.
ViewHostSystem *ViewHostSystem_CreateStandard(ViewportRegion *viewport, size_t view_count,
                                        const char *selector_labels[], size_t selector_label_count,
                                        ViewSelectionCallback selector_callback,
                                        const UIPalette *palette, Spacing root_child_spacing);

// Create a toggle bar container at the top of the panel and a view selector.
ViewSelector *ViewHostSystem_CreateStandardViewSelector(ViewHostSystem *panel,
                                                     const char *labels[], size_t count,
                                                     ViewSelectionCallback callback);

// View selector button click handler; wires button clicks to view selection.
void HandleViewHostSelectorClick(UIElement *button);

// Finalise panel initialisation: select first view and update UI space.
// Common routine that reduces boilerplate across lpanel, rpanel, utility panel systems.
void ViewHostSystem_FinaliseInit(ViewHostSystem *panel, ViewSelector **selector_out);
// Apply standard styling to a view container (border, fill, draggable).
void ViewHostSystem_StyleViewContainer(UIElement *container, const UIPalette *palette);
#endif // VIEW_HOST_SYSTEM_H
