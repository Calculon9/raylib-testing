#include "system/rpanel_system.h"

#include <stdio.h>
#include "raylib.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/world_system.h"
#include "system/universe_system.h"
#include "world/universe.h"
#include "system/viewport_system.h"
#include "system/panel_ui_helpers.h"
#include "system/str_helpers.h"
#include "math/coordinate_space.h"
#include "world/world.h"
#include "ui/text_region.h"
#include "ui/ui_renderer.h"

UIElement *rpanel_root = NULL;
Camera2d camera_rpanel = {0};
UIBox rpanel_seed_box = {0};

static Space2d rpanel_space = {0};
static int rpanel_resolution_x = 0;
static int rpanel_resolution_y = 0;
static float rpanel_grid_cell_size = 1.0f;
static View rpanel_state_view_storage = {0};
static View rpanel_create_view_storage = {0};
static View *rpanel_state_view = NULL;
static View *rpanel_create_view = NULL;
static UIElement *rpanel_toggle_cont = NULL;
static UIElement *rpanel_state_view_cont = NULL;
static UIElement *rpanel_create_view_cont = NULL;
static LArray rpanel_views = {0};
static int btn_action_rpanel_enumerate = 0;

static int btn_action_create_world = BUTTON_ACTION_CREATE_WORLD;
static int btn_action_select_world_prev = BUTTON_ACTION_SELECT_WORLD_PREV;
static int btn_action_select_world_next = BUTTON_ACTION_SELECT_WORLD_NEXT;

static UIElement *rpanel_stats_fps_tbox = NULL;
static UIElement *rpanel_stats_frame_tbox = NULL;
static UIElement *rpanel_stats_entities_tbox = NULL;
static UIElement *rpanel_world_index_tbox = NULL;
static UIElement *rpanel_world_universe_pos_tbox = NULL;
static UIElement *rpanel_world_gravity_edit_tbox = NULL;
static UIElement *rpanel_world_resolution_tbox = NULL;
static UIElement *rpanel_world_objects_tbox = NULL;
static UIElement *rpanel_world_next_id_tbox = NULL;
static UIElement *rpanel_create_world_count_tbox = NULL;
static UIElement *rpanel_create_selected_world_tbox = NULL;
static UIElement *rpanel_create_spawn_tbox = NULL;
static UIElement *rpanel_create_resolution_tbox = NULL;
static UIElement *rpanel_create_basis_u_tbox = NULL;
static UIElement *rpanel_create_basis_v_tbox = NULL;
static UIElement *rpanel_create_gravity_tbox = NULL;
static UIElement *rpanel_create_auto_select_tbox = NULL;

static Vector2d rpanel_default_padding = {0.1f, 0.1f};
static Vector2d rpanel_tfield_padding = {0.03f, 0.03f};
static Size rpanel_title_tfield_size = {{5.8f, 0.45f}, SIZE_FIXED};
static Size rpanel_row_tfield_size = {{5.8f, 0.5f}, SIZE_FIXED};
static Size rpanel_stat_row_tfield_size = {{5.8f, 0.55f}, SIZE_FIXED};
static Size rpanel_button_size = {{5.8f, 0.45f}, SIZE_FIXED};
static Spacing rpanel_world_btn_child_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};
static bool rpanel_space_basis_override_enabled = false;
static Vector2d rpanel_space_basis_override_u = {0};
static Vector2d rpanel_space_basis_override_v = {0};

static void CreateRPanelSubmitButton(UIElement *parent, const char *label, int *action)
{
    CreatePanelButtonDefault(parent, UI_ELEMENT_BUTTON_SUBMIT, label, rpanel_button_size,
                             (Vector2d){0.02f, 0.02f}, HandleBtnSubmitClick, action, NULL);
}

