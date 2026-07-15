// /**********************************************************************************************
//  *
//  *   raylib - Advance Game template
//  *
//  *   Universe System Functions Definitions (Init, Update, Draw)
//  *
//  **********************************************************************************************/
// #include "raylib.h"
// #include "system/universe_system.h"
// #include "system/world_system.h"
// #include "world/universe.h"
// #include "world/world.h"
// #include "camera/camera.h"
// #include "math/cvectors.h"
// #include "common/common.h"
// #include "ui/cfont.h"
// #include "ui/text_region.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------
// static int universe_grid_cells_x = 60;
// static int universe_grid_cells_y = 60;
// static float universe_grid_cell_size = 1.0f;
// static int create_world_auto_select = 0;
// static bool universe_grid_debug_labels_enabled = true;
// static bool coordinate_debug_overlay_enabled = false;
// Vector2d game_viewport_origin, game_viewport_end = {0};
// ColourRgba camera_marker_colour = {255, 80, 80, 100};
// //----------------------------------------------------------------------------------
// // Module Functions Declaration (forward declarations)
// //----------------------------------------------------------------------------------
// void DrawUniverseCameraMarker(void);
// void DrawUniverseGrid(void);
// void DrawCoordinateDebugOverlay(void);

// static Vector2d ResolveGameViewportCenter(void)
// {
//     Vector2d game_viewport_pixel_dimensions = VectorSum_2d(
//         VectorScale_2d(game_viewport_u, game_region_resolution.x),
//         VectorScale_2d(game_viewport_v, game_region_resolution.y));
//     return VectorSum_2d(game_viewport_origin, VectorScale_2d(game_viewport_pixel_dimensions, 0.5f));
// }

// static void DrawUniverseGridLine(Vector2d start, Vector2d end, ColourRgba colour)
// {
//     Vector2d start_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, start);
//     Vector2d end_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, end);

//     DrawLineV((Vector2){(float)start_pixel.x, (float)start_pixel.y},
//               (Vector2){(float)end_pixel.x, (float)end_pixel.y},
//               (Color){colour.r, colour.g, colour.b, colour.a});
// }

// static void SyncWorldStateFromSelection(void)
// {
//     World2d *w = Universe_GetSelectedWorld(&G_Universe);
//     G_WorldState.world = w;
//     G_WorldState.entity_world_index_registry = w ? &w->entity_world_index_registry : NULL;
//     G_WorldState.collisions = w ? &w->collisions : NULL;
//     G_WorldState.selected_object = NULL;
//     G_WorldState.selected_cell = NULL;
// }

// //----------------------------------------------------------------------------------
// // Universe System Functions Definition
// //----------------------------------------------------------------------------------

// void InitUniverseSystem(void)
// {
//     // Initialize universe with independent universe coordinate space.
//     // Universe dimensions are configured in cells and are independent of panels.
//     extern Vector2d game_region_resolution;
//     extern Vector2d game_viewport_origin;
//     extern Vector2d game_viewport_u;
//     extern Vector2d game_viewport_v;
//     extern float gravity;

//     if (universe_grid_cell_size <= 0.0f)
//         universe_grid_cell_size = 1.0f;

//     Vector2d universe_resolution = {
//         (float)universe_grid_cells_x * universe_grid_cell_size,
//         (float)universe_grid_cells_y * universe_grid_cell_size};

//     // Spawn first world centered in universe so startup composition is stable.
//     Vector2d first_world_spawn = ZERO_VECTOR_2D;

//     Universe_Init(&G_Universe, first_world_spawn, (Vector2d){4, 3}, universe_resolution, gravity);
//     // Universe camera maps universe-space coordinates into viewport pixels.
//     Basis2d game_viewport_basis = (Basis2d){game_viewport_u, game_viewport_v};
//     G_Universe.camera = CreateCamera2d(game_viewport_basis, IDENTITY_BASIS_2D, ResolveGameViewportCenter(), ZERO_VECTOR_2D);
//     G_Universe.camera.source_focus_coords = ZERO_VECTOR_2D;
//     // G_Universe.camera.source_focus_coords = (Vector2d){universe_resolution.x * 0.5f, universe_resolution.y * 0.5f};
//     // The source space is universe space; the destination is the game viewport basis.
//     UpdateCameraFull(&G_Universe.camera);

