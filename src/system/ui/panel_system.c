#include "system/panel_system.h"
#include "system/ui_system.h"
#include <stdlib.h>
#include "math/affine_space_ops.h"
#include "ui/ui_renderer.h"
#include "ui/ui_constructors.h"
#include "system/viewport_system.h"

void PanelSystem_HandleViewSelected(View *view)
{
    if (view)
    {
        G_UIState.active_panel_view = view->type;
    }
}

PanelSystem *PanelSystem_Create(ViewportRegion *viewport, float scale, Vector2d padding,
                                const UIPalette *palette, Spacing root_child_spacing)
{
    PanelSystem *panel = (PanelSystem *)AllocateBytes(sizeof(PanelSystem));
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
    panel->palette = palette ? palette : &ui_default_palette;
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
        (Vector2d){0.0f, 1.0f / panel->space_to_viewport_scale}};

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
                                  panel->default_padding, COLOURLESS_RGBA,
                                  panel->palette->panel_background);

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

    panel->views = MakeLArray(view_count, sizeof(View *));
}

bool PanelSystem_AddView(PanelSystem *panel, View *view, UIElement *container, ViewType type)
{
    if (!panel || !view || !container)
    {
        return false;
    }

    view->container = container;
    view->type = type;
    return LArray_Push(&panel->views, &view);
}

View *PanelSystem_CreateView(PanelSystem *panel, UIElement *container, ViewType type)
{
    View *view = AllocateBytes(sizeof(View));
    PanelSystem_AddView(panel, view, container, type);
    return view;
}

static void UpdatePanelViewSelectorButtons(ViewSelector *selector)
{
    for (size_t i = 0; i < selector->count; i++)
    {
        bool is_active = i == selector->active_index;
        selector->buttons[i]->colour_fill = is_active
                                                ? selector->panel->palette->button_fill
                                                : selector->panel->palette->panel_background;
        if (selector->buttons[i]->type == UI_ELEMENT_HOVER_ITEM)
        {
            selector->buttons[i]->data.hover_item.normal_fill = selector->buttons[i]->colour_fill;
            selector->buttons[i]->data.hover_item.font.colour = is_active
                                                                    ? selector->panel->palette->text_on_dark
                                                                    : selector->panel->palette->button_fill;
        }
        else
        {
            selector->buttons[i]->data.button.font.colour = is_active
                                                                ? selector->panel->palette->text_on_dark
                                                                : selector->panel->palette->button_fill;
        }
    }
}

static void HandlePanelViewSelectorClick(UIElement *button)
{
    ViewSelector *selector = (ViewSelector *)button->data.button.data_bind;
    if (!selector || !button->data.button.user_data)
    {
        return;
    }

    PanelSystem_SelectView(selector, (size_t)*((int *)button->data.button.user_data));
}

static void HandlePanelViewSelectorHover(UIElement *item)
{
    ViewSelector *selector = (ViewSelector *)item->data.hover_item.data_bind;
    if (!selector || !item->data.hover_item.user_data)
    {
        return;
    }

    PanelSystem_SelectView(selector, (size_t)*((int *)item->data.hover_item.user_data));
}

static ViewSelector *AllocatePanelViewSelector(PanelSystem *panel, const char *labels[], size_t count,
                                                ViewSelectionCallback on_view_selected)
{
    ViewSelector *selector = AllocateBytes(sizeof(ViewSelector));
    if (!selector)
    {
        return NULL;
    }

    selector->panel = panel;
    selector->buttons = AllocateBytes(sizeof(UIElement *) * count);
    selector->view_indices = AllocateBytes(sizeof(int) * count);
    if (!selector->buttons || !selector->view_indices)
    {
        free(selector->buttons);
        free(selector->view_indices);
        free(selector);
        return NULL;
    }

    selector->count = count;
    selector->active_index = count;
    selector->on_view_selected = on_view_selected;
    for (size_t i = 0; i < count; i++)
    {
        selector->view_indices[i] = (int)i;
    }

    return selector;
}

static bool SetPanelActiveView(PanelSystem *panel, size_t view_index)
{
    if (!panel || view_index >= panel->views.count)
    {
        return false;
    }

    for (size_t i = 0; i < panel->views.count; i++)
    {
        View *view = *((View **)LArray_Get(&panel->views, i));
        if (view && view->container)
        {
            if (i == view_index)
            {
                EnableElement(view->container);
            }
            else
            {
                DisableElement(view->container);
            }
        }
    }

    return true;
}