// Suspect I might be creating the wrong basis going from panel_viewport->screen. Might be too small?
void InitRPanel(void)
{
    float rpanel_space_to_viewport_scale = 2.0f; // Scale factor to convert from panel space to viewport space
    Vector2d rpanel_resolution = VectorScale_2d(rpanel_viewport_resolution, rpanel_space_to_viewport_scale);
    Basis2d rpanel_viewport_basis = (Basis2d){(Vector2d){1.0f / rpanel_space_to_viewport_scale, 0.0f}, (Vector2d){0.0f, 1.0f / rpanel_space_to_viewport_scale}};
    if (rpanel_space_basis_override_enabled)
    {
        rpanel_viewport_basis.u = rpanel_space_basis_override_u;
        rpanel_viewport_basis.v = rpanel_space_basis_override_v;
    }

    // Establish universe_frame with centered spatial alignment
    // =========================================================================
    rpanel_space = NewSpace2d(rpanel_viewport_local_origin, rpanel_resolution, rpanel_viewport_basis);

    camera_rpanel = CreateCamera2d(&rpanel_space.frame, &rpanel_viewport_frame);

    if (camera_rpanel.zoom <= 0.0f)
    {
        camera_rpanel = CreateCamera2d(&rpanel_space.frame, &rpanel_viewport_frame);
    }
    else
    {
        Camera_SetSourceFrame(&camera_rpanel, &rpanel_space.frame);
        Camera_SetDestinationFrame(&camera_rpanel, &rpanel_viewport_frame);
    }

    rpanel_root = CreateUIElement(UI_ELEMENT_ROOT,
                                  (Size){{(float)rpanel_space.columns, (float)rpanel_space.rows}, SIZE_FILL},
                                  (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                  rpanel_default_padding, COLOURLESS_RGBA, COLOUR_PANEL_DARK_1);

    rpanel_root->data.root.space = rpanel_space;
    rpanel_root->child_spacing = (Spacing){{0.0f, 0.0f}, PERCENT, SPACING_NORMAL};

    rpanel_views = MakeLArray(2, sizeof(View *));
    rpanel_state_view = &rpanel_state_view_storage;
    rpanel_create_view = &rpanel_create_view_storage;
    btn_action_rpanel_enumerate = 0;

    // Check here for a wildly incorrect basis scale that would cause the panel to be drawn off-screen or at an unexpected size.
    // Vector2d basis_scale = Camera_GetBasisScale(&camera_rpanel);
    rpanel_seed_box.coords = ZERO_VECTOR_2D;// lpanel_viewport_local_origin;
    // Its dimensions are the pure unscaled logical resolution units.
    rpanel_seed_box.dimensions = rpanel_resolution;
    // Basis2d basis_a = camera_rpanel.tunnel.source_frame->basis;
    // Basis2d basis_b = camera_rpanel.tunnel.destination_frame->basis;
    // Matrix2x2 basis_matrix = MatrixMultiply_2x2_2x2((Matrix2x2){basis_a.u, basis_a.v}, (Matrix2x2){basis_b.u, basis_b.v});
    // Vector2d basis_tfrm = VectorSum_2d(basis_matrix.col1, basis_matrix.col2);
    // rpanel_seed_box.coords = (Vector2d){rpanel_pixel_origin.x, rpanel_pixel_origin.y};
    // rpanel_seed_box.dimensions = (Vector2d){rpanel_viewport_resolution.x * basis_tfrm.x, rpanel_viewport_resolution.y * basis_tfrm.y};

    rpanel_toggle_cont = CreatePanelContainer(
        rpanel_root, (Size){{1.0f, 0.08f}, SIZE_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        tcont_default_child_spacing, false, true);
    CreatePanelButtonDefault(rpanel_toggle_cont, UI_ELEMENT_BUTTON_ENUMERATE,
                             "STATE -- CREATE", rpanel_button_size,
                             (Vector2d){0.02f, 0.02f}, HandleBtnEnumerateClick,
                             &btn_action_rpanel_enumerate, &rpanel_views);

    rpanel_state_view_cont = CreatePanelContainer(
        rpanel_root, (Size){{1.0f, 0.92f}, SIZE_PERCENT},
        (Offset){{0.0f, 0.08f}, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        cont_default_child_spacing, false, true);

    rpanel_create_view_cont = CreatePanelContainer(
        rpanel_root, (Size){{1.0f, 0.92f}, SIZE_PERCENT},
        (Offset){{0.0f, 0.08f}, OFFSET_PERCENT}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        tcont_default_child_spacing, false, false);

    rpanel_state_view->container = rpanel_state_view_cont;
    rpanel_state_view->type = RPANEL_STATE_VIEW;
    rpanel_create_view->container = rpanel_create_view_cont;
    rpanel_create_view->type = RPANEL_WORLD_CREATE_VIEW;
    LArray_Push(&rpanel_views, &rpanel_state_view);
    LArray_Push(&rpanel_views, &rpanel_create_view);

    UIElement *world_cont = CreatePanelContainer(
        rpanel_state_view_cont, (Size){{1.0f, 0.5f}, SIZE_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(world_cont, "WORLD MANAGER", rpanel_title_tfield_size, rpanel_tfield_padding);

    UIElement *world_btn_cont = CreatePanelContainer(
        world_cont, (Size){{1.0f, 0.22f}, SIZE_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, ZERO_VECTOR_2D,
        COLOURLESS_RGBA, COLOURLESS_RGBA,
        rpanel_world_btn_child_spacing, false, true);

    CreateRPanelSubmitButton(world_btn_cont, "SELECT PREV", &btn_action_select_world_prev);
    CreateRPanelSubmitButton(world_btn_cont, "SELECT NEXT", &btn_action_select_world_next);

    const PanelFieldSpec world_fields[] = {
        {"WORLD", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_world_index_tbox, NULL},
        {"UNIVERSE", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_world_universe_pos_tbox, NULL},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, FLOAT, &rpanel_world_gravity_edit_tbox, NULL},
        {"RES", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_world_resolution_tbox, NULL},
        {"OBJECTS", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_world_objects_tbox, NULL},
        {"NEXT ID", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_world_next_id_tbox, NULL},
    };
    InitPanelFields(world_cont, world_fields,
                    sizeof(world_fields) / sizeof(world_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    if (rpanel_world_gravity_edit_tbox)
    {
        rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
    }

    UIElement *stats_cont = CreatePanelContainer(
        rpanel_state_view_cont, (Size){{1.0f, 0.32f}, SIZE_PERCENT},
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED}, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(stats_cont, "UTILITY PANEL", rpanel_title_tfield_size, rpanel_tfield_padding);
    const PanelFieldSpec stat_fields[] = {
        {"FPS", UI_ELEMENT_TEXTBOX_O, rpanel_stat_row_tfield_size, FLOAT, &rpanel_stats_fps_tbox, NULL},
        {"FRAME (MS)", UI_ELEMENT_TEXTBOX_O, rpanel_stat_row_tfield_size, FLOAT, &rpanel_stats_frame_tbox, NULL},
        {"ENTITIES", UI_ELEMENT_TEXTBOX_O, rpanel_stat_row_tfield_size, FLOAT, &rpanel_stats_entities_tbox, NULL},
    };
    InitPanelFields(stats_cont, stat_fields,
                    sizeof(stat_fields) / sizeof(stat_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    UIElement *create_world_cont = CreatePanelContainer(
        rpanel_create_view_cont, (Size){{0.96f, 0.78f}, SIZE_PERCENT},
        (Offset){{0.02f, 0.02f}, OFFSET_PERCENT}, tcont_default_padding,
        tcont_default_colour_border, tcont_default_colour_fill,
        tcont_default_child_spacing, true, true);

    CreatePanelTitleLabelDefault(create_world_cont, "WORLD CREATION", rpanel_title_tfield_size, rpanel_tfield_padding);
    const PanelFieldSpec create_fields[] = {
        {"WORLDS", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_create_world_count_tbox, NULL},
        {"SELECTED", UI_ELEMENT_TEXTBOX_O, rpanel_row_tfield_size, FLOAT, &rpanel_create_selected_world_tbox, NULL},
        {"SPAWN", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, VECTOR2D, &rpanel_create_spawn_tbox, NULL},
        {"RESOLUTION", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, VECTOR2D, &rpanel_create_resolution_tbox, NULL},
        {"BASIS U", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, VECTOR2D, &rpanel_create_basis_u_tbox, NULL},
        {"BASIS V", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, VECTOR2D, &rpanel_create_basis_v_tbox, NULL},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, FLOAT, &rpanel_create_gravity_tbox, NULL},
        {"AUTO SELECT", UI_ELEMENT_TEXTBOX_SAFE_IO, rpanel_row_tfield_size, INT, &rpanel_create_auto_select_tbox, NULL},
    };
    InitPanelFields(create_world_cont, create_fields,
                    sizeof(create_fields) / sizeof(create_fields[0]), rpanel_tfield_padding,
                    WHITE_RGBA, COLOURLESS_RGBA);

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    BindTextboxData(rpanel_create_spawn_tbox, VECTOR2D, spawn_origin);

    Vector2d *next_res = GetNextWorldResolutionPtr();
    BindTextboxData(rpanel_create_resolution_tbox, VECTOR2D, next_res);

    Vector2d *basis_u = GetNextWorldBasisUPtr();
    BindTextboxData(rpanel_create_basis_u_tbox, VECTOR2D, basis_u);

    Vector2d *basis_v = GetNextWorldBasisVPtr();
    BindTextboxData(rpanel_create_basis_v_tbox, VECTOR2D, basis_v);

    float *next_grav = GetNextWorldGravityPtr();
    BindTextboxData(rpanel_create_gravity_tbox, FLOAT, next_grav);

    int *auto_select = GetCreateWorldAutoSelectPtr();
    BindTextboxData(rpanel_create_auto_select_tbox, INT, auto_select);

    CreateRPanelSubmitButton(create_world_cont, "NEW WORLD", &btn_action_create_world);
}

void UpdateRPanel(int mouse_x, int mouse_y)
{
    (void)mouse_x;
    (void)mouse_y;
}

void DrawRPanel(void)
{
    if (!rpanel_root)
    {
        return;
    }

    if (rpanel_stats_fps_tbox)
    {
        WriteTextboxFloat(rpanel_stats_fps_tbox, frame_counter.fps, 1);
    }
    if (rpanel_stats_frame_tbox)
    {
        WriteTextboxFloat(rpanel_stats_frame_tbox, frame_counter.delta_time * 1000.0f, 2);
    }
    if (rpanel_stats_entities_tbox)
    {
        WriteTextboxInt(rpanel_stats_entities_tbox, GetNewtonoidCount());
    }

    World2d *selected_world = GetSelectedWorld();
    int selected_world_idx = GetSelectedWorldIndex();
    int world_count = GetWorldCount();
    bool has_selected_world = selected_world && world_count > 0;

    if (rpanel_world_index_tbox)
    {
        if (has_selected_world)
        {
            snprintf(rpanel_world_index_tbox->data.textbox.text.string, sizeof(String64), "%d/%d", selected_world_idx + 1, world_count);
        }
        else
        {
            WriteTextboxText(rpanel_world_index_tbox, "0/0");
        }
    }

    if (rpanel_world_universe_pos_tbox)
    {
        if (selected_world)
        {
            WriteTextboxVectorPair(rpanel_world_universe_pos_tbox, selected_world->uni_coords_center);
        }
        else
        {
            WriteTextboxText(rpanel_world_universe_pos_tbox, "N/A");
        }
    }

    if (rpanel_create_world_count_tbox)
    {
        WriteTextboxInt(rpanel_create_world_count_tbox, world_count);
    }

    if (rpanel_create_selected_world_tbox)
    {
        if (has_selected_world)
        {
            WriteTextboxInt(rpanel_create_selected_world_tbox, selected_world_idx + 1);
        }
        else
        {
            WriteTextboxText(rpanel_create_selected_world_tbox, "0");
        }
    }

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    if (spawn_origin)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_spawn_tbox, *spawn_origin);
    }

    Vector2d *next_res = GetNextWorldResolutionPtr();
    if (next_res)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_resolution_tbox, *next_res);
    }

    Vector2d *basis_u = GetNextWorldBasisUPtr();
    if (basis_u)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_basis_u_tbox, *basis_u);
    }

    Vector2d *basis_v = GetNextWorldBasisVPtr();
    if (basis_v)
    {
        WriteTextboxVectorIfUnfocused(rpanel_create_basis_v_tbox, *basis_v);
    }

    float *next_grav = GetNextWorldGravityPtr();
    if (next_grav)
    {
        WriteTextboxNumberIfUnfocused(rpanel_create_gravity_tbox, *next_grav, 2);
    }

    int *auto_select = GetCreateWorldAutoSelectPtr();
    if (auto_select && rpanel_create_auto_select_tbox && !rpanel_create_auto_select_tbox->is_focused)
    {
        WriteTextboxInt(rpanel_create_auto_select_tbox, *auto_select);
    }

    if (selected_world)
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = &selected_world->gravity;
            WriteTextboxNumberIfUnfocused(rpanel_world_gravity_edit_tbox, selected_world->gravity, 2);
        }
        if (rpanel_world_resolution_tbox)
        {
            Vector2d world_size = {(float)selected_world->grid_space.space.columns,
                                   (float)selected_world->grid_space.space.rows};
            snprintf(rpanel_world_resolution_tbox->data.textbox.text.string, sizeof(String64), "%.0fx%.0f", world_size.x, world_size.y);
        }
        if (rpanel_world_objects_tbox)
        {
            WriteTextboxInt(rpanel_world_objects_tbox, selected_world->objects.count);
        }
        if (rpanel_world_next_id_tbox)
        {
            WriteTextboxInt(rpanel_world_next_id_tbox, selected_world->next_object_id);
        }
    }
    else
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            WriteTextboxText(rpanel_world_gravity_edit_tbox, "N/A");
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = NULL;
        }
        if (rpanel_world_resolution_tbox)
            WriteTextboxText(rpanel_world_resolution_tbox, "N/A");
        if (rpanel_world_objects_tbox)
            WriteTextboxText(rpanel_world_objects_tbox, "0");
        if (rpanel_world_next_id_tbox)
            WriteTextboxText(rpanel_world_next_id_tbox, "0");
    }
    // ISSUE IS HERE. COL3 is like 60000!
    Matrix3x3 M_ui_to_pixel = MatrixMultiply_3x3_3x3(
        camera_rpanel.tunnel.source_to_dest_mtx,
        rpanel_viewport_tunnel.source_to_dest_mtx);
    DrawRootUIElement(rpanel_root, rpanel_seed_box, camera_lpanel, M_ui_to_pixel);
}

Frame2d *GetRPanelSpaceFrame(void)
{
    return &rpanel_space.frame;
}

bool SetRPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    if (VectorMagnitude_2d(basis_u) < 0.0001f || VectorMagnitude_2d(basis_v) < 0.0001f)
    {
        return false;
    }

    rpanel_space_basis_override_enabled = true;
    rpanel_space_basis_override_u = basis_u;
    rpanel_space_basis_override_v = basis_v;
    return true;
}

void ResetRPanelSpaceBasis(void)
{
    rpanel_space_basis_override_enabled = false;
    rpanel_space_basis_override_u = ZERO_VECTOR_2D;
    rpanel_space_basis_override_v = ZERO_VECTOR_2D;
}
