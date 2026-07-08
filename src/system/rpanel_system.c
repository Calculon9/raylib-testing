#include "system/rpanel_system.h"

#include <stdio.h>
#include "raylib.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/world_system.h"
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

static CoordSpace2d rpanel_space = {0};
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
static UIElement *rpanel_create_gravity_tbox = NULL;

static Vector2d rpanel_default_padding = {0.1f, 0.1f};
static Vector2d rpanel_tfield_padding = {0.03f, 0.03f};
static Size rpanel_title_tfield_size = {{5.8f, 0.45f}, SIZE_FIXED};
static Size rpanel_row_tfield_size = {{5.8f, 0.5f}, SIZE_FIXED};
static Size rpanel_stat_row_tfield_size = {{5.8f, 0.55f}, SIZE_FIXED};
static Size rpanel_button_size = {{5.8f, 0.45f}, SIZE_FIXED};
static Spacing rpanel_world_btn_child_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};

static void SetTextboxText(UIElement *textbox, const char *value)
{
    if (!textbox)
    {
        return;
    }

    safe_strncpy(textbox->data.textbox.text.string, value, MAX_LABEL_CHARS);
}

static void SetTextboxInt(UIElement *textbox, int value)
{
    if (!textbox)
    {
        return;
    }

    snprintf(textbox->data.textbox.text.string, sizeof(String64), "%d", value);
}

static void SetTextboxFloat(UIElement *textbox, float value, int precision)
{
    if (!textbox)
    {
        return;
    }

    char format[16] = {0};
    snprintf(format, sizeof(format), "%%.%df", precision);
    snprintf(textbox->data.textbox.text.string, sizeof(String64), format, value);
}

static void SetTextboxVector2Pair(UIElement *textbox, Vector2d value)
{
    if (!textbox)
    {
        return;
    }

    snprintf(textbox->data.textbox.text.string, sizeof(String64), "(%.1f,%.1f)", value.x, value.y);
}

static UIElement *CreateRPanelTitleLabel(UIElement *parent, const char *text)
{
    return CreatePanelTitleLabel(parent,
                                 text,
                                 rpanel_title_tfield_size,
                                 rpanel_tfield_padding,
                                 FONT_BASIC,
                                 COLOURLESS_RGBA,
                                 COLOURLESS_RGBA);
}

static UIElement *CreateRPanelLabeledField(UIElement *parent, const char *label_text, UIElementType input_type)
{
    return CreatePanelLabeledField(parent,
                                   label_text,
                                   input_type,
                                   rpanel_row_tfield_size,
                                   tbox_default_size,
                                   rpanel_tfield_padding,
                                   tbox_tlabel_default_offset.offset,
                                   WHITE_RGBA,
                                   COLOURLESS_RGBA,
                                   tbox_default_padding,
                                   tbox_default_colour_border,
                                   tbox_default_colour_fill,
                                   FONT_BASIC);
}

static UIElement *CreateRPanelButton(UIElement *parent, const char *text, int *action_ptr)
{
    return CreatePanelButton(parent,
                             UI_ELEMENT_BUTTON_SUBMIT,
                             text,
                             rpanel_button_size,
                             (Vector2d){0.02f, 0.02f},
                             btn_default_colour_border,
                             btn_default_colour_fill,
                             FONT_BASIC,
                             HandleBtnSubmitClick,
                             action_ptr,
                             NULL);
}

static UIElement *CreateRPanelToggleButton(UIElement *parent, const char *text)
{
    return CreatePanelButton(parent,
                             UI_ELEMENT_BUTTON_ENUMERATE,
                             text,
                             rpanel_button_size,
                             (Vector2d){0.02f, 0.02f},
                             btn_default_colour_border,
                             btn_default_colour_fill,
                             FONT_BASIC,
                             HandleBtnEnumerateClick,
                             &btn_action_rpanel_enumerate,
                             &rpanel_views);
}

static UIElement *CreateRPanelStatField(UIElement *parent, const char *label_text)
{
    return CreatePanelLabeledField(parent,
                                   label_text,
                                   UI_ELEMENT_TEXTBOX_O,
                                   rpanel_stat_row_tfield_size,
                                   tbox_default_size,
                                   rpanel_tfield_padding,
                                   tbox_tlabel_default_offset.offset,
                                   WHITE_RGBA,
                                   COLOURLESS_RGBA,
                                   tbox_default_padding,
                                   tbox_default_colour_border,
                                   tbox_default_colour_fill,
                                   FONT_BASIC);
}

