#include "system/ui/lpanel_system.h"
#include "system/ui/ui_loader.h"
#include "system/command_system.h"
#include "system/viewport_system.h"
#include "system/ui/ui_state.h"
#include "ui/ui.h"
#include "system/ui_system.h"
#include "entities/entity_factory.h"
#include "system/debug_overlay_system.h"
#include "ui/ui_constructors.h"
#include "system/panel_system.h"
#include "system/systems.h"
#include <stdint.h>
#include <string.h>

// ============================================================================
// Panel System
// ============================================================================
static PanelSystem *lpanel = NULL;

// ============================================================================
// Action Codes
// ============================================================================
static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;

// ============================================================================
// Visual Style Properties
// ============================================================================
static Size debug_section_size = UI_SIZE_CONTENT_FILL;

// ============================================================================
// UI Element Pointers
// ============================================================================
UIElement *lpanel_state_view_cont = {0};
UIElement *lpanel_edit_view_cont = {0};
UIElement *lpanel_view_selector_cont = {0};
UIElement *lpanel_edit_entity_tcont = {0};
static ViewSelector *lpanel_view_selector = NULL;

typedef struct
{
    DebugOverlayId id;
    const char *label;
} LPanelDebugToggle;

static const LPanelDebugToggle lpanel_debug_general_toggles[] = {
    {DEBUG_DASHBOARD, "Dashboard"},
};

static const LPanelDebugToggle lpanel_debug_viewport_toggles[] = {
    {DEBUG_VIEWPORT_GRID, "Viewport Grid"},
};

static const LPanelDebugToggle lpanel_debug_world_toggles[] = {
    {DEBUG_WORLD_GRID, "World Grid"},
    {DEBUG_WORLD_GRID_LABELS, "World Grid Labels"},
    {DEBUG_UNIVERSE_GRID_LABELS, "Universe Grid Labels"},
};

static const LPanelDebugToggle lpanel_debug_ui_toggles[] = {
    {DEBUG_UI_BORDERS, "UI Borders"},
};

static const LPanelDebugToggle lpanel_debug_object_toggles[] = {
    {DEBUG_OBJECT_AXES, "Object Axes"},
    {DEBUG_OBJECT_HULL, "Object Hull"},
    {DEBUG_OBJECT_AABB, "Object AABB"},
};

static void HandleLPanelDebugToggleClickInternal(UIElement *button)
{
    if (!button || !button->data.button.user_data)
    {
        return;
    }

    const LPanelDebugToggle *toggle = (const LPanelDebugToggle *)button->data.button.user_data;
    ToggleDebug(toggle->id);
    UpdateString64(button->data.button.label.string, "%s: %s", toggle->label,
                   IsDebugEnabled(toggle->id) ? "ON" : "OFF");
}

// ============================================================================
// UI Loader Resolvers (Application-Specific)
// ============================================================================

/**
 * Resolve data bindings for entity creation UI.
 * Maps binding strings like "physics.width" to entity_create_params fields.
 */
static UIBinding LPanel_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    UIBinding binding = {NULL, FLOAT};  // Default: unresolved
    
    if (!binding_string || !G_UIState.entity_create_params)
        return binding;
    
    // Parse binding string format: "component.field"
    const char *dot_pos = strchr(binding_string, '.');
    if (!dot_pos)
        return binding;  // Invalid format
    
    int component_len = dot_pos - binding_string;
    const char *field_name = dot_pos + 1;
    
    // Handle physics.* bindings for Newtonoid2dParams
    if (component_len == 7 && !strncmp(binding_string, "physics", 7))
    {
        Newtonoid2dParams *physics = &G_UIState.entity_create_params->physics;
        
        // Map field names to addresses and types
        if (!strcmp(field_name, "vertice_count"))
        {
            binding.address = &physics->vertice_count;
            binding.data_type = INT;
        }
        else if (!strcmp(field_name, "width"))
        {
            binding.address = &physics->width;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "height"))
        {
            binding.address = &physics->height;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "mass"))
        {
            binding.address = &physics->mass;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "restitution"))
        {
            binding.address = &physics->restitution;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "friction"))
        {
            binding.address = &physics->friction;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "radius"))
        {
            binding.address = &physics->radius;
            binding.data_type = FLOAT;
        }
        else if (!strcmp(field_name, "anchor_position"))
        {
            binding.address = &physics->anchor_position;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "velocity"))
        {
            binding.address = &physics->velocity;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "acceleration"))
        {
            binding.address = &physics->acceleration;
            binding.data_type = VECTOR2D;
        }
        else if (!strcmp(field_name, "momentum"))
        {
            binding.address = &physics->momentum;
            binding.data_type = VECTOR2D;
        }
    }
    // TODO: Add support for other components (portal.*, rotor.*, gear.*, health.*)
    
    return binding;
}

