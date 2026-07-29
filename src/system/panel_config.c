// #include "system/panel_config.h"

// #include "system/viewport_system.h"
// #include "system/ui_system.h"
// #include "system/str_helpers.h"
// #include "world/world.h"
// #include "system/systems.h"
// #include "system/world_system.h"
// #include "system/universe_system.h"
// #include "world/universe.h"

// // ============================================================================
// // Shared Layout Constants
// // ============================================================================
// static const Vector2d panel_tfield_padding = {0.03f, 0.03f};
// static const Size panel_title_size = {{6.0f, 0.5f}, SIZE_FIXED};
// static const Size panel_row_size = {{6.0f, 0.5f}, SIZE_FIXED};
// static const Size panel_btn_cont_size = {{1.0f, 0.08f}, SIZE_PERCENT};
// static const Spacing panel_btn_spacing = {{0.0f, 0.03f}, NONE, SPACING_STACKED};

// // Button/field defaults (matching ui_system.h defaults)
// static const Size default_btn_size = {{1.0f, 0.25f}, SIZE_FIXED};
// static const Vector2d default_btn_padding = {0.025f, 0.025f};
// static const Size default_tfield_size = {{6.0f, 0.36f}, SIZE_FIXED};

// // ============================================================================
// // LPanel Configuration
// // ============================================================================

// // Stats container fields
// static const PanelFieldSpec lpanel_stats_fields[] = {
//     {"POLYOIDS", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_stats_polygs_str},
//     {"MEM", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_stats_mem_str},
//     {"FPS", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_stats_fps_str},
//     {"F.TIME", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_stats_ftime_str},
// };

// // Entity state container fields
// static const PanelFieldSpec lpanel_entity_state_fields[] = {
//     {"ID", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, &G_UIState.lpanel_entity_state_id_tbox, NULL},
//     {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, &G_UIState.lpanel_entity_state_mass_tbox, NULL},
//     {"POS.TL", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_state_pos_tl_tbox, NULL},
//     {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_state_pos_c_tbox, NULL},
//     {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_state_vel_tbox, NULL},
//     {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_state_accel_tbox, NULL},
//     {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_state_moment_tbox, NULL},
// };

// static const ButtonSpec lpanel_entity_delete_button[] = {
//     {"DELETE", UI_ELEMENT_BUTTON_SUBMIT, BUTTON_TYPE_DELETE_ENTITY, {{1.0f, 0.25f}, SIZE_FIXED}, {0.025f, 0.025f}}
// };

// // Cell state container fields
// static const PanelFieldSpec lpanel_cell_state_fields[] = {
//     {"INDEX", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_cell_state_id_str},
//     {"OCCU", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_cell_state_occu_str},
//     {"VALUE", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_cell_state_value_str},
//     {"FILL", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, NULL, &G_UIState.lpanel_cell_state_fill_str},
// };

// // Entity editor container fields
// static const PanelFieldSpec lpanel_entity_edit_fields[] = {
//     {"VERT.CNT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, INT, &G_UIState.lpanel_entity_edit_vertice_count_tbox, NULL},
//     {"WIDTH", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, INT, &G_UIState.lpanel_entity_edit_width_tbox, NULL},
//     {"HEIGHT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, &G_UIState.lpanel_entity_edit_height_tbox, NULL},
//     {"MASS", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, &G_UIState.lpanel_entity_edit_mass_tbox, NULL},
//     {"POS.C", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, FLOAT, &G_UIState.lpanel_entity_edit_pos_c_tbox, NULL},
//     {"VEL", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_edit_vel_tbox, NULL},
//     {"ACCEL", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_edit_accel_tbox, NULL},
//     {"MOMENT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.5f}, SIZE_FIXED}, VECTOR2D, &G_UIState.lpanel_entity_edit_moment_tbox, NULL},
// };

// static const ButtonSpec lpanel_entity_create_button[] = {
//     {"CREATE", UI_ELEMENT_BUTTON_SUBMIT, BUTTON_TYPE_CREATE_ENTITY, {{1.0f, 0.25f}, SIZE_FIXED}, {0.025f, 0.025f}}
// };

