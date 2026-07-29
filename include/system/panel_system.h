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

// Forward declarations
typedef struct ViewportRegion ViewportRegion;

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
    ColourRgba fill_colour;
    Spacing root_child_spacing;
    
    // Basis Override
    bool basis_override_enabled;
    Vector2d basis_override_u;
    Vector2d basis_override_v;
    
    // Views
    LArray views;  // Array of View*
} PanelSystem;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Create a new panel system instance
PanelSystem* PanelSystem_Create(ViewportRegion *viewport, float scale, Vector2d padding,
                                ColourRgba fill_colour, Spacing root_child_spacing);

// Initialize the panel's root UI element and coordinate space
void PanelSystem_InitRoot(PanelSystem *panel);

// Initialize the views array
void PanelSystem_InitViews(PanelSystem *panel, size_t view_count);

// Draw the panel (call every frame)
void PanelSystem_Draw(PanelSystem *panel);

// Get the panel's coordinate space frame
Frame2d* PanelSystem_GetSpaceFrame(PanelSystem *panel);

// Set custom basis vectors for the panel space
bool PanelSystem_SetSpaceBasis(PanelSystem *panel, Vector2d u, Vector2d v);

// Reset basis to default
void PanelSystem_ResetSpaceBasis(PanelSystem *panel);

// Destroy panel system and free resources
void PanelSystem_Destroy(PanelSystem *panel);

#endif // PANEL_SYSTEM_H