/**
 * Resolve action strings to command codes (adapter for CommandSystem_ResolveString).
 * This is a wrapper around the universal command system resolver, allowing application-specific
 * logic to be added here if needed in the future.
 */
static int LPanel_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)ctx;  // Currently unused; provided for future application-specific context
    return CommandSystem_ResolveString(cmd_string);
}

// Recalculate the left-panel UI tree immediately for layout debugging.
static void HandleLPanelUIRefreshClick(UIElement *button)
{
    (void)button;

    if (lpanel && lpanel->root)
    {
        UpdateUISpace(lpanel->root, lpanel->seed_box);
    }
}

// ============================================================================
// Root Layout
// ============================================================================
// ============================================================================
// Toggle Button Layout
// ============================================================================
// ============================================================================
// State View Layout
// ============================================================================s
// ============================================================================
// Edit View Layout
// ============================================================================
Size create_entity_section_size = UI_SIZE_CONTENT_FILL;

void InitLPanelStateView(void);
void InitLPanelEditView(void);

static void InitEntityCreateDefaults(void)
{
    if (!G_UIState.entity_create_params)
    {
        return;
    }

    EntityCreateParams *params = G_UIState.entity_create_params;
    params->component_flags = 0;
    params->physics.shape_type = SHAPE_AUTO;
    params->physics.vertice_count = 4;
    params->physics.width = 1.0f;
    params->physics.height = 1.0f;
    params->physics.mass = 1.0f;
    params->physics.restitution = 0.9f;
    params->physics.friction = 0.5f;
    params->physics.anchor_position = ZERO_VECTOR_2D;
    params->physics.velocity = ZERO_VECTOR_2D;
    params->portal_params.entrant_roles = ENTITY_ROLE_NEWTONOID | ENTITY_ROLE_PROJECTILE;
    params->portal_params.cooldown_frames = 30;

    WriteTextboxInt(G_UIState.edit_vertice_count_tbox, params->physics.vertice_count);
    WriteTextboxFloat(G_UIState.edit_width_tbox, params->physics.width, 2);
    WriteTextboxFloat(G_UIState.edit_height_tbox, params->physics.height, 2);
    WriteTextboxFloat(G_UIState.edit_mass_tbox, params->physics.mass, 2);
    WriteTextboxFloat(G_UIState.edit_restitution_tbox, params->physics.restitution, 2);
    WriteTextboxFloat(G_UIState.edit_friction_tbox, params->physics.friction, 2);
    WriteTextboxVectorPair(G_UIState.edit_pos_c_tbox, params->physics.anchor_position);
    WriteTextboxVectorPair(G_UIState.edit_vel_tbox, params->physics.velocity);
}