//     // Create the initial world so the universe starts with one world.
//     CreateNewWorld(false);
// }

// void UpdateUniverseSystem(int mouse_x, int mouse_y)
// {
//     bool cursor_in_game_viewport = mouse_x >= game_viewport_origin.x &&
//                                    mouse_x <= (game_viewport_origin.x + (game_viewport_u.x * game_region_resolution.x)) &&
//                                    mouse_y >= game_viewport_origin.y &&
//                                    mouse_y <= (game_viewport_origin.y + (game_viewport_v.y * game_region_resolution.y));

//     UpdateUniverseInput(mouse_x, mouse_y, cursor_in_game_viewport);
// }

// void UpdateUniverseInput(int mouse_x, int mouse_y, bool cursor_in_game_viewport)
// {
//     // Universe camera controls: arrow keys for panning, Ctrl +/- for zooming
//     // Only pan/zoom when cursor is in the viewport region
//     if (cursor_in_game_viewport)
//     {
//         float wheel_move = GetMouseWheelMove();
//         Vector2d pan_delta = ZERO_VECTOR_2D;
//         if (IsKeyDown(KEY_UP))
//             pan_delta.y -= 0.5f;
//         if (IsKeyDown(KEY_DOWN))
//             pan_delta.y += 0.5f;
//         if (IsKeyDown(KEY_LEFT))
//             pan_delta.x -= 0.5f;
//         if (IsKeyDown(KEY_RIGHT))
//             pan_delta.x += 0.5f;

//         if (pan_delta.x != 0.0f || pan_delta.y != 0.0f)
//             PanCamera(&G_Universe.camera, pan_delta);

//         // Zoom with Ctrl +/-
//         // if (IsKeyDown(KEY_LEFT_CONTROL))
//         // {
//         //     if (IsKeyPressed(KEY_EQUAL))
//         //         ZoomCamera(&G_Universe.camera, 1.1f);
//         //     else if (IsKeyPressed(KEY_MINUS))
//         //         ZoomCamera(&G_Universe.camera, 1.0f / 1.1f);
//         // }
//         if (IsKeyDown(KEY_LEFT_CONTROL))
//         {
//             if (wheel_move > 0.0f)
//                 ZoomCamera(&G_Universe.camera, 1.1f);
//             else if (wheel_move < 0.0f)
//                 ZoomCamera(&G_Universe.camera, 1.0f / 1.1f);
//         }

//         if (IsKeyDown(KEY_LEFT_SHIFT))
//         {
//             if (wheel_move > 0.0f)
//                 RotateCamera(&G_Universe.camera, 0.25);
//             else if (wheel_move < 0.0f)
//                 RotateCamera(&G_Universe.camera, -0.25);
//             // if (IsKeyPressed(KEY_EQUAL))
//             //     RotateCamera(&G_Universe.camera, 0.25);
//             // else if (IsKeyPressed(KEY_MINUS))
//             //     RotateCamera(&G_Universe.camera, -0.25);
//         }
//     }
//     // Click to select/deselect worlds
//     if (IsMouseButtonPressed((int)MOUSE_BUTTON_LEFT) && cursor_in_game_viewport)
//     {
//         Vector2d click_pixel_coords = {mouse_x, mouse_y};
//         // Use universe camera to transform pixel to universe space
//         Vector2d click_universe_coords = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, click_pixel_coords);
//         bool world_hit = Universe_ResolveClick(&G_Universe, click_universe_coords, NULL);

