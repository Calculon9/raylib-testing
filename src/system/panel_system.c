#include "system/panel_system.h"

#include <stdlib.h>
#include "math/affine_space_ops.h"
#include "ui/ui_renderer.h"
#include "system/viewport_system.h"

PanelSystem* PanelSystem_Create(ViewportRegion *viewport, float scale, Vector2d padding,
                                ColourRgba fill_colour, Spacing root_child_spacing)
{
    PanelSystem *panel = (PanelSystem*)malloc(sizeof(PanelSystem));
    if (!panel)
    {
        return NULL;
    }

    panel->root = NULL;
    panel->seed_box = (UIBox){0};
    panel->space = (Space2d){0};
    panel->viewport = viewport;
    panel->space_to_viewport_scale = scale;
    panel->default_padding = padding;
    panel->fill_colour = fill_colour;
    panel->root_child_spacing = root_child_spacing;
    panel->basis_override_enabled = false;
    panel->basis_override_u = ZERO_VECTOR_2D;
    panel->basis_override_v = ZERO_VECTOR_2D;
    panel->views = (LArray){0};

    return panel;
}

void PanelSystem_InitRoot(PanelSystem *panel)
{
    if (!panel || !panel->viewport)
    {
        return;
    }

    // Calculate resolution scaled to panel space
    Vector2d resolution = VectorScale_2d(panel->viewport->resolution, panel->space_to_viewport_scale);
    
    // Setup basis (default or override)
    Basis2d viewport_basis = (Basis2d){
        (Vector2d){1.0f / panel->space_to_viewport_scale, 0.0f},
        (Vector2d){0.0f, 1.0f / panel->space_to_viewport_scale}
    };
    
    if (panel->basis_override_enabled)
    {
        viewport_basis.u = panel->basis_override_u;
        viewport_basis.v = panel->basis_override_v;
    }

    // Initialize coordinate space
    panel->space = NewSpace2d(panel->viewport->local_origin, resolution, viewport_basis);

    // Create root UI element
    Size root_size = {{(float)panel->space.columns, (float)panel->space.rows}, SIZE_FILL};
    panel->root = CreateUIElement(UI_ELEMENT_ROOT, root_size,
                                  (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                  panel->default_padding, COLOURLESS_RGBA, panel->fill_colour);
    
    if (panel->root)
    {
        panel->root->data.root.space = panel->space;
        panel->root->child_spacing = panel->root_child_spacing;
    }

    // Setup seed box for rendering
    panel->seed_box.coords = ZERO_VECTOR_2D;
    panel->seed_box.dimensions = resolution;
}

void PanelSystem_InitViews(PanelSystem *panel, size_t view_count)
{
    if (!panel)
    {
        return;
    }

    panel->views = MakeLArray(view_count, sizeof(View*));
}

void PanelSystem_Draw(PanelSystem *panel)
{
    if (!panel || !panel->root)
    {
        return;
    }

    // Update UI layout to reflect any interactive changes
    UpdateUISpace(panel->root, panel->seed_box);

    // Apply panel-space basis transformation
    Frame2d panel_basis_frame = panel->space.frame;
    panel_basis_frame.origin_in_parent = ZERO_VECTOR_2D;
    Matrix3x3 panel_local_to_viewport = MtxTransform_GetLocalToParent(panel_basis_frame);
    Matrix3x3 panel_local_to_pixel = MatrixMultiply_3x3_3x3(panel->viewport->tunnel.source_to_dest_mtx,
                                                           panel_local_to_viewport);

    DrawRootUIElement(panel->root, panel->seed_box, panel_local_to_pixel);
}

Frame2d* PanelSystem_GetSpaceFrame(PanelSystem *panel)
{
    if (!panel)
    {
        return NULL;
    }

    return &panel->space.frame;
}

bool PanelSystem_SetSpaceBasis(PanelSystem *panel, Vector2d u, Vector2d v)
{
    if (!panel)
    {
        return false;
    }

    if (VectorMagnitude_2d(u) < 0.0001f || VectorMagnitude_2d(v) < 0.0001f)
    {
        return false;
    }

    panel->basis_override_enabled = true;
    panel->basis_override_u = u;
    panel->basis_override_v = v;
    return true;
}

void PanelSystem_ResetSpaceBasis(PanelSystem *panel)
{
    if (!panel)
    {
        return;
    }

    panel->basis_override_enabled = false;
    panel->basis_override_u = ZERO_VECTOR_2D;
    panel->basis_override_v = ZERO_VECTOR_2D;
}

void PanelSystem_Destroy(PanelSystem *panel)
{
    if (!panel)
    {
        return;
    }

    // Note: UI elements are owned by the UI system, not freed here
    // Views array memory is managed by the array itself
    // No cleanup needed for LArray - it's stack-allocated

    free(panel);
}