void InitLPanel()
{
    // Try to load from XML first with application-provided resolvers
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        "C:\\Projects\\raylib-testing\\src\\engine\\ui\\components\\lpanel.xml",
        &ui_default_palette,
        LPanel_ResolveBinding,
        LPanel_ResolveCommand,
        NULL
    );

    if (root)
    {
        // XML loaded successfully; extract Views from tree
        size_t view_count = 0;
        View **xml_views = UILoader_ExtractViews(root, &view_count);
        
        // Create panel infrastructure
        const char *labels[] = {"STATE", "DRAW"};
        lpanel = PanelSystem_CreateStandard(&lpanel_viewport, 2, labels, ARRAY_COUNT(labels),
                                            PanelSystem_HandleViewSelected,
                                            &ui_default_palette, ui_standard_stack_spacing);
        if (lpanel)
        {
            lpanel->root = root;
            
            // Register XML-created Views with panel
            for (size_t i = 0; i < view_count; i++)
            {
                LArray_Push(&lpanel->views, &xml_views[i]);
            }
            
            // Free the array (not the Views, which panel now owns)
            if (xml_views)
            {
                Deallocate((void **)&xml_views, sizeof(View *) * view_count);
            }
            
            PanelSystem_FinaliseInit(lpanel, &lpanel_view_selector);
        }
        return;
    }

    // Fallback: build panel-specific UI with hardcoded construction
    // const char *labels[] = {"STATE", "DRAW"};
    // lpanel = PanelSystem_CreateStandard(&lpanel_viewport, 2, labels, ARRAY_COUNT(labels),
    //                                     PanelSystem_HandleViewSelected,
    //                                     &ui_default_palette, ui_standard_stack_spacing);
    // if (!lpanel)
    // {
    //     return;
    // }

    // // Build panel-specific UI
    // InitLPanelStateView();
    // InitLPanelEditView();

    // // Finalise: select first view and update layout
    // PanelSystem_FinaliseInit(lpanel, &lpanel_view_selector);
}

void InitLPanelStateView(void)
{
    View *view = PanelSystem_CreateView(lpanel, LPANEL_STATE_VIEW);
    lpanel_state_view_cont = view ? view->container : NULL;

    if (!lpanel_state_view_cont)
    {
        return;
    }

    // Apply standard view container styling
    PanelSystem_StyleViewContainer(lpanel_state_view_cont, lpanel->palette);

    // Keep every debug feature in STATE, grouped by the system it visualises.
    const struct
    {
        const char *title;
        const LPanelDebugToggle *toggles;
        size_t count;
    } debug_sections[] = {
        {"General", lpanel_debug_general_toggles, ARRAY_COUNT(lpanel_debug_general_toggles)},
        {"Viewport", lpanel_debug_viewport_toggles, ARRAY_COUNT(lpanel_debug_viewport_toggles)},
        {"World", lpanel_debug_world_toggles, ARRAY_COUNT(lpanel_debug_world_toggles)},
        {"UI", lpanel_debug_ui_toggles, ARRAY_COUNT(lpanel_debug_ui_toggles)},
        {"Objects", lpanel_debug_object_toggles, ARRAY_COUNT(lpanel_debug_object_toggles)},
    };

    for (size_t section_index = 0; section_index < ARRAY_COUNT(debug_sections); section_index++)
    {
        UIElement *section = CreateViewSection_Stack(
            lpanel_state_view_cont, debug_sections[section_index].title,
            debug_section_size, lpanel->palette);

        for (size_t toggle_index = 0; toggle_index < debug_sections[section_index].count; toggle_index++)
        {
            const LPanelDebugToggle *toggle = &debug_sections[section_index].toggles[toggle_index];
            String64 label = {0};
            UpdateString64(label.string, "%s: %s", toggle->label, IsDebugEnabled(toggle->id) ? "ON" : "OFF");
            CreateUIButtonDefault(section, UI_ELEMENT_BUTTON_SIMPLE, label.string,
                                  ui_wide_button_size, ui_standard_button_padding,
                                  lpanel->palette, HandleLPanelDebugToggleClickInternal, (void *)toggle, NULL);
        }
    }
}

