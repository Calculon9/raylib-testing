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

typedef struct PanelFieldInit
{
    const char *label;
    UIElementType type;
    Size size;
    UIElement **target;
} PanelFieldInit;

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

static void WriteBoundVectorIfUnfocused(UIElement *textbox, const Vector2d *value)
{
    if (!textbox || !value || textbox->is_focused)
    {
        return;
    }

    PipelineVectorToText(*value, textbox->data.textbox.text.string, sizeof(String64));
}

static void WriteBoundNumberIfUnfocused(UIElement *textbox, const float *value, int precision)
{
    if (!textbox || !value || textbox->is_focused)
    {
        return;
    }

    PipelineNumberToText(*value, precision, textbox->data.textbox.text.string, sizeof(String64));
}

static UIElement *CreateRPanelField(UIElement *parent, const char *label, UIElementType type, Size size)
{
    return CreatePanelLabeledFieldDefault(parent,
                                          label,
                                          type,
                                          size,
                                          rpanel_tfield_padding,
                                          WHITE_RGBA,
                                          COLOURLESS_RGBA);
}

static void InitRPanelFields(UIElement *parent, const PanelFieldInit *defs, size_t count)
{
    if (!parent || !defs)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (defs[i].target)
        {
            *defs[i].target = CreateRPanelField(parent, defs[i].label, defs[i].type, defs[i].size);
        }
    }
}

static void BindTextboxData(UIElement *textbox, DataType type, void *data_bind)
{
    if (!textbox)
    {
        return;
    }

    textbox->data.textbox.data_type = type;
    textbox->data.textbox.data_bind = data_bind;
}