// // State view containers
// static const ContainerSpec lpanel_state_containers[] = {
//     {
//         .title = "STATISTICS",
//         .size = {{1.0f, 0.24f}, SIZE_PERCENT},
//         .offset = {{0.0, 0.0}, OFFSET_PERCENT},
//         .fields = lpanel_stats_fields,
//         .field_count = sizeof(lpanel_stats_fields) / sizeof(lpanel_stats_fields[0]),
//         .buttons = NULL,
//         .button_count = 0
//     },
//     {
//         .title = "SELECTED ENTITY",
//         .size = {{1.0f, 0.40f}, SIZE_PERCENT},
//         .offset = {{0.0, 0.0}, OFFSET_PERCENT},
//         .fields = lpanel_entity_state_fields,
//         .field_count = sizeof(lpanel_entity_state_fields) / sizeof(lpanel_entity_state_fields[0]),
//         .buttons = lpanel_entity_delete_button,
//         .button_count = sizeof(lpanel_entity_delete_button) / sizeof(lpanel_entity_delete_button[0])
//     },
//     {
//         .title = "CELL STATE",
//         .size = {{1.0f, 0.24f}, SIZE_PERCENT},
//         .offset = {{0.0, 0.0}, OFFSET_PERCENT},
//         .fields = lpanel_cell_state_fields,
//         .field_count = sizeof(lpanel_cell_state_fields) / sizeof(lpanel_cell_state_fields[0]),
//         .buttons = NULL,
//         .button_count = 0
//     }
// };

// // Edit view containers
// static const ContainerSpec lpanel_edit_containers[] = {
//     {
//         .title = "ENTITY EDIT",
//         .size = {{1.0f, 1.0f}, SIZE_PERCENT},
//         .offset = {{0.0, 0.0}, OFFSET_PERCENT},
//         .fields = lpanel_entity_edit_fields,
//         .field_count = sizeof(lpanel_entity_edit_fields) / sizeof(lpanel_entity_edit_fields[0]),
//         .buttons = lpanel_entity_create_button,
//         .button_count = sizeof(lpanel_entity_create_button) / sizeof(lpanel_entity_create_button[0])
//     }
// };

// // Views
// static const ViewSpec lpanel_views[] = {
//     {
//         .view_type = LPANEL_STATE_VIEW,
//         .enabled = true,
//         .size = {{1.0f, 0.92f}, SIZE_PERCENT},
//         .offset = {{0, 0.08}, OFFSET_PERCENT},
//         .containers = lpanel_state_containers,
//         .container_count = sizeof(lpanel_state_containers) / sizeof(lpanel_state_containers[0])
//     },
//     {
//         .view_type = LPANEL_EDIT_ENTITY_VIEW,
//         .enabled = false,
//         .size = {{1.0f, 0.92f}, SIZE_PERCENT},
//         .offset = {{0, 0.08}, OFFSET_PERCENT},
//         .containers = lpanel_edit_containers,
//         .container_count = sizeof(lpanel_edit_containers) / sizeof(lpanel_edit_containers[0])
//     }
// };

// // LPanel configuration
// static const PanelConfigDef lpanel_config = {
//     .viewport = &lpanel_viewport,
//     .fill_colour = {40, 54, 24, 255},
//     .scale = 1.0f,
//     .view_capacity = 3,
//     .toggle_label = "STATE -- UTIL",
//     .toggle_size = {{1.0f, 0.08f}, SIZE_PERCENT},
//     .toggle_offset = {{0.0, 0.0}, OFFSET_PERCENT},
//     .views = lpanel_views,
//     .view_count = sizeof(lpanel_views) / sizeof(lpanel_views[0])
// };

// // ============================================================================
// // RPanel Configuration
// // ============================================================================

// // World manager fields
// static UIElement *rpanel_world_index_tbox = NULL;
// static UIElement *rpanel_world_universe_pos_tbox = NULL;
// static UIElement *rpanel_world_gravity_edit_tbox = NULL;
// static UIElement *rpanel_world_resolution_tbox = NULL;
// static UIElement *rpanel_world_objects_tbox = NULL;
// static UIElement *rpanel_world_next_id_tbox = NULL;