void InitRPanel(void)
{
    Basis2d rpanel_basis = (Basis2d){lpanel_u, lpanel_v};
    Basis2d rpanel_pixel_basis = (Basis2d){rpanel_pixel_u, rpanel_pixel_v};

    camera_rpanel = CreateCamera2d(rpanel_pixel_basis, rpanel_basis, rpanel_pixel_origin, rpanel_origin);

    rpanel_space = NewCoordSpace2d(rpanel_origin, rpanel_resolution, rpanel_basis);
    rpanel_root = CreateUIElement(
        UI_ELEMENT_ROOT,
        (Size){rpanel_space.resolution_ixj, SIZE_FILL},
        (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
        rpanel_default_padding,
        COLOURLESS_RGBA,
        COLOUR_PANEL_DARK_1);

    rpanel_root->data.root.coord_space = rpanel_space;
    rpanel_root->child_spacing = (Spacing){{0.0f, 0.0f}, PERCENT, SPACING_NORMAL};

    rpanel_views = MakeLArray(2, sizeof(View *));
    rpanel_state_view = &rpanel_state_view_storage;
    rpanel_create_view = &rpanel_create_view_storage;
    btn_action_rpanel_enumerate = 0;

    Vector2d basis_scale = BasisTransform_2d_Scale(camera_rpanel.source_basis, camera_rpanel.destination_basis);
    rpanel_seed_box.coords = (Vector2d){rpanel_pixel_origin.x, rpanel_pixel_origin.y};
    rpanel_seed_box.dimensions = (Vector2d){rpanel_resolution.x * basis_scale.x, rpanel_resolution.y * basis_scale.y};

    rpanel_toggle_cont = CreatePanelContainer(rpanel_root,
                                              (Size){{1.0f, 0.08f}, SIZE_PERCENT},
                                              (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                              ZERO_VECTOR_2D,
                                              COLOURLESS_RGBA,
                                              COLOURLESS_RGBA,
                                              tcont_default_child_spacing,
                                              false,
                                              true);
    CreateRPanelToggleButton(rpanel_toggle_cont, "STATE -- CREATE");

    rpanel_state_view_cont = CreatePanelContainer(rpanel_root,
                                                  (Size){{1.0f, 0.92f}, SIZE_PERCENT},
                                                  (Offset){{0.0f, 0.08f}, OFFSET_PERCENT},
                                                  ZERO_VECTOR_2D,
                                                  COLOURLESS_RGBA,
                                                  COLOURLESS_RGBA,
                                                  cont_default_child_spacing,
                                                  false,
                                                  true);

    rpanel_create_view_cont = CreatePanelContainer(rpanel_root,
                                                   (Size){{1.0f, 0.92f}, SIZE_PERCENT},
                                                   (Offset){{0.0f, 0.08f}, OFFSET_PERCENT},
                                                   ZERO_VECTOR_2D,
                                                   COLOURLESS_RGBA,
                                                   COLOURLESS_RGBA,
                                                   tcont_default_child_spacing,
                                                   false,
                                                   false);

    rpanel_state_view->container = rpanel_state_view_cont;
    rpanel_state_view->type = RPANEL_STATE_VIEW;
    rpanel_create_view->container = rpanel_create_view_cont;
    rpanel_create_view->type = RPANEL_WORLD_CREATE_VIEW;
    LArray_Push(&rpanel_views, &rpanel_state_view);
    LArray_Push(&rpanel_views, &rpanel_create_view);

    UIElement *world_cont = CreatePanelContainer(rpanel_state_view_cont,
                                                 (Size){{1.0f, 0.5f}, SIZE_PERCENT},
                                                 (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                 tcont_default_padding,
                                                 tcont_default_colour_border,
                                                 tcont_default_colour_fill,
                                                 tcont_default_child_spacing,
                                                 true,
                                                 true);

    CreateRPanelTitleLabel(world_cont, "WORLD MANAGER");

    UIElement *world_btn_cont = CreatePanelContainer(world_cont,
                                                     (Size){{1.0f, 0.22f}, SIZE_PERCENT},
                                                     (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                     ZERO_VECTOR_2D,
                                                     COLOURLESS_RGBA,
                                                     COLOURLESS_RGBA,
                                                     rpanel_world_btn_child_spacing,
                                                     false,
                                                     true);

    CreateRPanelButton(world_btn_cont, "SELECT PREV", &btn_action_select_world_prev);
    CreateRPanelButton(world_btn_cont, "SELECT NEXT", &btn_action_select_world_next);

    rpanel_world_index_tbox = CreateRPanelLabeledField(world_cont, "WORLD", UI_ELEMENT_TEXTBOX_O);
    rpanel_world_universe_pos_tbox = CreateRPanelLabeledField(world_cont, "UNIVERSE", UI_ELEMENT_TEXTBOX_O);
    rpanel_world_gravity_edit_tbox = CreateRPanelLabeledField(world_cont, "GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO);
    if (rpanel_world_gravity_edit_tbox)
    {
        rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
    }

    rpanel_world_resolution_tbox = CreateRPanelLabeledField(world_cont, "RES", UI_ELEMENT_TEXTBOX_O);
    rpanel_world_objects_tbox = CreateRPanelLabeledField(world_cont, "OBJECTS", UI_ELEMENT_TEXTBOX_O);
    rpanel_world_next_id_tbox = CreateRPanelLabeledField(world_cont, "NEXT ID", UI_ELEMENT_TEXTBOX_O);

    UIElement *stats_cont = CreatePanelContainer(rpanel_state_view_cont,
                                                 (Size){{1.0f, 0.32f}, SIZE_PERCENT},
                                                 (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                 tcont_default_padding,
                                                 tcont_default_colour_border,
                                                 tcont_default_colour_fill,
                                                 tcont_default_child_spacing,
                                                 true,
                                                 true);

    CreateRPanelTitleLabel(stats_cont, "UTILITY PANEL");
    rpanel_stats_fps_tbox = CreateRPanelStatField(stats_cont, "FPS");
    rpanel_stats_frame_tbox = CreateRPanelStatField(stats_cont, "FRAME (MS)");
    rpanel_stats_entities_tbox = CreateRPanelStatField(stats_cont, "ENTITIES");

    UIElement *create_world_cont = CreatePanelContainer(rpanel_create_view_cont,
                                                        (Size){{0.96f, 0.78f}, SIZE_PERCENT},
                                                        (Offset){{0.02f, 0.02f}, OFFSET_PERCENT},
                                                        tcont_default_padding,
                                                        tcont_default_colour_border,
                                                        tcont_default_colour_fill,
                                                        tcont_default_child_spacing,
                                                        true,
                                                        true);

    CreateRPanelTitleLabel(create_world_cont, "WORLD CREATION");
    rpanel_create_world_count_tbox = CreateRPanelLabeledField(create_world_cont, "WORLDS", UI_ELEMENT_TEXTBOX_O);
    rpanel_create_selected_world_tbox = CreateRPanelLabeledField(create_world_cont, "SELECTED", UI_ELEMENT_TEXTBOX_O);
    rpanel_create_spawn_tbox = CreateRPanelLabeledField(create_world_cont, "SPAWN", UI_ELEMENT_TEXTBOX_SAFE_IO);
    rpanel_create_resolution_tbox = CreateRPanelLabeledField(create_world_cont, "RESOLUTION", UI_ELEMENT_TEXTBOX_SAFE_IO);
    rpanel_create_gravity_tbox = CreateRPanelLabeledField(create_world_cont, "GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO);

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    if (rpanel_create_spawn_tbox && spawn_origin)
    {
        rpanel_create_spawn_tbox->data.textbox.data_type = VECTOR2D;
        rpanel_create_spawn_tbox->data.textbox.data_bind = spawn_origin;
    }
    Vector2d *next_res = GetNextWorldResolutionPtr();
    if (rpanel_create_resolution_tbox && next_res)
    {
        rpanel_create_resolution_tbox->data.textbox.data_type = VECTOR2D;
        rpanel_create_resolution_tbox->data.textbox.data_bind = next_res;
    }
    float *next_grav = GetNextWorldGravityPtr();
    if (rpanel_create_gravity_tbox && next_grav)
    {
        rpanel_create_gravity_tbox->data.textbox.data_type = FLOAT;
        rpanel_create_gravity_tbox->data.textbox.data_bind = next_grav;
    }
    CreateRPanelButton(create_world_cont, "NEW WORLD", &btn_action_create_world);
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
        SetTextboxFloat(rpanel_stats_fps_tbox, frame_counter.fps, 1);
    }
    if (rpanel_stats_frame_tbox)
    {
        SetTextboxFloat(rpanel_stats_frame_tbox, frame_counter.delta_time * 1000.0f, 2);
    }
    if (rpanel_stats_entities_tbox)
    {
        SetTextboxInt(rpanel_stats_entities_tbox, GetNewtonoidCount());
    }

    World2d *selected_world = GetSelectedWorld();
    int selected_world_idx = GetSelectedWorldIndex();
    int world_count = GetWorldCount();

    if (rpanel_world_index_tbox)
    {
        if (selected_world && world_count > 0)
        {
            snprintf(rpanel_world_index_tbox->data.textbox.text.string, sizeof(String64), "%d/%d", selected_world_idx + 1, world_count);
        }
        else
        {
            SetTextboxText(rpanel_world_index_tbox, "0/0");
        }
    }

    if (rpanel_world_universe_pos_tbox)
    {
        if (selected_world)
        {
            SetTextboxVector2Pair(rpanel_world_universe_pos_tbox, selected_world->universe_position);
        }
        else
        {
            SetTextboxText(rpanel_world_universe_pos_tbox, "N/A");
        }
    }

    if (rpanel_create_world_count_tbox)
    {
        SetTextboxInt(rpanel_create_world_count_tbox, world_count);
    }

    if (rpanel_create_selected_world_tbox)
    {
        if (selected_world && world_count > 0)
        {
            SetTextboxInt(rpanel_create_selected_world_tbox, selected_world_idx + 1);
        }
        else
        {
            SetTextboxText(rpanel_create_selected_world_tbox, "0");
        }
    }

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    if (spawn_origin && rpanel_create_spawn_tbox && !rpanel_create_spawn_tbox->is_focused)
    {
        PipelineVectorToText(*spawn_origin, rpanel_create_spawn_tbox->data.textbox.text.string, sizeof(String64));
    }

    Vector2d *next_res = GetNextWorldResolutionPtr();
    if (next_res && rpanel_create_resolution_tbox && !rpanel_create_resolution_tbox->is_focused)
    {
        PipelineVectorToText(*next_res, rpanel_create_resolution_tbox->data.textbox.text.string, sizeof(String64));
    }

    float *next_grav = GetNextWorldGravityPtr();
    if (next_grav && rpanel_create_gravity_tbox && !rpanel_create_gravity_tbox->is_focused)
    {
        PipelineNumberToText(*next_grav, 2, rpanel_create_gravity_tbox->data.textbox.text.string, sizeof(String64));
    }

    if (selected_world)
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = &selected_world->gravity;
            if (!rpanel_world_gravity_edit_tbox->is_focused)
            {
                PipelineNumberToText(selected_world->gravity, 2, rpanel_world_gravity_edit_tbox->data.textbox.text.string, sizeof(String64));
            }
        }
        if (rpanel_world_resolution_tbox)
        {
            Vector2d world_size = selected_world->coord_space_grid.coord_space.resolution_ixj;
            snprintf(rpanel_world_resolution_tbox->data.textbox.text.string, sizeof(String64), "%.0fx%.0f", world_size.x, world_size.y);
        }
        if (rpanel_world_objects_tbox)
        {
            SetTextboxInt(rpanel_world_objects_tbox, selected_world->objects.count);
        }
        if (rpanel_world_next_id_tbox)
        {
            SetTextboxInt(rpanel_world_next_id_tbox, selected_world->next_object_id);
        }
    }
    else
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            SetTextboxText(rpanel_world_gravity_edit_tbox, "N/A");
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = NULL;
        }
        if (rpanel_world_resolution_tbox)
            SetTextboxText(rpanel_world_resolution_tbox, "N/A");
        if (rpanel_world_objects_tbox)
            SetTextboxText(rpanel_world_objects_tbox, "0");
        if (rpanel_world_next_id_tbox)
            SetTextboxText(rpanel_world_next_id_tbox, "0");
    }

    DrawRootUIElement(rpanel_root, rpanel_seed_box, camera_rpanel);
}