//         // If no world was hit, deselect and reset camera offset so all worlds are visible
//         if (!world_hit)
//         {
//             G_Universe.selected_world_index = -1;
//             // G_Universe.camera_offset = ZERO_VECTOR_2D;
//         }

//         SyncWorldStateFromSelection();
//     }

//     if (IsKeyPressed(KEY_F6))
//     {
//         universe_grid_debug_labels_enabled = !universe_grid_debug_labels_enabled;
//         printf("[Universe] Grid debug labels: %s\n", universe_grid_debug_labels_enabled ? "ON" : "OFF");
//     }

//     if (IsKeyPressed(KEY_F11))
//     {
//         coordinate_debug_overlay_enabled = !coordinate_debug_overlay_enabled;
//         printf("[Debug] Coordinate overlay: %s\n", coordinate_debug_overlay_enabled ? "ON" : "OFF");
//     }

//     UpdateCameraSmoothingTick(&G_Universe.camera);
// }

// void DrawUniverse(void)
// {

//     // Draw universe grid background
//     DrawUniverseGrid();

//     Universe_Draw(&G_Universe);
//     DrawUniverseCameraMarker();
//     DrawCoordinateDebugOverlay();
// }

// void DrawCoordinateDebugOverlay(void)
// {
//     if (!coordinate_debug_overlay_enabled)
//     {
//         return;
//     }

//     int mouse_x = GetMouseX();
//     int mouse_y = GetMouseY();
//     Vector2d pixel = {(float)mouse_x, (float)mouse_y};
//     bool cursor_in_game_viewport = mouse_x >= game_viewport_origin.x && mouse_x <= game_viewport_end.x &&
//                                    mouse_y >= game_viewport_origin.y && mouse_y <= game_viewport_end.y;

//     //
//     Vector2d parent_local = TransformCoordinates(G_Universe.camera.dest_to_source_mtx, pixel);
//     int selected_index = G_Universe.selected_world_index;
//     int hovered_world_index = Universe_FindWorldAt(&G_Universe, parent_local);
//     int target_world_index = (hovered_world_index >= 0) ? hovered_world_index : selected_index;

//     bool has_child_local = false;
//     Vector2d child_origin_in_parent = ZERO_VECTOR_2D;
//     Vector2d child_local = ZERO_VECTOR_2D;
//     // Vector2d world_top_left = ZERO_VECTOR_2D;
//     int world_cell_index = -1;
//     if (target_world_index >= 0 && target_world_index < G_Universe.world_count)
//     {
//         World2d *w = &G_Universe.worlds[target_world_index];
//         Vector2d res = w->grid_space.space.resolution_ixj;
//         Matrix3x3 parent_to_child_center_mtx = w->camera.dest_to_source_mtx;


//         // Transform the un-mapped universe mouse cursor directly into centered grid space
//         has_child_local = true;
//         child_local = TransformCoordinates(parent_to_child_center_mtx, parent_local);

//         // Shift (0,0) to top-left corner
//         child_origin_in_parent.x = w->grid_space.space.local_origin.x;// + (res.x * 0.5f);
//         child_origin_in_parent.y = w->grid_space.space.local_origin.x;// + (res.y * 0.5f);

//         // Boundary check and index mapping
//         if (child_local.x >= 0.0f && child_local.y >= 0.0f &&
//             child_local.x < res.x && child_local.y < res.y)
//         {
//             int cell_x = (int)floorf(child_local.x);
//             int cell_y = (int)floorf(child_local.y);
//             world_cell_index = (cell_y * (int)res.x) + cell_x;
//         }
//         else
//         {
//             world_cell_index = -1;
//         }
//     }

//     const int panel_x = (int)game_viewport_origin.x + 6;
//     const int panel_y = (int)game_viewport_origin.y + 6;
//     const int panel_w = 660;
//     const int panel_h = 188;

//     DrawRectangle(panel_x, panel_y, panel_w, panel_h, (Color){12, 16, 24, 210});
//     DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, (Color){210, 230, 255, 180});