void InitLPanelEditView(void)
{
    // Create View's container & register the View
    View *view = PanelSystem_CreateView(lpanel, LPANEL_DRAW_VIEW);
    lpanel_edit_view_cont = view ? view->container : NULL;
    if (!lpanel_edit_view_cont)
    {
        return;
    }

    // The edit view starts disabled until selected.
    DisableElement(lpanel_edit_view_cont);

    // Build the editable object controls as a standard stacked ViewSection.
    lpanel_edit_entity_tcont = CreateViewSection_Stack(
        lpanel_edit_view_cont, "Object Create", create_entity_section_size,
        lpanel->palette);

    const UIFieldSpec edit_specs[] = {
        {"Vertices", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, INT, &G_UIState.edit_vertice_count_tbox, NULL, &G_UIState.entity_create_params->physics.vertice_count},
        {"Width", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_width_tbox, NULL, &G_UIState.entity_create_params->physics.width},
        {"Height", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_height_tbox, NULL, &G_UIState.entity_create_params->physics.height},
        {"Mass", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_mass_tbox, NULL, &G_UIState.entity_create_params->physics.mass},
        {"Restitution", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_restitution_tbox, NULL, &G_UIState.entity_create_params->physics.restitution},
        {"Friction", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, FLOAT, &G_UIState.edit_friction_tbox, NULL, &G_UIState.entity_create_params->physics.friction},
        {"Anchor", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_pos_c_tbox, NULL, &G_UIState.entity_create_params->physics.anchor_position},
        {"Vel", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_vel_tbox, NULL, &G_UIState.entity_create_params->physics.velocity},
        {"Acc", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_accel_tbox, NULL},
        {"Moment", UI_ELEMENT_TEXTBOX_SAFE_IO, ui_standard_control_size, VECTOR2D, &G_UIState.edit_moment_tbox, NULL},
    };
    InitUIFields(lpanel_edit_entity_tcont, edit_specs,
                 ARRAY_COUNT(edit_specs), ui_standard_field_padding,
                 lpanel->palette);

    InitEntityCreateDefaults();

    CreateUIButtonDefault(lpanel_edit_entity_tcont, UI_ELEMENT_BUTTON_SUBMIT,
                          "CREATE", ui_standard_button_size, ui_standard_button_padding,
                          lpanel->palette, HandleBtnSubmitClick,
                          &btn_action_create_entity, NULL);
}

// void InitEntityEditorContainer(void)
// {

// }

void DrawLPanel(void)
{
    if (!lpanel)
    {
        return;
    }

    PanelSystem_Draw(lpanel);
}

Frame2d *GetLPanelSpaceFrame(void)
{
    return PanelSystem_GetSpaceFrame(lpanel);
}

bool SetLPanelSpaceBasis(Vector2d basis_u, Vector2d basis_v)
{
    return PanelSystem_SetSpaceBasis(lpanel, basis_u, basis_v);
}

void ResetLPanelSpaceBasis(void)
{
    PanelSystem_ResetSpaceBasis(lpanel);
}

UIElement *GetLPanelRoot(void)
{
    return lpanel ? lpanel->root : NULL;
}

PanelSystem *GetLPanelSystem(void)
{
    return lpanel;
}

// Destroy the left panel and clear its cached UI references.
void DestroyLPanel(void)
{
    PanelSystem *panel = lpanel;
    lpanel = NULL;
    PanelSystem_Destroy(panel);

    lpanel_state_view_cont = NULL;
    lpanel_edit_view_cont = NULL;
    lpanel_edit_entity_tcont = NULL;
    lpanel_view_selector_cont = NULL;
    lpanel_view_selector = NULL;
    G_UIState.lpanel_views = NULL;

    G_UIState.edit_edge_count_tbox = NULL;
    G_UIState.edit_vertice_count_tbox = NULL;
    G_UIState.edit_width_tbox = NULL;
    G_UIState.edit_height_tbox = NULL;
    G_UIState.edit_mass_tbox = NULL;
    G_UIState.edit_restitution_tbox = NULL;
    G_UIState.edit_friction_tbox = NULL;
    G_UIState.edit_pos_c_tbox = NULL;
    G_UIState.edit_vel_tbox = NULL;
    G_UIState.edit_accel_tbox = NULL;
    G_UIState.edit_moment_tbox = NULL;
    G_UIState.edit_edge_count_str = NULL;
    G_UIState.edit_vertice_count_str = NULL;
    G_UIState.edit_width_str = NULL;
    G_UIState.edit_height_str = NULL;
    G_UIState.edit_mass_str = NULL;
    G_UIState.edit_restitution_str = NULL;
    G_UIState.edit_friction_str = NULL;
    G_UIState.edit_pos_c_str = NULL;
    G_UIState.edit_vel_str = NULL;
    G_UIState.edit_accel_str = NULL;
    G_UIState.edit_moment_str = NULL;
}