// static const PanelFieldSpec rpanel_world_fields[] = {
//     {"WORLD", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_index_tbox, NULL},
//     {"UNIVERSE", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_universe_pos_tbox, NULL},
//     {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_gravity_edit_tbox, NULL},
//     {"RES", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_resolution_tbox, NULL},
//     {"OBJECTS", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_objects_tbox, NULL},
//     {"NEXT ID", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_world_next_id_tbox, NULL},
// };

// static const ButtonSpec rpanel_world_buttons[] = {
//     {"SELECT PREV", UI_ELEMENT_BUTTON_SUBMIT, BUTTON_TYPE_SELECT_WORLD_PREV, {{1.0f, 0.25f}, SIZE_FIXED}, {0.025f, 0.025f}},
//     {"SELECT NEXT", UI_ELEMENT_BUTTON_SUBMIT, BUTTON_TYPE_SELECT_WORLD_NEXT, {{1.0f, 0.25f}, SIZE_FIXED}, {0.025f, 0.025f}}
// };

// // Stats fields
// static UIElement *rpanel_stats_entities_tbox = NULL;

// static const PanelFieldSpec rpanel_stats_fields[] = {
//     {"ENTITIES", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_stats_entities_tbox, NULL},
// };

// // World creation fields
// static UIElement *rpanel_create_world_count_tbox = NULL;
// static UIElement *rpanel_create_selected_world_tbox = NULL;
// static UIElement *rpanel_create_spawn_tbox = NULL;
// static UIElement *rpanel_create_resolution_tbox = NULL;
// static UIElement *rpanel_create_basis_u_tbox = NULL;
// static UIElement *rpanel_create_basis_v_tbox = NULL;
// static UIElement *rpanel_create_gravity_tbox = NULL;
// static UIElement *rpanel_create_auto_select_tbox = NULL;

// static const PanelFieldSpec rpanel_create_fields[] = {
//     {"WORLDS", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_create_world_count_tbox, NULL},
//     {"SELECTED", UI_ELEMENT_TEXTBOX_O, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_create_selected_world_tbox, NULL},
//     {"SPAWN", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, VECTOR2D, &rpanel_create_spawn_tbox, NULL},
//     {"RESOLUTION", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, VECTOR2D, &rpanel_create_resolution_tbox, NULL},
//     {"BASIS U", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, VECTOR2D, &rpanel_create_basis_u_tbox, NULL},
//     {"BASIS V", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, VECTOR2D, &rpanel_create_basis_v_tbox, NULL},
//     {"GRAVITY", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, FLOAT, &rpanel_create_gravity_tbox, NULL},
//     {"AUTO SELECT", UI_ELEMENT_TEXTBOX_SAFE_IO, {{6.0f, 0.36f}, SIZE_FIXED}, INT, &rpanel_create_auto_select_tbox, NULL},
// };

// static const ButtonSpec rpanel_create_button[] = {
//     {"NEW WORLD", UI_ELEMENT_BUTTON_SUBMIT, BUTTON_TYPE_CREATE_WORLD, {{1.0f, 0.25f}, SIZE_FIXED}, {0.025f, 0.025f}}
// };

// // State view containers
// static const ContainerSpec rpanel_state_containers[] = {
//     {
//         .title = "WORLD MANAGER",
//         .size = {{1.0f, 0.5f}, SIZE_PERCENT},
//         .offset = {ZERO_VECTOR_2D, OFFSET_FIXED},
//         .fields = rpanel_world_fields,
//         .field_count = sizeof(rpanel_world_fields) / sizeof(rpanel_world_fields[0]),
//         .buttons = rpanel_world_buttons,
//         .button_count = sizeof(rpanel_world_buttons) / sizeof(rpanel_world_buttons[0])
//     },
//     {
//         .title = "UTILITY PANEL",
//         .size = {{1.0f, 0.32f}, SIZE_PERCENT},
//         .offset = {ZERO_VECTOR_2D, OFFSET_FIXED},
//         .fields = rpanel_stats_fields,
//         .field_count = sizeof(rpanel_stats_fields) / sizeof(rpanel_stats_fields[0]),
//         .buttons = NULL,
//         .button_count = 0
//     }
// };