//     char line1[256] = {0};
//     char line2[256] = {0};
//     char line3[256] = {0};
//     char line4[256] = {0};
//     char line5[256] = {0};
//     char line6[256] = {0};

//     snprintf(line1, sizeof(line1), "Cursor px: (%.1f, %.1f) [%s]", pixel.x, pixel.y, cursor_in_game_viewport ? "in viewport" : "outside viewport");
//     snprintf(line2, sizeof(line2), "Parent coords: (%.3f, %.3f)", parent_local.x, parent_local.y);

//     if (has_child_local)
//     {
//         snprintf(line3, sizeof(line3), "Child local [world %d]: (%.3f, %.3f)", target_world_index, child_local.x, child_local.y);
//         snprintf(line4, sizeof(line4), "Child origin in parent: (%.3f, %.3f)", child_origin_in_parent.x, child_origin_in_parent.y);
//         snprintf(line5, sizeof(line5), "Child cell index: %d", world_cell_index);
//     }
//     else
//     {
//         snprintf(line3, sizeof(line3), "Child local: n/a (no selected/hovered world)");
//         snprintf(line4, sizeof(line4), "Child origin in parent: n/a");
//         snprintf(line5, sizeof(line5), "Child cell index: n/a");
//     }

//     snprintf(line6, sizeof(line6), "Selected world: %d | Hovered world: %d | Toggle: F11", selected_index, hovered_world_index);

//     Bitmap_Font overlay_font = FONT_BASIC;
//     // overlay_font.scale = 2;

//     DrawTextCustom(line1, (Vector2d){(float)panel_x + 10, (float)panel_y + 10}, overlay_font.scale,
//                    overlay_font, (ColourRgba){240, 246, 255, 230});
//     DrawTextCustom(line2, (Vector2d){(float)panel_x + 10, (float)panel_y + 38}, overlay_font.scale,
//                    overlay_font, (ColourRgba){240, 246, 255, 230});
//     DrawTextCustom(line3, (Vector2d){(float)panel_x + 10, (float)panel_y + 66}, overlay_font.scale,
//                    overlay_font, (ColourRgba){240, 246, 255, 230});
//     DrawTextCustom(line4, (Vector2d){(float)panel_x + 10, (float)panel_y + 94}, overlay_font.scale,
//                    overlay_font, (ColourRgba){190, 220, 255, 230});
//     DrawTextCustom(line5, (Vector2d){(float)panel_x + 10, (float)panel_y + 122}, overlay_font.scale,
//                    overlay_font, (ColourRgba){190, 220, 255, 230});
//     DrawTextCustom(line6, (Vector2d){(float)panel_x + 10, (float)panel_y + 150}, overlay_font.scale,
//                    overlay_font, (ColourRgba){190, 220, 255, 230});
// }

// void DrawUniverseCameraMarker(void)
// {
//     // Get the absolute world position of the camera center
//     Vector2d camera_world_pos = G_Universe.camera.source_focus_coords;

//     // Transform ONLY the single center point to screen coordinates.
//     // This perfectly handles panning, zooming, and rotation without point drift.
//     Vector2d center_screen = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, camera_world_pos);

//     // Define the marker size in SCREEN PIXELS.
//     // We adjust it by our camera zoom factor so it physically scales down/up with the world!
//     float marker_pixel_size = 32.0f * G_Universe.camera.zoom;
//     float half_size = marker_pixel_size * 0.5f;

//     // Calculate the top-left screen position
//     Vector2 position = {
//         (float)(center_screen.x - half_size),
//         (float)(center_screen.y - half_size)};

//     Vector2 size = {(float)marker_pixel_size, (float)marker_pixel_size};
//     Color color = {camera_marker_colour.r, camera_marker_colour.g, camera_marker_colour.b, camera_marker_colour.a};

//     // Draw the unwarped screen square centered on the camera position
//     DrawRectangleV(position, size, color);
// }

