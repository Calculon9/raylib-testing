#include "system/lpanel_system.h"

#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"
#include "system/ui_system.h"
#include "system/panel_ui_helpers.h"
#include "system/str_helpers.h"
#include "world/world.h"
#include "system/systems.h"

// Small static action codes to avoid heap allocations for immutable button tags
static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;
static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;
static int btn_action_enumerate = 0;

// Static storage for a couple of small, long-lived Views to avoid heap allocs
static View lpanel_state_view_storage = {0};
static View lpanel_edit_entity_view_storage = {0};

// ----------LEFT PANEL SCREEN----------
// Visual Properties
static ColourRgba lpanel_fill_colour = COLOUR_PANEL_DARK_1;
static Vector2d lpanel_default_padding = {0.1, 0.1};
static Vector2d lpanel_tfield_padding = {0.03f, 0.03f};
static Size lpanel_title_tfield_size = {{6.0f, 0.45f}, SIZE_FIXED};
static Size lpanel_row_tfield_size = {{6.0f, 0.5f}, SIZE_FIXED};

// Coordinate Space Properties
static Space2d lpanel_space = {0};

UIBox seed_box = {0};
UIElement *lpanel_root = {0};
View *lpanel_state_view = {0};
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_btn_toggle_view_cont = {0};
UIElement *lpanel_btn_create_entity_cont = {0};
UIElement *lpanel_btn_delete_entity_cont = {0};
UIElement *lpanel_state_stats_tcont = {0};
UIElement *lpanel_state_entity_tcont = {0};
UIElement *lpanel_state_cell_tcont = {0};
View *lpanel_edit_entity_view = {0};
UIElement *lpanel_edit_entity_tcont = {0};

LArray lpanel_views = {0};