// // Create view containers
// static const ContainerSpec rpanel_create_containers[] = {
//     {
//         .title = "WORLD CREATION",
//         .size = {{1.0f, 0.6f}, SIZE_PERCENT},
//         .offset = {{0.02f, 0.02f}, OFFSET_PERCENT},
//         .fields = rpanel_create_fields,
//         .field_count = sizeof(rpanel_create_fields) / sizeof(rpanel_create_fields[0]),
//         .buttons = rpanel_create_button,
//         .button_count = sizeof(rpanel_create_button) / sizeof(rpanel_create_button[0])
//     }
// };

// // Views
// static const ViewSpec rpanel_views[] = {
//     {
//         .view_type = RPANEL_STATE_VIEW,
//         .enabled = true,
//         .size = {{1.0f, 0.92f}, SIZE_PERCENT},
//         .offset = {{0.0f, 0.08f}, OFFSET_PERCENT},
//         .containers = rpanel_state_containers,
//         .container_count = sizeof(rpanel_state_containers) / sizeof(rpanel_state_containers[0])
//     },
//     {
//         .view_type = RPANEL_WORLD_CREATE_VIEW,
//         .enabled = false,
//         .size = {{1.0f, 0.92f}, SIZE_PERCENT},
//         .offset = {{0.0f, 0.08f}, OFFSET_PERCENT},
//         .containers = rpanel_create_containers,
//         .container_count = sizeof(rpanel_create_containers) / sizeof(rpanel_create_containers[0])
//     }
// };

// // RPanel configuration
// static const PanelConfigDef rpanel_config = {
//     .viewport = &rpanel_viewport,
//     .fill_colour = {40, 54, 24, 255},
//     .scale = 1.0f,
//     .view_capacity = 2,
//     .toggle_label = "STATE -- CREATE",
//     .toggle_size = {{1.0f, 0.08f}, SIZE_PERCENT},
//     .toggle_offset = {ZERO_VECTOR_2D, OFFSET_FIXED},
//     .views = rpanel_views,
//     .view_count = sizeof(rpanel_views) / sizeof(rpanel_views[0])
// };

// // ============================================================================
// // Configuration Accessors
// // ============================================================================

// const PanelConfigDef* GetLPanelConfig(void)
// {
//     return &lpanel_config;
// }

// const PanelConfigDef* GetRPanelConfig(void)
// {
//     return &rpanel_config;
// }

// // ============================================================================
// // Button Action Mapping
// // ============================================================================

// static int btn_action_create_entity = BUTTON_ACTION_CREATE_ENTITY;
// static int btn_action_delete_entity = BUTTON_ACTION_DELETE_ENTITY;
// static int btn_action_create_world = BUTTON_ACTION_CREATE_WORLD;
// static int btn_action_select_world_prev = BUTTON_ACTION_SELECT_WORLD_PREV;
// static int btn_action_select_world_next = BUTTON_ACTION_SELECT_WORLD_NEXT;

// static void* GetButtonActionData(ButtonActionType type)
// {
//     switch (type)
//     {
//         case BUTTON_TYPE_CREATE_ENTITY: return &btn_action_create_entity;
//         case BUTTON_TYPE_DELETE_ENTITY: return &btn_action_delete_entity;
//         case BUTTON_TYPE_CREATE_WORLD: return &btn_action_create_world;
//         case BUTTON_TYPE_SELECT_WORLD_PREV: return &btn_action_select_world_prev;
//         case BUTTON_TYPE_SELECT_WORLD_NEXT: return &btn_action_select_world_next;
//         default: return NULL;
//     }
// }

// // ============================================================================
// // Configuration-Driven Initialization
// // ============================================================================

// void Panel_InitFromConfig(Panel *panel, const PanelConfigDef *config, int *toggle_action_code,
//                           View *view_storage_array, UIElement ***element_ptrs_out, size_t *element_count_out)
// {
//     Panel_Init(panel, config->viewport, config->scale, config->fill_colour, config->view_capacity);

//     *toggle_action_code = 0;
//     Panel_CreateToggleButtons(panel, config->toggle_size, config->toggle_offset,
//                               config->toggle_label, HandleBtnEnumerateClick, toggle_action_code);

//     // Create all views
//     for (size_t v = 0; v < config->view_count; v++)
//     {
//         const ViewSpec *vspec = &config->views[v];
//         View *view_storage = &view_storage_array[v];
        