// void DrawUniverseGrid(void)
// {
//     ColourRgba grid_colour = {100, 100, 100, 100}; // Faint gray
//     Color axis_x_colour = (Color){230, 90, 90, 220};
//     Color axis_y_colour = (Color){90, 200, 255, 220};
//     float grid_cell_size = universe_grid_cell_size;

//     // Find the 4 corners of the game viewport in local Universe coords
//     Vector2d uni_coords[] = {TransformCoordinates(G_Universe.camera.dest_to_source_mtx, game_viewport_origin),
//                              TransformCoordinates(G_Universe.camera.dest_to_source_mtx, (Vector2d){(float)game_viewport_end.x, game_viewport_origin.y}),
//                              TransformCoordinates(G_Universe.camera.dest_to_source_mtx, (Vector2d){game_viewport_origin.x, (float)game_viewport_end.y}),
//                              TransformCoordinates(G_Universe.camera.dest_to_source_mtx, game_viewport_end)};

//     // Find the absolute bounding box of what the camera sees in the world. This safely accounts for any camera rotation
//     Matrix2x2 game_aabb = CalcAABBCoords_Tight(uni_coords, 4, ZERO_VECTOR_2D);

//     // Clamp search area to your actual universe limits so lines don't draw in the void
//     float uni_half_w = G_Universe.resolution.x * 0.5f;
//     float uni_half_h = G_Universe.resolution.y * 0.5f;

//     float world_min_x = fmaxf(game_aabb.col1.x, -uni_half_w);
//     float world_max_x = fminf(game_aabb.col2.x, uni_half_w);
//     float world_min_y = fmaxf(game_aabb.col1.y, -uni_half_h);
//     float world_max_y = fminf(game_aabb.col2.y, uni_half_h);

//     // Snap the starting points to the nearest grid step baseline
//     float start_x = floorf(world_min_x / grid_cell_size) * grid_cell_size;
//     float start_y = floorf(world_min_y / grid_cell_size) * grid_cell_size;

//     // Draw Vertical Lines (along the X axis moving across the viewport)
//     for (float x = start_x; x <= world_max_x; x += grid_cell_size)
//     {
//         // Line extends from the absolute top universe boundary to the bottom universe boundary
//         Vector2d line_start = {x, -uni_half_w};
//         Vector2d line_end = {x, uni_half_w};

//         DrawUniverseGridLine(line_start, line_end, grid_colour);
//     }

//     // Draw Horizontal Lines (along the Y axis moving down the viewport)
//     for (float y = start_y; y <= world_max_y; y += grid_cell_size)
//     {
//         Vector2d line_start = {-uni_half_w, y};
//         Vector2d line_end = {uni_half_w, y};

//         DrawUniverseGridLine(line_start, line_end, grid_colour);
//     }

//     // Draw graph-style origin axes on top of the regular grid for clear orientation.
//     Vector2d y_axis_start = {0.0f, -uni_half_h};
//     Vector2d y_axis_end = {0.0f, uni_half_h};
//     Vector2d x_axis_start = {-uni_half_w, 0.0f};
//     Vector2d x_axis_end = {uni_half_w, 0.0f};

//     Vector2d y_axis_start_px = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, y_axis_start);
//     Vector2d y_axis_end_px = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, y_axis_end);
//     Vector2d x_axis_start_px = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, x_axis_start);
//     Vector2d x_axis_end_px = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, x_axis_end);

//     DrawLineEx((Vector2){(float)x_axis_start_px.x, (float)x_axis_start_px.y},
//                (Vector2){(float)x_axis_end_px.x, (float)x_axis_end_px.y},
//                2.5f,
//                axis_x_colour);
//     DrawLineEx((Vector2){(float)y_axis_start_px.x, (float)y_axis_start_px.y},
//                (Vector2){(float)y_axis_end_px.x, (float)y_axis_end_px.y},
//                2.5f,
//                axis_y_colour);