// ----Root Layout----
Offset lpanel_btn_toggle_view_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_create_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_btn_delete_entity_cont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_edit_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_edit_entity_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_view_cont_offset = {{0, 0.05}, OFFSET_PERCENT};
Offset lpanel_state_cell_tcont_offset = {{0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_stats_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};
Offset lpanel_state_entity_tcont_offset = {{0.0, 0.0}, OFFSET_PERCENT};

Spacing lpanel_root_child_spacing = {{0, 0.0}, PERCENT, SPACING_NORMAL};

Size lpanel_root_size = {{1, 1}, SIZE_PERCENT};
Size lpanel_btn_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
Size lpanel_state_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Size lpanel_state_stats_tcont_size = {{1.0f, 0.20f}, SIZE_PERCENT};
Size lpanel_state_entity_tcont_size = {{1.0f, 0.36f}, SIZE_PERCENT};
Size lpanel_state_cell_tcont_size = {{1.0f, 0.20f}, SIZE_PERCENT};
Size lpanel_edit_view_cont_size = {{1.0f, 0.92f}, SIZE_PERCENT};
Size lpanel_edit_entity_tcont_size = {{1.0f, 1.0f}, SIZE_PERCENT};

typedef struct LPanelElementFieldSpec
{
    const char *label;
    UIElementType type;
    DataType data_type;
    UIElement **target;
} LPanelElementFieldSpec;

typedef struct LPanelStringFieldSpec
{
    const char *label;
    String64 **target;
} LPanelStringFieldSpec;

static UIElement *CreateLPanelField(UIElement *parent, const char *label, UIElementType type)
{
    return CreatePanelLabeledFieldDefault(parent,
                                          label,
                                          type,
                                          lpanel_row_tfield_size,
                                          lpanel_tfield_padding,
                                          WHITE_RGBA,
                                          COLOURLESS_RGBA);
}

static void InitLPanelElementFields(UIElement *parent, const LPanelElementFieldSpec *specs, size_t count)
{
    if (!parent || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        UIElement *input_child = CreateLPanelField(parent, specs[i].label, specs[i].type);
        if (!input_child)
        {
            continue;
        }

        input_child->type = specs[i].type;
        input_child->data.textbox.data_type = specs[i].data_type;

        if (specs[i].target)
        {
            *specs[i].target = input_child;
        }
    }
}

static void InitLPanelStringFields(UIElement *parent, const LPanelStringFieldSpec *specs, size_t count)
{
    if (!parent || !specs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        UIElement *input_child = CreateLPanelField(parent, specs[i].label, UI_ELEMENT_TEXTBOX_O);
        if (!input_child)
        {
            continue;
        }

        input_child->type = UI_ELEMENT_TEXTBOX_O;

        if (specs[i].target)
        {
            *specs[i].target = &input_child->data.textbox.text;
        }
    }
}

void InitPanelRoot(void);
void InitPanelStateView(void);
void InitPanelEditView(void);
void InitPanelToggleButtons(void);
void InitStatsContainer(void);
void InitCellStateContainer(void);
void InitEntityStateContainer(void);
void InitEntityEditorContainer(void);

void InitPanel()
{
    lpanel_views = MakeLArray(3, sizeof(UIElement *));
    lpanel_state_view = &lpanel_state_view_storage;
    lpanel_edit_entity_view = &lpanel_edit_entity_view_storage;
    InitPanelRoot();
    InitPanelToggleButtons();
    InitPanelStateView();
    InitPanelEditView();
}

void InitPanelStateView(void)
{
    lpanel_state_view_cont = CreatePanelContainer(lpanel_root,
                                                  lpanel_state_view_cont_size,
                                                  lpanel_state_view_cont_offset,
                                                  ZERO_VECTOR_2D,
                                                  COLOURLESS_RGBA,
                                                  COLOURLESS_RGBA,
                                                  cont_default_child_spacing,
                                                  false,
                                                  true);
    lpanel_state_view->container = lpanel_state_view_cont;
    lpanel_state_view->type = LPANEL_STATE_VIEW;
    InitStatsContainer();
    InitEntityStateContainer();
    InitCellStateContainer();

    LArray_Push(&lpanel_views, &lpanel_state_view);
}

void InitPanelEditView(void)
{
    lpanel_edit_view_cont = CreatePanelContainer(lpanel_root,
                                                 lpanel_edit_view_cont_size,
                                                 lpanel_edit_view_cont_offset,
                                                 ZERO_VECTOR_2D,
                                                 COLOURLESS_RGBA,
                                                 COLOURLESS_RGBA,
                                                 cont_default_child_spacing,
                                                 false,
                                                 false);
    lpanel_edit_entity_view->container = lpanel_edit_view_cont;
    lpanel_edit_entity_view->type = LPANEL_EDIT_ENTITY_VIEW;
    InitEntityEditorContainer();

    LArray_Push(&lpanel_views, &lpanel_edit_entity_view);
}

void InitPanelRoot(void)
{
    Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
    camera_lpanel = CreateCamera2d(lpanel_pixel_basis, lpanel_basis, lpanel_pixel_origin, lpanel_origin);

    lpanel_space = NewSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);
    lpanel_root = CreateUIElement(UI_ELEMENT_ROOT, lpanel_root_size, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, lpanel_default_padding, COLOURLESS_RGBA, lpanel_fill_colour);
    lpanel_root->data.root.space = lpanel_space;
    lpanel_root->child_spacing = lpanel_root_child_spacing;

    Vector2d basis_scale = BasisTransform_2d_Scale(camera_lpanel.source_basis, camera_lpanel.destination_basis);
    seed_box.coords = (Vector2d){lpanel_pixel_origin.x, lpanel_pixel_origin.y};
    seed_box.dimensions = (Vector2d){lpanel_resolution.x * basis_scale.x, lpanel_resolution.y * basis_scale.y};
}