static void CreateRPanelSubmitButton(UIElement *parent, const char *label, int *action)
{
    CreatePanelButtonDefault(parent,
                             UI_ELEMENT_BUTTON_SUBMIT,
                             label,
                             rpanel_button_size,
                             (Vector2d){0.02f, 0.02f},
                             HandleBtnSubmitClick,
                             action,
                             NULL);
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
    CreatePanelButtonDefault(rpanel_toggle_cont,
                             UI_ELEMENT_BUTTON_ENUMERATE,
                             "STATE -- CREATE",
                             rpanel_button_size,
                             (Vector2d){0.02f, 0.02f},
                             HandleBtnEnumerateClick,
                             &btn_action_rpanel_enumerate,
                             &rpanel_views);

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

    CreatePanelTitleLabelDefault(world_cont, "WORLD MANAGER", rpanel_title_tfield_size, rpanel_tfield_padding);

    UIElement *world_btn_cont = CreatePanelContainer(world_cont,
                                                     (Size){{1.0f, 0.22f}, SIZE_PERCENT},
                                                     (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                     ZERO_VECTOR_2D,
                                                     COLOURLESS_RGBA,
                                                     COLOURLESS_RGBA,
                                                     rpanel_world_btn_child_spacing,
                                                     false,
                                                     true);

    CreateRPanelSubmitButton(world_btn_cont, "SELECT PREV", &btn_action_select_world_prev);
    CreateRPanelSubmitButton(world_btn_cont, "SELECT NEXT", &btn_action_select_world_next);

    const PanelFieldInit world_fields[] = {
        {"WORLD", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_index_tbox},
        {"UNIVERSE", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_universe_pos_tbox},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_gravity_edit_tbox},
        {"RES", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_resolution_tbox},
        {"OBJECTS", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_objects_tbox},
        {"NEXT ID", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_world_next_id_tbox},
    };
    InitRPanelFields(world_cont, world_fields, sizeof(world_fields) / sizeof(world_fields[0]));

    if (rpanel_world_gravity_edit_tbox)
    {
        rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
    }

    UIElement *stats_cont = CreatePanelContainer(rpanel_state_view_cont,
                                                 (Size){{1.0f, 0.32f}, SIZE_PERCENT},
                                                 (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                                 tcont_default_padding,
                                                 tcont_default_colour_border,
                                                 tcont_default_colour_fill,
                                                 tcont_default_child_spacing,
                                                 true,
                                                 true);

    CreatePanelTitleLabelDefault(stats_cont, "UTILITY PANEL", rpanel_title_tfield_size, rpanel_tfield_padding);
    const PanelFieldInit stat_fields[] = {
        {"FPS", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.55f}, SIZE_FIXED}, &rpanel_stats_fps_tbox},
        {"FRAME (MS)", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.55f}, SIZE_FIXED}, &rpanel_stats_frame_tbox},
        {"ENTITIES", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.55f}, SIZE_FIXED}, &rpanel_stats_entities_tbox},
    };
    InitRPanelFields(stats_cont, stat_fields, sizeof(stat_fields) / sizeof(stat_fields[0]));

    UIElement *create_world_cont = CreatePanelContainer(rpanel_create_view_cont,
                                                        (Size){{0.96f, 0.78f}, SIZE_PERCENT},
                                                        (Offset){{0.02f, 0.02f}, OFFSET_PERCENT},
                                                        tcont_default_padding,
                                                        tcont_default_colour_border,
                                                        tcont_default_colour_fill,
                                                        tcont_default_child_spacing,
                                                        true,
                                                        true);

    CreatePanelTitleLabelDefault(create_world_cont, "WORLD CREATION", rpanel_title_tfield_size, rpanel_tfield_padding);
    const PanelFieldInit create_fields[] = {
        {"WORLDS", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_world_count_tbox},
        {"SELECTED", UI_ELEMENT_TEXTBOX_O, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_selected_world_tbox},
        {"SPAWN", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_spawn_tbox},
        {"RESOLUTION", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_resolution_tbox},
        {"BASIS U", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_basis_u_tbox},
        {"BASIS V", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_basis_v_tbox},
        {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_gravity_tbox},
        {"AUTO SELECT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{5.8f, 0.5f}, SIZE_FIXED}, &rpanel_create_auto_select_tbox},
    };
    InitRPanelFields(create_world_cont, create_fields, sizeof(create_fields) / sizeof(create_fields[0]));

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
    bool has_selected_world = selected_world && world_count > 0;

    if (rpanel_world_index_tbox)
    {
        if (has_selected_world)
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
            SetTextboxVector2Pair(rpanel_world_universe_pos_tbox, selected_world->uni_coords_center);
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
        if (has_selected_world)
        {
            SetTextboxInt(rpanel_create_selected_world_tbox, selected_world_idx + 1);
        }
        else
        {
            SetTextboxText(rpanel_create_selected_world_tbox, "0");
        }
    }

    Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
    WriteBoundVectorIfUnfocused(rpanel_create_spawn_tbox, spawn_origin);

    Vector2d *next_res = GetNextWorldResolutionPtr();
    WriteBoundVectorIfUnfocused(rpanel_create_resolution_tbox, next_res);

    Vector2d *basis_u = GetNextWorldBasisUPtr();
    WriteBoundVectorIfUnfocused(rpanel_create_basis_u_tbox, basis_u);

    Vector2d *basis_v = GetNextWorldBasisVPtr();
    WriteBoundVectorIfUnfocused(rpanel_create_basis_v_tbox, basis_v);

    float *next_grav = GetNextWorldGravityPtr();
    WriteBoundNumberIfUnfocused(rpanel_create_gravity_tbox, next_grav, 2);

    int *auto_select = GetCreateWorldAutoSelectPtr();
    if (auto_select && rpanel_create_auto_select_tbox && !rpanel_create_auto_select_tbox->is_focused)
    {
        SetTextboxInt(rpanel_create_auto_select_tbox, *auto_select);
    }

    if (selected_world)
    {
        if (rpanel_world_gravity_edit_tbox)
        {
            rpanel_world_gravity_edit_tbox->data.textbox.data_bind = &selected_world->gravity;
            WriteBoundNumberIfUnfocused(rpanel_world_gravity_edit_tbox, &selected_world->gravity, 2);
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