//     // --- DEBUG LABELS SECTOR ---
//     if (!universe_grid_debug_labels_enabled)
//         return;

//     // Check if the cell size on screen is too dense to render text safely
//     Vector2d p00 = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, (Vector2d){0.0f, 0.0f});
//     Vector2d p10 = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, (Vector2d){grid_cell_size, 0.0f});
//     float cell_px_w = (float)VectorMagnitude_2d((Vector2d){p10.x - p00.x, p10.y - p00.y});
//     if (cell_px_w < 40.0f)
//         return;

//     Color text_colour = (Color){210, 210, 210, 220};

//     // Iterate through the exact localised cell blocks visible right now
//     for (float y = start_y; y < world_max_y; y += grid_cell_size)
//     {
//         for (float x = start_x; x < world_max_x; x += grid_cell_size)
//         {
//             Vector2d cell_origin = {x, y};
//             Vector2d cell_pixel = TransformCoordinates(G_Universe.camera.source_to_dest_mtx, cell_origin);

//             // True pixel-space culling bounds check that works beautifully under any rotation angle
//             if (cell_pixel.x < game_viewport_origin.x - 100 || cell_pixel.x > game_viewport_end.x + 100 ||
//                 cell_pixel.y < game_viewport_origin.y - 100 || cell_pixel.y > game_viewport_end.y + 100)
//             {
//                 continue;
//             }

//             // Calculate matching positive grid indexes relative to the Top-Left of the entire universe space
//             int ix = (int)((x + uni_half_w) / grid_cell_size);
//             int iy = (int)((y + uni_half_h) / grid_cell_size);
//             int cell_index = (iy * universe_grid_cells_x) + ix;

//             const char *display_text = TextFormat("%d\n(%.0f,%.0f)", cell_index, cell_origin.x, cell_origin.y);
//             DrawTextEx(font, display_text, (Vector2){(float)cell_pixel.x + 2, (float)cell_pixel.y + 2}, 18, 1, text_colour);
//         }
//     }
// }

// int CreateNewWorld(bool auto_select)
// {
//     Vector2d game_viewport_center = ResolveGameViewportCenter();
//     int index = Universe_CreateWorld(&G_Universe,
//                                      WHITE_RGBA,
//                                      LIGHTGRAY_RGBA,
//                                      camera_marker_colour,
//                                      G_Universe.next_spawn,
//                                      &G_WorldState,
//                                      auto_select);
//     if (index >= 0 && auto_select)
//     {
//         SyncWorldStateFromSelection();
//     }
//     return index;
// }

// bool SelectWorldByIndex(int index)
// {
//     bool ok = Universe_SelectWorld(&G_Universe, index);
//     if (ok)
//     {
//         SyncWorldStateFromSelection();
//     }
//     return ok;
// }

// bool IsCreateWorldAutoSelectEnabled(void)
// {
//     return create_world_auto_select != 0;
// }

// int *GetCreateWorldAutoSelectPtr(void)
// {
//     return &create_world_auto_select;
// }

// int GetWorldCount(void) { return Universe_GetWorldCount(&G_Universe); }
// int GetSelectedWorldIndex(void) { return Universe_GetSelectedIndex(&G_Universe); }
// World2d *GetSelectedWorld(void) { return Universe_GetSelectedWorld(&G_Universe); }
// World2d *GetWorldByIndex(int i) { return Universe_GetWorld(&G_Universe, i); }

// Vector2d *GetNextWorldSpawnOriginPtr(void) { return &G_Universe.next_spawn; }
// Vector2d *GetNextWorldResolutionPtr(void) { return &G_Universe.next_resolution; }
// Vector2d *GetNextWorldBasisUPtr(void) { return &G_Universe.next_basis_u; }
// Vector2d *GetNextWorldBasisVPtr(void) { return &G_Universe.next_basis_v; }
// float *GetNextWorldGravityPtr(void) { return &G_Universe.next_gravity; }