ViewSelector *PanelSystem_CreateViewSelector(PanelSystem *panel, UIElement *parent, Size button_size, const char *labels[],
                                             size_t count, ViewSelectionCallback on_view_selected)
{
    if (!panel || !parent || !labels || count == 0)
    {
        return NULL;
    }

    ViewSelector *selector = AllocatePanelViewSelector(panel, labels, count, on_view_selected);
    if (!selector)
    {
        return NULL;
    }

    for (size_t i = 0; i < count; i++)
    {
        selector->buttons[i] = CreateUIButtonDefault(
            parent, UI_ELEMENT_BUTTON_ENUMERATE, labels[i], button_size,
            ui_standard_button_padding, panel->palette, HandlePanelViewSelectorClick,
            &selector->view_indices[i], selector);
    }

    UpdatePanelViewSelectorButtons(selector);
    return selector;
}

ViewSelector *PanelSystem_CreateHoverViewSelector(PanelSystem *panel, UIElement *parent,
                                                  Size item_size, const char *labels[],
                                                  size_t count,
                                                  ViewSelectionCallback on_view_selected)
{
    if (!panel || !parent || !labels || count == 0)
    {
        return NULL;
    }

    ViewSelector *selector = AllocatePanelViewSelector(panel, labels, count, on_view_selected);
    if (!selector)
    {
        return NULL;
    }

    for (size_t i = 0; i < count; i++)
    {
        selector->buttons[i] = CreateUIHoverItemDefault(
            parent, labels[i], item_size, ui_standard_button_padding,
            panel->palette, HandlePanelViewSelectorHover,
            &selector->view_indices[i]);
        if (!selector->buttons[i])
        {
            return NULL;
        }

        selector->buttons[i]->data.hover_item.data_bind = selector;
    }

    UpdatePanelViewSelectorButtons(selector);
    return selector;
}

bool PanelSystem_SelectView(ViewSelector *selector, size_t view_index)
{
    if (!selector || view_index >= selector->count || !SetPanelActiveView(selector->panel, view_index))
    {
        return false;
    }

    selector->active_index = view_index;
    UpdatePanelViewSelectorButtons(selector);

    View *selected_view = *((View **)LArray_Get(&selector->panel->views, view_index));
    if (selector->on_view_selected)
    {
        selector->on_view_selected(selected_view);
    }

    return true;
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

Frame2d *PanelSystem_GetSpaceFrame(PanelSystem *panel)
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

PanelSystem *PanelSystem_CreateStandard(ViewportRegion *viewport, size_t view_count,
                                        const char *selector_labels[], size_t selector_label_count,
                                        ViewSelectionCallback selector_callback,
                                        const UIPalette *palette, Spacing root_child_spacing)
{
    PanelSystem *panel = PanelSystem_Create(viewport, 1.0f, (Vector2d){0.1f, 0.1f},
                                            palette, root_child_spacing);
    if (!panel)
    {
        return NULL;
    }

    PanelSystem_InitViews(panel, view_count);
    PanelSystem_InitRoot(panel);

    if (selector_labels && selector_label_count > 0)
    {
        ViewSelector *selector = PanelSystem_CreateStandardViewSelector(
            panel, selector_labels, selector_label_count, selector_callback);
        PanelSystem_SelectView(selector, 0);
    }

    return panel;
}

UIElement *PanelSystem_CreateRootViewContainer(PanelSystem *panel, ViewType view_type, View *view_storage)
{
    if (!panel || !view_storage)
    {
        return NULL;
    }

    UIElement *container = CreateUIContainer(
        panel->root, ui_fill_container_size,
        (Offset){ZERO_VECTOR_2D, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_standard_stack_spacing, false, true);

    PanelSystem_AddView(panel, view_storage, container, view_type);
    return container;
}

ViewSelector *PanelSystem_CreateStandardViewSelector(PanelSystem *panel,
                                                     const char *labels[], size_t count,
                                                     ViewSelectionCallback callback)
{
    if (!panel || !labels || count == 0)
    {
        return NULL;
    }

    UIElement *toggle_cont = CreateUIContainer(
        panel->root, ui_standard_selector_container_size,
        (Offset){{0.0f, 0.0f}, OFFSET_FIXED}, ZERO_VECTOR_2D,
        panel->palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_zero_inline_spacing, false, true);
    toggle_cont->colour_border = panel->palette->container_border;

    return PanelSystem_CreateViewSelector(
        panel, toggle_cont, ui_standard_selector_button_size,
        labels, count, callback);
}