//         UIElement *view_cont = Panel_CreateView(panel, view_storage, vspec->view_type,
//                                                 vspec->size, vspec->offset, vspec->enabled);

//         // Create containers within the view
//         for (size_t c = 0; c < vspec->container_count; c++)
//         {
//             const ContainerSpec *cspec = &vspec->containers[c];
            
//             UIElement *container = CreatePanelContainer(
//                 view_cont, cspec->size, cspec->offset, tcont_default_padding,
//                 tcont_default_colour_border, tcont_default_colour_fill,
//                 tcont_default_child_spacing, true, true);

//             if (!container) continue;

//             // Create title
//             CreatePanelTitleLabelDefault(container, cspec->title, panel_title_size, panel_tfield_padding);

//             // Create buttons container if needed (buttons before fields)
//             if (cspec->buttons && cspec->button_count > 0)
//             {
//                 UIElement *btn_cont = CreatePanelContainer(
//                     container, panel_btn_cont_size, (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
//                     ZERO_VECTOR_2D, COLOURLESS_RGBA, COLOURLESS_RGBA,
//                     panel_btn_spacing, false, true);

//                 if (btn_cont)
//                 {
//                     for (size_t b = 0; b < cspec->button_count; b++)
//                     {
//                         const ButtonSpec *bspec = &cspec->buttons[b];
//                         void *user_data = GetButtonActionData(bspec->action_type);
//                         CreatePanelButtonDefault(btn_cont, bspec->type, bspec->label,
//                                                 bspec->size, bspec->padding,
//                                                 HandleBtnSubmitClick, user_data, NULL);
//                     }
//                 }
//             }

//             // Create fields
//             InitPanelFields(container, cspec->fields, cspec->field_count,
//                           panel_tfield_padding, WHITE_RGBA, COLOURLESS_RGBA);
//         }
//     }

//     Panel_UpdateAndDraw(panel);
// }

// // ============================================================================
// // RPanel-specific bindings (called after Panel_InitFromConfig)
// // ============================================================================

// void RPanelConfig_SetupBindings(void)
// {
//     if (rpanel_world_gravity_edit_tbox)
//     {
//         rpanel_world_gravity_edit_tbox->data.textbox.data_type = FLOAT;
//     }

//     if (rpanel_create_spawn_tbox)
//     {
//         Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
//         BindTextboxData(rpanel_create_spawn_tbox, VECTOR2D, spawn_origin);
//     }

//     if (rpanel_create_resolution_tbox)
//     {
//         Vector2d *next_res = GetNextWorldResolutionPtr();
//         BindTextboxData(rpanel_create_resolution_tbox, VECTOR2D, next_res);
//     }

//     if (rpanel_create_basis_u_tbox)
//     {
//         Vector2d *basis_u = GetNextWorldBasisUPtr();
//         BindTextboxData(rpanel_create_basis_u_tbox, VECTOR2D, basis_u);
//     }

//     if (rpanel_create_basis_v_tbox)
//     {
//         Vector2d *basis_v = GetNextWorldBasisVPtr();
//         BindTextboxData(rpanel_create_basis_v_tbox, VECTOR2D, basis_v);
//     }

//     if (rpanel_create_gravity_tbox)
//     {
//         float *next_grav = GetNextWorldGravityPtr();
//         BindTextboxData(rpanel_create_gravity_tbox, FLOAT, next_grav);
//     }

//     if (rpanel_create_auto_select_tbox)
//     {
//         int *auto_select = GetCreateWorldAutoSelectPtr();
//         BindTextboxData(rpanel_create_auto_select_tbox, INT, auto_select);
//     }
// }

// // RPanel update logic
// void RPanelConfig_UpdateState(void)
// {
//     if (rpanel_stats_entities_tbox)
//     {
//         WriteTextboxInt(rpanel_stats_entities_tbox, GetNewtonoidCount());
//     }

//     World2d *selected_world = GetSelectedWorld();
//     int selected_world_idx = GetSelectedWorldIndex();
//     int world_count = GetWorldCount();
//     bool has_selected_world = selected_world && world_count > 0;