void InitPanelToggleButtons(void)
{
    lpanel_btn_toggle_view_cont = CreatePanelContainer(lpanel_root,
                                                       lpanel_btn_cont_size,
                                                       lpanel_btn_toggle_view_cont_offset,
                                                       ZERO_VECTOR_2D,
                                                       COLOURLESS_RGBA,
                                                       COLOURLESS_RGBA,
                                                       btn_cont_default_child_spacing,
                                                       false,
                                                       true);
    btn_action_enumerate = 0;
    CreatePanelButtonDefault(lpanel_btn_toggle_view_cont,
                             UI_ELEMENT_BUTTON_ENUMERATE,
                             "STATE -- UTIL",
                             btn_default_size,
                             btn_default_padding,
                             HandleBtnEnumerateClick,
                             &btn_action_enumerate,
                             &lpanel_views);
}

void InitEntityStateContainer(void)
{
    lpanel_state_entity_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                     lpanel_state_entity_tcont_size,
                                                     lpanel_state_entity_tcont_offset,
                                                     tcont_default_padding,
                                                     tcont_default_colour_border,
                                                     tcont_default_colour_fill,
                                                     tcont_default_child_spacing,
                                                     true,
                                                     true);

    CreatePanelTitleLabelDefault(lpanel_state_entity_tcont, "OBJECT PROPERTIES", lpanel_title_tfield_size, lpanel_tfield_padding);

    const LPanelElementFieldSpec state_specs[] = {
        {"ID", UI_ELEMENT_TEXTBOX_O, FLOAT, &G_UIState.lpanel_entity_state_id_tbox},
        {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, FLOAT, &G_UIState.lpanel_entity_state_mass_tbox},
        {"POS.TL", UI_ELEMENT_TEXTBOX_O, VECTOR2D, &G_UIState.lpanel_entity_state_pos_tl_tbox},
        {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_state_pos_c_tbox},
        {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_state_vel_tbox},
        {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_state_accel_tbox},
        {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_state_moment_tbox},
    };
    InitLPanelElementFields(lpanel_state_entity_tcont, state_specs, sizeof(state_specs) / sizeof(state_specs[0]));

    lpanel_btn_delete_entity_cont = CreatePanelContainer(lpanel_state_entity_tcont,
                                                         lpanel_btn_cont_size,
                                                         lpanel_btn_delete_entity_cont_offset,
                                                         ZERO_VECTOR_2D,
                                                         COLOURLESS_RGBA,
                                                         COLOURLESS_RGBA,
                                                         btn_cont_default_child_spacing,
                                                         false,
                                                         true);

    CreatePanelButtonDefault(lpanel_btn_delete_entity_cont,
                             UI_ELEMENT_BUTTON_SUBMIT,
                             "DELETE",
                             btn_default_size,
                             btn_default_padding,
                             HandleBtnSubmitClick,
                             &btn_action_delete_entity,
                             NULL);
}

void InitCellStateContainer(void)
{
    lpanel_state_cell_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                   lpanel_state_cell_tcont_size,
                                                   lpanel_state_cell_tcont_offset,
                                                   tcont_default_padding,
                                                   tcont_default_colour_border,
                                                   tcont_default_colour_fill,
                                                   tcont_default_child_spacing,
                                                   true,
                                                   true);

    CreatePanelTitleLabelDefault(lpanel_state_cell_tcont, "CELL STATE", lpanel_title_tfield_size, lpanel_tfield_padding);

    const LPanelStringFieldSpec cell_specs[] = {
        {"INDEX", &G_UIState.lpanel_cell_state_id_str},
        {"OCCU", &G_UIState.lpanel_cell_state_occu_str},
        {"VALUE", &G_UIState.lpanel_cell_state_value_str},
        {"FILL", &G_UIState.lpanel_cell_state_fill_str},
    };
    InitLPanelStringFields(lpanel_state_cell_tcont, cell_specs, sizeof(cell_specs) / sizeof(cell_specs[0]));
}