//     if (rpanel_world_index_tbox)
//     {
//         if (has_selected_world)
//         {
//             snprintf(rpanel_world_index_tbox->data.textbox.text.string, sizeof(String64), "%d/%d", selected_world_idx + 1, world_count);
//         }
//         else
//         {
//             WriteTextboxText(rpanel_world_index_tbox, "0/0");
//         }
//     }

//     if (rpanel_world_universe_pos_tbox)
//     {
//         if (selected_world)
//         {
//             WriteTextboxVectorPair(rpanel_world_universe_pos_tbox, selected_world->uni_coords_center);
//         }
//         else
//         {
//             WriteTextboxText(rpanel_world_universe_pos_tbox, "N/A");
//         }
//     }

//     if (rpanel_create_world_count_tbox)
//     {
//         WriteTextboxInt(rpanel_create_world_count_tbox, world_count);
//     }

//     if (rpanel_create_selected_world_tbox)
//     {
//         if (has_selected_world)
//         {
//             WriteTextboxInt(rpanel_create_selected_world_tbox, selected_world_idx + 1);
//         }
//         else
//         {
//             WriteTextboxText(rpanel_create_selected_world_tbox, "0");
//         }
//     }

//     Vector2d *spawn_origin = GetNextWorldSpawnOriginPtr();
//     if (spawn_origin)
//     {
//         WriteTextboxVectorIfUnfocused(rpanel_create_spawn_tbox, *spawn_origin);
//     }

//     Vector2d *next_res = GetNextWorldResolutionPtr();
//     if (next_res)
//     {
//         WriteTextboxVectorIfUnfocused(rpanel_create_resolution_tbox, *next_res);
//     }

//     Vector2d *basis_u = GetNextWorldBasisUPtr();
//     if (basis_u)
//     {
//         WriteTextboxVectorIfUnfocused(rpanel_create_basis_u_tbox, *basis_u);
//     }

//     Vector2d *basis_v = GetNextWorldBasisVPtr();
//     if (basis_v)
//     {
//         WriteTextboxVectorIfUnfocused(rpanel_create_basis_v_tbox, *basis_v);
//     }

//     float *next_grav = GetNextWorldGravityPtr();
//     if (next_grav)
//     {
//         WriteTextboxNumberIfUnfocused(rpanel_create_gravity_tbox, *next_grav, 2);
//     }

//     int *auto_select = GetCreateWorldAutoSelectPtr();
//     if (auto_select && rpanel_create_auto_select_tbox && !rpanel_create_auto_select_tbox->is_focused)
//     {
//         WriteTextboxInt(rpanel_create_auto_select_tbox, *auto_select);
//     }

//     if (selected_world)
//     {
//         if (rpanel_world_gravity_edit_tbox)
//         {
//             rpanel_world_gravity_edit_tbox->data.textbox.data_bind = &selected_world->gravity;
//             WriteTextboxNumberIfUnfocused(rpanel_world_gravity_edit_tbox, selected_world->gravity, 2);
//         }
//         if (rpanel_world_resolution_tbox)
//         {
//             Vector2d world_size = {(float)selected_world->grid_space.space.columns,
//                                    (float)selected_world->grid_space.space.rows};
//             snprintf(rpanel_world_resolution_tbox->data.textbox.text.string, sizeof(String64), "%.0fx%.0f", world_size.x, world_size.y);
//         }
//         if (rpanel_world_objects_tbox)
//         {
//             WriteTextboxInt(rpanel_world_objects_tbox, selected_world->objects.count);
//         }
//         if (rpanel_world_next_id_tbox)
//         {
//             WriteTextboxInt(rpanel_world_next_id_tbox, selected_world->next_object_id);
//         }
//     }
//     else
//     {
//         if (rpanel_world_gravity_edit_tbox)
//         {
//             WriteTextboxText(rpanel_world_gravity_edit_tbox, "N/A");
//             rpanel_world_gravity_edit_tbox->data.textbox.data_bind = NULL;
//         }
//         if (rpanel_world_resolution_tbox)
//             WriteTextboxText(rpanel_world_resolution_tbox, "N/A");
//         if (rpanel_world_objects_tbox)
//             WriteTextboxText(rpanel_world_objects_tbox, "0");
//         if (rpanel_world_next_id_tbox)
//             WriteTextboxText(rpanel_world_next_id_tbox, "0");
//     }
// }