void InitStatsContainer(void)
{
    lpanel_state_stats_tcont = CreatePanelContainer(lpanel_state_view->container,
                                                    lpanel_state_stats_tcont_size,
                                                    lpanel_state_stats_tcont_offset,
                                                    tcont_default_padding,
                                                    tcont_default_colour_border,
                                                    tcont_default_colour_fill,
                                                    tcont_default_child_spacing,
                                                    true,
                                                    true);

    CreatePanelTitleLabelDefault(lpanel_state_stats_tcont, "STATISTICS", lpanel_title_tfield_size, lpanel_tfield_padding);

    const LPanelStringFieldSpec stats_specs[] = {
        {"POLYOIDS", &G_UIState.lpanel_stats_polygs_str},
        {"MEM", &G_UIState.lpanel_stats_mem_str},
        {"FPS", &G_UIState.lpanel_stats_fps_str},
        {"F.TIME", &G_UIState.lpanel_stats_ftime_str},
    };
    InitLPanelStringFields(lpanel_state_stats_tcont, stats_specs, sizeof(stats_specs) / sizeof(stats_specs[0]));
}

void InitEntityEditorContainer(void)
{
    lpanel_edit_entity_tcont = CreatePanelContainer(lpanel_edit_view_cont,
                                                    lpanel_edit_entity_tcont_size,
                                                    lpanel_edit_entity_tcont_offset,
                                                    tcont_default_padding,
                                                    tcont_default_colour_border,
                                                    tcont_default_colour_fill,
                                                    tcont_default_child_spacing,
                                                    true,
                                                    true);

    CreatePanelTitleLabelDefault(lpanel_edit_entity_view->container, "ENTITY EDIT", lpanel_title_tfield_size, lpanel_tfield_padding);

    const LPanelElementFieldSpec edit_specs[] = {
        {"VERT.CNT", UI_ELEMENT_TEXTBOX_SAFE_IO, INT, &G_UIState.lpanel_entity_edit_vertice_count_tbox},
        {"WIDTH", UI_ELEMENT_TEXTBOX_SAFE_IO, INT, &G_UIState.lpanel_entity_edit_width_tbox},
        {"HEIGHT", UI_ELEMENT_TEXTBOX_SAFE_IO, FLOAT, &G_UIState.lpanel_entity_edit_height_tbox},
        {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, FLOAT, &G_UIState.lpanel_entity_edit_mass_tbox},
        {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, FLOAT, &G_UIState.lpanel_entity_edit_pos_c_tbox},
        {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_edit_vel_tbox},
        {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_edit_accel_tbox},
        {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, VECTOR2D, &G_UIState.lpanel_entity_edit_moment_tbox},
    };
    InitLPanelElementFields(lpanel_edit_entity_tcont, edit_specs, sizeof(edit_specs) / sizeof(edit_specs[0]));

    lpanel_btn_create_entity_cont = CreatePanelContainer(lpanel_edit_entity_tcont,
                                                         lpanel_btn_cont_size,
                                                         lpanel_btn_create_entity_cont_offset,
                                                         ZERO_VECTOR_2D,
                                                         COLOURLESS_RGBA,
                                                         COLOURLESS_RGBA,
                                                         btn_cont_default_child_spacing,
                                                         false,
                                                         true);

    CreatePanelButtonDefault(lpanel_btn_create_entity_cont,
                             UI_ELEMENT_BUTTON_SUBMIT,
                             "CREATE",
                             btn_default_size,
                             btn_default_padding,
                             HandleBtnSubmitClick,
                             &btn_action_create_entity,
                             NULL);
}

void InitLPanel(void)
{
    InitPanel();
}

void UpdateLPanel(int mouse_x, int mouse_y)
{
    (void)mouse_x;
    (void)mouse_y;
}

void DrawLPanel(void)
{
    DrawRootUIElement(lpanel_root, seed_box, camera_lpanel);
}


