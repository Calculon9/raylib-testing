// /**********************************************************************************************
//  *
//  *   raylib - Advance Game template
//  *
//  *   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
//  *
//  *   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
//  *
//  *   This software is provided "as-is", without any express or implied warranty. In no event
//  *   will the authors be held liable for any damages arising from the use of this software.
//  *
//  *   Permission is granted to anyone to use this software for any purpose, including commercial
//  *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
//  *
//  *     1. The origin of this software must not be misrepresented; you must not claim that you
//  *     wrote the original software. If you use this software in a product, an acknowledgment
//  *     in the product documentation would be appreciated but is not required.
//  *
//  *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
//  *     as being the original software.
//  *
//  *     3. This notice may not be removed or altered from any source distribution.
//  *
//  **********************************************************************************************/
// #include "raylib.h"
// #include <stdint.h>
// #include "screens.h"
// #include "system/utility_system.h"
// #include "system/systems.h"
// #include "system/ui_system.h"
// #include "physics/circloid.h"
// #include "math/cvectors.h"
// #include "physics/rectangloid.h"
// #include "physics/polygonoid.h"
// #include "physics/field.h"
// #include "common/common.h"
// #include "world/world.h"
// #include "camera/camera.h"
// #include "ui/ui.h"
// #include "ui/text_region.h"

// //----------------------------------------------------------------------------------
// // Module Variables Definition (local)
// //----------------------------------------------------------------------------------
// //

// // ------------------FRAME COUNTER-------------------------
// uint64_t total_frames = 0;
// FrameCounter frame_counter = {0};

// // ------------------TOTAL SCREEN-------------------------
// // Pixel Space Properties
// static Vector2d screen_pixel_origin = {0};
// static Vector2d screen_pixel_resolution = {1920, 1080};
// // Coordinate Space Properties
// static Vector2d screen_origin = {0};
// static Vector2d resolution = {0};
// // Logical->pixel-space conversion properties
// //int screen_resolution_scalar = 100.0; // used to divide up the pixel resolution to get a local coordinate resolution for the entire screen
// // Default font
// //static Bitmap_Font font_default = FONT_BASIC;
// static int font_scale_l = 3;
// static int font_scale_m = 2;
// static int font_scale_s = 1;
// // Default UI Properties
// static Vector2d tbox_tlabel_default_offset = {0.04, 0};
// static Vector2d tbox_default_dims = {2.5, 0.4};
// static Vector2d tbox_default_padding_outer = {0.03, 0.03};
// static Vector2d tbox_default_padding_inner = {0.02, 0.02};
// static ColourRgba tbox_default_colour_border_outer = BROWN_1_RGBA; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
// static ColourRgba tbox_default_colour_border_inner = BEIGE_RGBA; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
// static ColourRgba tbox_default_colour_fill_outer = BROWN_1_RGBA;
// static ColourRgba tbox_default_colour_fill_inner = BEIGE_RGBA;
// static Vector2d tfield_default_padding = {0.03, 0.03};
// static Vector2d tfield_default_spacing = {0, 0.05};
// static ColourRgba tfield_default_colour_fill_outer = COLOURLESS_RGBA;
// static ColourRgba tfield_default_colour_fill_inner = COLOURLESS_RGBA;
// // ----------LEFT PANEL SCREEN----------
// //  Visual Properties
// static ColourRgba lpanel_text_colour = BEIGE_RGBA;
// static ColourRgba lpanel_fill_colour = {150, 115, 70, 255};
// // Coordinate Space Properties
// static CoordSpace2d lpanel_space = {0};
// // static Vector2d lpanel_origin, lpanel_end = {0}; // Dependent on the game world screen area
// // static Vector2d lpanel_pixel_origin, lpanel_pixel_end = {0};
// // static Vector2d lpanel_u = {1, 0};
// // static Vector2d lpanel_v = {0, 1};
// // static Vector2d lpanel_resolution = {0};
// // // Logical->pixel-space conversion properties
// // static Vector2d lpanel_pixel_u = {75, 0};
// // static Vector2d lpanel_pixel_v = {0, 75};
// Camera2d camera_lpanel = {0};
// // UI Elements
// static UIElement *lpanel_properties_tcont = {0};
// static UIElement *lpanel_stats_tcont = {0};
// // static TextFieldsContainer *lpanel_properties_tcont = {0};
// // static TextFieldsContainer *lpanel_stats_tcont = {0};
// static Vector2d lpanel_properties_tcont_origin = {0, 5};
// static Vector2d lpanel_stats_tcont_origin = {0};
// // - default text container props
// static Vector2d lpanel_tcont_default_dims = {3, 5};
// static Vector2d lpanel_tcont_default_padding = {0.05, 0.05};
// // - other default text container props are same as text box
// // - default text box props

// // ----------GAME WORLD SCREEN----------
// static int finishScreen = 0;
// static Polygonoid *selectedObject = NULL; // Pointer to the currently selected object (if any) for displaying its properties in the panel
// static Cell *selectedCell = NULL;         // Pointer to the currently selected cell (if any) for displaying its properties in the panel
// // Visual Properties
// static ColourRgba world_text_colour = BROWN_1_RGBA_4; //{55, 97, 0, 200};
// static ColourRgba world_fill_colour = WHITE_RGBA;   //{48, 104, 68, 70}; // MAROON_RGBA;// {150, 255, 220,180};//DARKGREEN_RGBA;
// static ColourRgba world_line_colour = YELLOW_RGBA;  //{128, 99, 42, 100};
// // Coordinate Space Properties
// static World2d world = {0};
// static Vector2d world_origin, world_end = {0};
// static Vector2d world_pixel_origin, world_pixel_end = {0};
// static Vector2d world_u = {1, 0};
// static Vector2d world_v = {0, 1};
// static Vector2d world_resolution = {0};
// static float gravity = 10;
// // Objects and properties
// static int next_object_id = 1;           // Global variable to keep track of the next available ID for NewtonObjects
// static DynamicArray *polygonoids = NULL; // static DynamicArray *circloids = NULL;
// static ColourRgba polygonoid_line_colour = {155, 0, 0, 255};
// static ColourRgba polygonoid_text_colour = {64, 64, 64, 255};
// static float polygonoid_radius_default = 0.4;
// static float polygonoid_mass_default = 1.0;
// static Vector2d polygonoid_velocity_default = {1.40, 0.60};
// static Vector2d polygonoid_acceleration_default = {0.0, 0.0f};
// static int initObjectCount = 8;

// // Logical->pixel-space conversion properties
// // static Vector2d screen_game_origin, screen_game_end = {0};
// static Vector2d world_pixel_u = {75, 0};
// static Vector2d world_pixel_v = {0, 75};
// static Camera2d camera_world = {0};
// static float camera_world_zoom = 1.0;
// static float camera_world_rotation = 0.0;

// // ----------SCREEN WORLD (logical)----------

// // Left Side Panel Screen Properties
// // static Vector2d screen_panel_origin, screen_panel_end = {0, 0};
// // static Vector2d screen_panel_u = {75, 0};
// // static Vector2d screen_panel_v = {0, 75};
// // // Game Screen Properties
// // static Vector2d screen_game_origin, screen_game_end = {0, 0};
// // static Vector2d world_pixel_u = {75, 0};
// // static Vector2d world_pixel_v = {0, 75};
// // static ColourRgba stageBackgroundColour = {225, 225, 225, 255};

// // ----------CAMERA (logical to pixel)----------
// // Camera and Properties
// // static Camera2d camera = {0};
// // Matrix3x3 screen_basis_transform = {0};
// // static Basis2d world_basis = {0};
// // static Vector2d world_u = {1, 0};
// // static Vector2d world_v = {0, 1};

// //----------------------------------------------------------------------------------
// // Gameplay Screen Functions Definition
// //----------------------------------------------------------------------------------
// // void InitCoordinateSpaceProperties();
// // void InitPanelTextContainers();
// // // void DrawCircloids();
// // void DrawPolygonoids(Collection *polygonoids);
// // void DrawPanelRegion(CoordSpace2d panel_space, Color fill_colour);
// // void DrawPanelRegion_ObjectProps(CoordSpace2d panel_space, Color fill_colour);
// // void DrawPanelRegion_Stats(CoordSpace2d panel_space, Color fill_colour);
// // void DrawWorldRegion(World2d *world, Camera2d world_camera);
// // void DrawWorldCoordinateGrid();
// // void DrawTextFieldElements(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera);
// // void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera);
// // // int GetCircloidCount(void);

// // void UpdatePolygonoidVectors(DynamicArray *polygonoids);
// // void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
// // void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_panel);
// // // void DrawFields_Rect(void);
// // void CreateAddPolygonoid_Circle(float radius, float mass, ColourRgba colour, Vector2d origin, Vector2d velocity, Vector2d acceleration);
// // void HandleTextBoxClick(UIElement *clicked);
// // Vector2d WorldToScreenCoordinates(Matrix3x3 screen_basis_transform, Vector2d world_coordinates);


// void UpdateGameplayScreen(void)
// {
//     // Update frame counter for FPS tracking
//     // UpdateFrameCounter(&frame_counter);

//     int mouse_x = GetMouseX();
//     int mouse_y = GetMouseY();

//     // bool cursor_in_panel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) && mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));
//     bool cursor_in_world = mouse_x >= world_pixel_origin.x && mouse_x <= (world_pixel_origin.x + (world_pixel_u.x * world_resolution.x)) && mouse_y >= world_pixel_origin.y && mouse_y <= (world_pixel_origin.y + (world_pixel_v.y * world_resolution.y));

//     // UpdatePanelRegion(mouse_x, mouse_y, cursor_in_panel);
//     UpdateWorldRegion(mouse_x, mouse_y, cursor_in_world);

//     //UpdateUtilities();

//     // Press enter or tap to change to ENDING screen
//     // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
//     // {
//     //     finishScreen = 1;
//     //     PlaySound(fxCoin);
//     // }
// }


// // FIRST: Initialisation of Gameplay Screen
// void InitGameplayScreen(void)
// {
//     // 0. CALCULATE LOGICAL screen origin and end points for each region (panel, world)
//     InitCoordinateSpaceProperties();

//     // 1. INIT CAMERAS using using the resolutions, sceen basis, origins etc. from Step 0
//     // 1.1 Game world camera
//     Basis2d world_basis = (Basis2d){world_u, world_v};
//     Basis2d world_pixel_basis = (Basis2d){world_pixel_u, world_pixel_v};
//     camera_world = CreateCamera2d(world_pixel_basis, world_basis, world_pixel_origin, world_origin, camera_world_zoom, camera_world_rotation);
//     // 1.2 Panel camera
//     Basis2d lpanel_basis = (Basis2d){lpanel_u, lpanel_v};
//     Basis2d lpanel_pixel_basis = (Basis2d){lpanel_pixel_u, lpanel_pixel_v};
//     camera_lpanel = CreateCamera2d(lpanel_pixel_basis, lpanel_basis, lpanel_pixel_origin, lpanel_origin, 1, 0);

//     // 2. INIT a LOCAL COORD SPACE for the SIDE PANEL using the resolutions, origins etc. from Step 0
//     lpanel_space = NewCoordSpace2d(lpanel_origin, lpanel_resolution, lpanel_basis);

//     // 3 CREATE GAME WORLD using the resolutions, origins etc. from Step 0
//     // 3.1 Create the coordinate space for the world
//     // 3.11 Initialise Objects
//     polygonoids = NEW_DYNAMIC_ARRAY(initObjectCount, Polygonoid);
//     // 3.2 Create the space then world
//     CoordSpace2d_Grid space_g = NewCoordSpace2d_Grid(world_origin, world_resolution, world_basis, world_fill_colour, world_line_colour);
//     world = CreateWorld(space_g, *polygonoids, gravity);

//     // 4 CREATE TEST POLYGONOIDS
//     // 4.1 Static
//     // CreateAddPolygonoid_Circle(polygonoid_radius_default, polygonoid_mass_default, polygonoid_line_colour, (Vector2d){world_resolution.x / 1.70, world_resolution.y / 1.65}, polygonoid_velocity_default, polygonoid_acceleration_default);
//     // 4.2 Moving
//     // AddPolygonoid_Circle(0.4, 2.0, (Vector2d){world_resolution.x / 1.70, world_resolution.y / 1.65}, (Vector2d){0.0f, 0.3f}, (Vector2d){0.0f, 0.0f});

//     // 5. Initialise UI elements (text boxes, etc.) using the coordinate space properties from Step 0
//     //InitPanelTextContainers();
//     InitPanelSpace();
//     // text_boxes = NEW_DYNAMIC_ARRAY(8, TextBox);
//     // 5.1 Create text box for displaying object properties in the panel, using the panel coordinate space properties from Step 0
//     // lpanel_properties_tbox = CreateTextBox(lpanel_tbox_default_dims.x, lpanel_tbox_default_dims.y, lpanel_properties_tbox_coords, lpanel_tbox_default_padding_inner, lpanel_tbox_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);

//     // Initialise utilities (FPS tracking, etc.)
//     InitUtilities();

//     // framesCounter = 0;
//     finishScreen = 0;
// }

// // void InitPanelTextContainers()
// // {
// //     // PROPERTIES Text Container and its Text Boxes for the panel

// //     lpanel_properties_tcont = CreateTextFieldContainer(NULL, lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, lpanel_properties_tcont_origin, lpanel_properties_tcont_origin, lpanel_tcont_default_padding, tfield_default_spacing, tbox_default_colour_border_outer, tbox_default_colour_fill_inner);
// //     // lpanel_properties_tcont = CreateTextFieldContainer(lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, lpanel_properties_tcont_origin, lpanel_tcont_default_padding_inner, lpanel_tcont_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);
// //     Vector2d text_field_spacing = (Vector2d){0.05, 0.05};

// //     // This will be the initial origin for the first text field in the properties container, and the rest will be positioned relative to this one using the text_field_spacing property of the container
// //     // Just set to container origin coords {0,0} for now and will update in the Draw loop based on the actual position and dimensions of the container, so that it works even if we change the container's properties later
// //     // char *tbox_labels[] = {"OBJECT PROPERTIES"};
// //     char *tbox_labels[] = {"OBJECT PROPERTIES", "MASS.", "POS.(X,Y)", "VEL.(X,Y)", "ACCEL.(X,Y)"};
// //     Vector2d tf_initial_origin = (Vector2d){lpanel_properties_tcont_origin.x, lpanel_properties_tcont_origin.y};
// //     for (int i = 0; i < 5; i++)
// //     {
// //         // Create TextField - they will be stacked so the offset between each TextField is the spacing + height
// //         Vector2d offset_y = (Vector2d){0, tbox_default_dims.y};
// //         Vector2d tf_offset = VectorSum_2d(VectorScale_2d(tfield_default_spacing, i + 1), VectorScale_2d(offset_y, i));
// //         UIElement *tfield = CreateTextField(lpanel_properties_tcont, tbox_default_dims.x, tbox_default_dims.y, tf_initial_origin, tf_offset, tfield_default_padding, tbox_tlabel_default_offset, tbox_default_padding_inner, COLOURLESS_RGBA, COLOURLESS_RGBA, MAX_LABEL_CHARS, MAX_TEXTBOX_CHARS);
// //         tfield->parent = lpanel_properties_tcont;

// //         // Customise the TextField's TextBox and calculate the TextBox's origin based on the TextField's origin and the TextBox's padding, so that the text box is positioned correctly within the field
// //         UIElement *children = (UIElement *)tfield->children.items;
// //         children[0].padding = tbox_default_padding_outer;
// //         children[0].colour_border = tbox_default_colour_border_outer;
// //         children[1].padding = tbox_default_padding_outer;
// //         children[1].colour_border = tbox_default_colour_border_outer;

// //         strncpy(children[0].data.textbox.text, tbox_labels[i], MAX_LABEL_CHARS - 1);
// //         LArray_Push(&lpanel_properties_tcont->children, tfield);

// //         // tfield->text_box.padding = tbox_default_padding_outer;
// //         // tfield->text_box.colour_border = tbox_default_colour_border_outer;

// //         // tfield->label.colour_fill = tbox_default_colour_fill_outer;
// //         // tfield->label.padding = tbox_default_padding_outer;
// //         // tfield->label.colour_border = tbox_default_colour_border_outer;
// //     }

// //     // lpanel_stats_tcont = CreateTextFieldContainer(lpanel_tcont_default_dims.x, lpanel_tcont_default_dims.y, lpanel_stats_text_container_origin, lpanel_tcont_default_padding_inner, lpanel_tcont_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);
// // }

// // Calculates initial coordinates in local coordinate space, screen/pixel space basis's, resolutoins, from local resolution and desired world->pixel scaling.
// void InitCoordinateSpaceProperties()
// {
//     // 0 CALCULATE LOGICAL/LOCAL resolution from screen's pixel resolution
//     resolution = VectorScale_2d(screen_pixel_resolution, 1 / screen_resolution_scalar);
//     resolution.x = floorf(resolution.x);
//     resolution.y = floorf(resolution.y);
//     float total_space_measure = resolution.x * resolution.y;

//     // 1. DEFINE & CALCULATE LOGICAL screen origin and end points for each region (panel, world)
//     // 1.1 Give the panel ~1/4 of the x-dimension, and always 100% y-dimension
//     lpanel_origin = screen_origin;
//     lpanel_end.x = floorf(lpanel_origin.x + ((1.0f / 4.0f) * resolution.x));
//     lpanel_end.y = resolution.y;
//     lpanel_resolution = VectorSum_2d(VectorScale_2d(lpanel_origin, -1), lpanel_end);
//     float lpanel_space_measure = VectorBox_2d(lpanel_resolution);

//     // 1.2 The game screen simply takes up the rest of the screen
//     world_resolution = (Vector2d){resolution.x - lpanel_resolution.x, resolution.y};
//     world_origin = (Vector2d){lpanel_end.x, screen_origin.y};
//     world_end = (Vector2d){world_origin.x + world_resolution.x, world_origin.y + world_resolution.y};
//     float world_space_measure = VectorBox_2d(world_resolution);

//     // 2. BACK-CALCULATE SCREEN PIXEL SPACE Basis for panel and game world
//     // 2.1 Influence of resolution scaling
//     world_pixel_u = VectorScale_2d(world_u, screen_resolution_scalar);
//     world_pixel_v = VectorScale_2d(world_v, screen_resolution_scalar);
//     lpanel_pixel_u = VectorScale_2d(lpanel_u, screen_resolution_scalar);
//     lpanel_pixel_v = VectorScale_2d(lpanel_v, screen_resolution_scalar);

//     // 3. CALCULATE SCREEN PIXEL SPACE origins for each region (panel, world)
//     lpanel_pixel_origin.x = (lpanel_pixel_u.x + lpanel_pixel_v.x) * lpanel_origin.x;
//     lpanel_pixel_origin.y = (lpanel_pixel_u.y + lpanel_pixel_v.y) * lpanel_origin.y;
//     world_pixel_origin.x = (world_pixel_u.x + world_pixel_v.x) * world_origin.x;
//     world_pixel_origin.y = (world_pixel_u.y + world_pixel_v.y) * world_origin.y;

//     // DEBUG - the sum of panel and world resolutions should equal the overall resolution from Step 0
//     float resolution_recalc_x = lpanel_resolution.x + world_resolution.x;
//     float resolution_recalc_y = lpanel_resolution.y + world_resolution.y;
//     float res_recalc_measure = resolution_recalc_x * resolution_recalc_y; //world_space_measure + lpanel_space_measure;
    
//     printf("LOCAL RESOLUTIONS --> TOTAL_LOCAL(%0.1f)(%0.1f,%0.1f); PANEL_LOCAL(%0.1f)(%0.1f,%0.1f); WORLD_LOCAL(%0.1f)(%0.1f,%0.1f); TOTAL_LOCAL_RECALC_MEASURE(%0.1f);\n", total_space_measure, resolution.x, resolution.y, lpanel_space_measure, lpanel_resolution.x, lpanel_resolution.y, world_space_measure, world_resolution.x, world_resolution.y, res_recalc_measure);
//     printf("PIXEL ORIGINS --> PANEL(%0.1f,%0.1f); GAME_WORLD (%0.1f,%0.1f);\n", lpanel_pixel_origin.x, lpanel_pixel_origin.y, world_pixel_origin.x, world_pixel_origin.y);

//     // Create UI
//     // text_box_props_coords = CreateTextBox(0.2, 0.1, (Vector2d){0.2, 6}, (Vector2d){0.02, 0.02}, (Vector2d){0.01, 0.01}, (ColourRgba){128, 99, 42, 100}, (ColourRgba)GRAY_RGBA);
//     // text_box_props_coords.text[0] = '\0';
//     // // Put some text in the text box for testing
//     // strncpy(text_box_props_coords.text, "Object Properties", sizeof(text_box_props_coords.text) - 1);
// }

// //-

// // Gameplay Screen Update logic
// void UpdateGameplayScreen(void)
// {
//     // Update frame counter for FPS tracking
//     total_frames++;
//     UpdateFrameCounter(&frame_counter);

//     int mouse_x = GetMouseX();
//     int mouse_y = GetMouseY();

//     bool cursor_in_panel = mouse_x >= lpanel_pixel_origin.x && mouse_x <= (lpanel_pixel_origin.x + (lpanel_pixel_u.x * lpanel_resolution.x)) && mouse_y >= lpanel_pixel_origin.y && mouse_y <= (lpanel_pixel_origin.y + (lpanel_pixel_v.y * lpanel_resolution.y));
//     bool cursor_in_world = mouse_x >= world_pixel_origin.x && mouse_x <= (world_pixel_origin.x + (world_pixel_u.x * world_resolution.x)) && mouse_y >= world_pixel_origin.y && mouse_y <= (world_pixel_origin.y + (world_pixel_v.y * world_resolution.y));

//     UpdatePanelRegion(mouse_x, mouse_y, cursor_in_panel);
//     UpdateWorldRegion(mouse_x, mouse_y, cursor_in_world);

//     UpdateUtilities();

//     // Press enter or tap to change to ENDING screen
//     // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
//     // {
//     //     finishScreen = 1;
//     //     PlaySound(fxCoin);
//     // }
// }

// // Gameplay Screen Stage Update logic
// // void UpdatePanelRegion(int mouse_x, int mouse_y, bool cursor_in_region)
// // {
// //     // Check if something was clicked on the panel
// //     if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
// //     {
// //         return;
// //     }
// //     if (!cursor_in_region)
// //     {
// //         return;
// //     }

// //     char log[256] = "";
// //     int offset = 0;

// //     // Convert to local panel coordinates
// //     Vector2d click_pixel_coords = {mouse_x, mouse_y};
// //     Vector2d click_panel_coords = TransformCoordinates(camera_lpanel.dest_to_source_mtx, click_pixel_coords);

// //     int cell_index = ((int)click_panel_coords.y * (int)lpanel_resolution.x) + (int)click_panel_coords.x;

// //     // Check if there are any objects in that cell and print info about those objects if so
// //     Cell *cells = lpanel_space.cells.coll.items;
// //     Cell cell = cells[cell_index];
// //     // selectedCell = &cells[cell_index];
// //     offset += snprintf(log + offset, sizeof(log) - offset, "Region: Panel (%.1f, %.1f) --> Cell %d (%.1f, %.1f) --> ", lpanel_pixel_origin.x, lpanel_pixel_origin.y, cell_index, cell.coords.x, cell.coords.y);

// //     // Loop through all properties text boxes and apply focus if the clicked_coords are within a text_box, otherwise set focus --> false
// //     UIElement **tfields = (UIElement **)lpanel_properties_tcont->children.items; //.coll;
// //     UIElement *t = NULL;
// //     int tfield_count = lpanel_properties_tcont->children.count;
// //     Vector2d vertices[4] = {0};
// //     bool any_focus = false;
// //     if (lpanel_properties_tcont->children.count > 0)
// //     {
// //         for (int i = 0; i < tfield_count; i++)
// //         {
// //             // Check if the click is within the text box
// //             t = (UIElement *)(tfields[i]);
// //             GetUIElementVertices(t, vertices); // ISSUE HERE
// //             if (IsFocused(click_panel_coords, vertices, 4))
// //             {
// //                 // Apply focus on text box
// //                 HandleTextBoxClick(t);
// //                 any_focus = true;
// //                 offset += snprintf(log + offset, sizeof(log) - offset, "Element: TextBox (%.1f, %.1f)", click_panel_coords.x, click_panel_coords.y);
// //                 break;
// //                 // printf("Clicked on object properties text box! Text box properties - Position: (%.1f, %.1f), Dimensions: (%.1f, %.1f)\n", lpanel_properties_tbox.origin.x, lpanel_properties_tbox.origin.y, lpanel_properties_tbox.width, lpanel_properties_tbox.height);
// //             }
// //         }
// //     }
// //     // If nothing in Properties was clicked, loop through all stats text boxes and apply focus if the clicked_coords are within a text_box, otherwise set focus --> false
// //     // if (!any_focus && lpanel_stats_tcont->text_fields.coll.count > 0)
// //     // {
// //     //     tbox_coll = (Collection)lpanel_stats_tcont->text_fields.coll;

// //     //     t = &((TextField *)(tbox_coll.items))[0];
// //     //     vertices = GetTextFieldVertices(*t);
// //     //     bool any_focus = false;
// //     //     for (int i = 0; i < tbox_coll.count; i++)
// //     //     {
// //     //         // Check if the click is within the text box
// //     //         t = &((TextField *)(tbox_coll.items))[i];
// //     //         vertices = GetTextFieldVertices(*t);
// //     //         if (IsFocused(click_panel_coords, vertices, 4))
// //     //         {
// //     //             // Apply focus on text box
// //     //             HandleTextFieldClick(t);
// //     //             // t->is_focused = true;
// //     //             any_focus = true;
// //     //             offset += snprintf(log + offset, sizeof(log) - offset, "Element: TextBox (%.1f, %.1f)", click_panel_coords.x, click_panel_coords.y);
// //     //             break;
// //     //             // printf("Clicked on object properties text box! Text box properties - Position: (%.1f, %.1f), Dimensions: (%.1f, %.1f)\n", lpanel_properties_tbox.origin.x, lpanel_properties_tbox.origin.y, lpanel_properties_tbox.width, lpanel_properties_tbox.height);
// //     //         }
// //     //     }
// //     // }
// //     if (!any_focus)
// //     {
// //         offset += snprintf(log + offset, sizeof(log) - offset, "Element: Nill");
// //     }

// //     printf("CLICKED (%d, %d) { %s }\n", mouse_x, mouse_x, log);

// //     finishScreen = 1;
// //     // PlaySound(fxCoin);
// // }

// // Gameplay Screen Stage Update logic
// void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region)
// {

//     // Update vectors of all objects
//     // DEBUGGING - we will update object vectors if button is pressed
//     bool keyDown = IsKeyDown(KEY_LEFT_CONTROL);
//     if (keyDown)
//     {
//         UpdateWorld(&world, frame_counter.delta_time);
//         // UpdatePolygonoidVectors(polygonoids);
//         // UpdateWorldPhysics(*polygonoids);
//     }

//     // Update Fields

//     // Draw a circle where the mouse clicks and add it to the state
//     if (IsKeyPressed(KEY_UP) && cursor_in_region)
//     {
//         // 1. Add a new polygonoid to the state with the position of the mouse click - give an initial velocity
//         // 1.1 Convert mouse pixel coords to world coords
//         Vector2d click_pixel_coords = {mouse_x, mouse_y};
//         Vector2d click_world_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords);

//         float radius = polygonoid_radius_default;
//         float mass = polygonoid_mass_default;
//         Vector2d velocity = polygonoid_velocity_default;
//         Vector2d acceleration = polygonoid_acceleration_default;
//         ColourRgba colour = polygonoid_line_colour;
//         CreateAddPolygonoid_Circle(radius, mass, colour, click_world_coords, velocity, acceleration);

//         finishScreen = 1;
//         PlaySound(fxCoin);
//     }

//     if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
//     {
//         return;
//     }
//     if (!cursor_in_region)
//     {
//         return;
//     }

//     char log[256] = "";
//     int offset = 0;
//     offset += snprintf(log + offset, sizeof(log) - offset, "Region: World (%.1f, %.1f) --> ", world_origin.x, world_origin.y);

//     // Check if a click is on an object and print some info about that object if so
//     Vector2d click_pixel_coords = {mouse_x, mouse_y};                                                        // This is in pixel coordinates relative to the top left of the screen, so we need to convert it to world coordinates before we can compare it to object coordinates which are relative to the world origin
//     Vector2d click_world_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords); // This is relative to the world origin, so we can use it directly to compare to object coordinates which are also relative to the world origin

//     Polygonoid *polygonoid_coll = polygonoids->coll.items;
//     int cell_index = ((int)click_world_coords.y * (int)world_resolution.x) + (int)click_world_coords.x;

//     // Check if there are any objects in that cell and print info about those objects if so
//     Cell *cells = world.coord_space_grid.coord_space.cells.coll.items;
//     Cell cell = cells[cell_index];
//     selectedCell = &cells[cell_index];

//     offset += snprintf(log + offset, sizeof(log) - offset, "Element: Cell %d (%.1f, %.1f), Occ. %d, Val. %.1f  --> ", cell_index, cell.coords.x, cell.coords.y, cell.occupancy, cell.value);

//     // Check World objects for the object with the same ID as the one in the cell and print its properties if found
//     Polygonoid *objs = (Polygonoid *)world.objects.coll.items;
//     Vector2d click_to_cell_dist = VectorSum_2d(VectorScale_2d(cell.coords, -1), click_world_coords);
//     float click_to_cell_mag = fabs(VectorMagnitude_2d(click_to_cell_dist));
//     Polygonoid *p_closest = NULL;
//     for (int i = 0; i < MAX_CELL_OCCUPANCY; i++)
//     {
//         int id = cell.object_ids[i];
//         if (id == 0)
//         {
//             // No object Id  at this index yet (still at initialised value).
//             continue;
//         }
//         if (id < 0)
//         {
//             printf("ERROR. Object Id stored in Cell is < 0 (%.1f)\n", id);
//             continue;
//         }

//         Polygonoid *p = &objs[id - 1]; // Minus 1 because the Id starts at 1, not 0;

//         if (p->id == cell.object_ids[i])
//         {
//             Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(p->newtonian_properties.coords_origin, -1), click_world_coords);
//             float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
//             p_closest = click_to_obj_mag < click_to_cell_mag ? p : p_closest;
//             // Check if the object's distance to the clicked coords is smaller than the previous and overwrite if so
//             offset += snprintf(log + offset, sizeof(log) - offset, "Object %d: ID = %d (%.1f, %.1f), ", i + 1, p->id, p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y);
//         }
//         else
//         {
//             printf("ERROR. Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. Id in Cell = %d. Id in Object = %d.\n", id, p->id);
//             continue;
//         }

//         // for (size_t j = 0; j < world.objects.coll.count; j++)
//         // {
//         //     Polygonoid *p = &objs[j];

//         //     if (p->id == cell.object_ids[i])
//         //     {
//         //         Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(p->newtonian_properties.coords_origin, -1), click_world_coords);
//         //         float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
//         //         p_closest = click_to_obj_mag < click_to_cell_mag ? p : p_closest;
//         //         // Check if the object's distance to the clicked coords is smaller than the previous and overwrite if so
//         //         offset += snprintf(log + offset, sizeof(log) - offset, "Object %d: ID = %d (%.1f, %.1f), ", i + 1, p->id, p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y);

//         //         // printf("Object with ID %d is in the cell. Object properties - Position: (%.1f, %.1f), Velocity: (%.1f, %.1f), Acceleration: (%.1f, %.1f)\n",
//         //         //        p->newtonian_properties.id,
//         //         //        p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y,
//         //         //        p->newtonian_properties.velocity.x, p->newtonian_properties.velocity.y,
//         //         //        p->newtonian_properties.acceleration.x, p->newtonian_properties.acceleration.y);
//         //     }
//         // }
//     }
//     selectedObject = p_closest; // Set the selected object to the one in the cell that was clicked, so that its properties can be displayed in the panel
//     if (!p_closest)
//     {
//         offset += snprintf(log + offset, sizeof(log) - offset, "Object: Nill");
//     }
//     printf("CLICKED (%d, %d) { %s }\n", mouse_x, mouse_x, log);
// }

// void CreateAddPolygonoid_Circle(float radius, float mass, ColourRgba colour, Vector2d origin, Vector2d velocity, Vector2d acceleration)
// {
//     Polygonoid newPolygonoid = CreatePolygonoid_Symmetric(12, radius, colour, mass, origin, velocity, acceleration);

//     AddObjectToWorld(&world, &newPolygonoid);

//     // Array_Push(polygonoids, &newPolygonoid);
// }

// // Gameplay Screen Draw logic
// void DrawGameplayScreen()
// {
//     // 1. Draw the side panel
//     DrawPanelRegion(lpanel_space, (Color){lpanel_fill_colour.r, lpanel_fill_colour.g, lpanel_fill_colour.b, lpanel_fill_colour.a});

//     // 2. Draw the game world
//     DrawWorldRegion(&world, camera_world);
//     // DrawWorldRegion(panelWidth, 0, stageWidth, stageHeight, (Color){stageBackgroundColour.r, stageBackgroundColour.g, stageBackgroundColour.b, stageBackgroundColour.a});
// }

// // Draws Left Panel
// // void DrawPanelRegion(CoordSpace2d panel_space, Color fill_colour)
// // {
// //     // Need to convert world coordinates to screen coordinates
// //     Basis2d basis = panel_space.basis;

// //     // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
// //     // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
// //     Vector2d origin = panel_space.coords_origin;
// //     Vector2d end = VectorSum_2d(origin, panel_space.resolution_ixj);

// //     // Transform local space position to pixel space
// //     Vector2d pixel_origin = TransformCoordinates(camera_lpanel.source_to_dest_mtx, origin);
// //     Vector2d pixel_end = TransformCoordinates(camera_lpanel.source_to_dest_mtx, end);

// //     // First: Draw background
// //     ColourRgba colour_fill = world.coord_space_grid.colour_fill;
// //     ColourRgba colour_line = world.coord_space_grid.colour_line;
// //     DrawRectangle(pixel_origin.x, pixel_origin.y, abs(pixel_end.x - pixel_origin.x), abs(pixel_end.y - pixel_origin.y), fill_colour);

// //     // DrawPanelRegion_Stats(panel_space, fill_colour);
// //     //  DrawPanelRegion_ObjectProps(panel_space, fill_colour);

// //     // Draw the text boxes in the Properties container
// //     DrawTextFieldsContainer(lpanel_properties_tcont, camera_lpanel);

// //     // Draw the text boxes in the Stats container
// //     // DrawTextFieldsContainer(&lpanel_stats_tcont, camera_lpanel);

// //     // Memory display - Consumed memory in bytes out of the total allocated bytes
// //     // snprintf(text, sizeof(text), "Memory Consumed (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
// //     // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);
// // }

// // void DrawPanelRegion_Stats(CoordSpace2d panel_space, Color fill_colour)
// // {
// //     Vector2 pos = {20, 100};
// //     Vector2 lineSpacing = {0, 40};

// //     // Count display
// //     char text[32];
// //     snprintf(text, sizeof(text), "Polygonoids: %d", GetPolygonoidCount());                                                                                 // Format the FPS value into the buffer
// //     DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text
// //     // snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
// //     // DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

// //     // FPS display
// //     snprintf(text, sizeof(text), "FPS: %.1f", frame_counter.fps); // Format the FPS value into the buffer
// //     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

// //     // Memory display - Total allocated memory in bytes
// //     snprintf(text, sizeof(text), "Memory (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
// //     DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});
// // }

// // void DrawPanelRegion_ObjectProps(CoordSpace2d panel_space, Color fill_colour)
// //{
// // Vector2 pos = {20, 250};
// // Vector2 lineSpacing = {0, 40};

// // DrawTextFieldsContainer(lpanel_properties_tcont, camera_lpanel);

// // Draw the Properties text box container and its text boxes

// // Title display
// // DrawTextField(&lpanel_properties_tbox, font_default, font_scale_m, camera_lpanel);
// // DrawTextBox(&lpanel_properties_tbox, font_default, 1, camera_lpanel);
// //  char text[32];
// //  snprintf(text, sizeof(text), "Polygonoid Properties");
// //  DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text

// // // Coords display
// // snprintf(text, sizeof(text), "Id. %d", selectedObject != NULL ? selectedObject->id : 0);
// // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 1.5f, 1.5, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a}); // Buffer to hold the text

// // // Coords display
// // snprintf(text, sizeof(text), "Coords. {%.1f, %.1f}", selectedObject != NULL ? selectedObject->newtonian_properties.coords_origin.x : 0, selectedObject != NULL ? selectedObject->newtonian_properties.coords_origin.y : 0);
// // DrawTextEx(font, text, (Vector2){pos.x + 2 * lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 1.5f, 1.5, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

// // // Velocity display
// // snprintf(text, sizeof(text), "Vel. {%.1f, %.1f}", selectedObject != NULL ? selectedObject->newtonian_properties.velocity.x : 0, selectedObject != NULL ? selectedObject->newtonian_properties.velocity.y : 0);
// // DrawTextEx(font, text, (Vector2){pos.x + 3 * lineSpacing.x, pos.y + 3 * lineSpacing.y}, font.baseSize * 1.5f, 1.5, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});

// // // Acceleration display
// // snprintf(text, sizeof(text), "Accel. {%.1f, %.1f}", selectedObject != NULL ? selectedObject->newtonian_properties.acceleration.x : 0, selectedObject != NULL ? selectedObject->newtonian_properties.acceleration.y : 0);
// // DrawTextEx(font, text, (Vector2){pos.x + 4 * lineSpacing.x, pos.y + 4 * lineSpacing.y}, font.baseSize * 1.5f, 1.5, (Color){lpanel_text_colour.r, lpanel_text_colour.g, lpanel_text_colour.b, lpanel_text_colour.a});
// //}

// // void HandleTextBoxClick(UIElement *clicked)
// // {
// //     // 1. "Bubble up" to parent
// //     UIElement *p = clicked->parent;

// //     if (p == NULL)
// //         return;
// //     LArray siblings = p->children; //->text_fields.coll.items;
// //     int sibling_count = p->children.count;

// //     // 2. Access siblings via parent's list
// //     for (int i = 0; i < sibling_count; i++)
// //     {
// //         UIElement *sibling = ((UIElement **)siblings.items)[i];
// //         UIElement *sibling_tbox = sibling->children.count == 2 ? ((UIElement **)sibling->children.items)[0] : NULL; // Assuming the TextBox is the first child of the TextField

// //         if (sibling == clicked)
// //         {
// //             continue; // Skip the one we clicked
// //         }

// //         // 3. Do something to the siblings (e.g., deselect them)
// //         sibling_tbox->is_focused = false;
// //     }

// //     clicked->is_focused = true;
// // }

// void DrawWorldRegion(World2d *world, Camera2d camera)
// {
//     // Draw the world's coordinate space
//     // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
//     DrawWorldCoordinateGrid();
//     // Draw objects in the world (circloids, polygonoids, etc.)
//     DrawPolygonoids(&world->objects.coll);
// }

// void DrawWorldCoordinateGrid()
// {
//     if (!world.coord_space_grid.coord_space.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
//     {
//         return; // No field to draw
//     }

//     // Need to convert world coordinates to screen coordinates
//     Basis2d basis = world.coord_space_grid.coord_space.basis;

//     // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
//     // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
//     Vector2d origin = world.coord_space_grid.coord_space.coords_origin;
//     Vector2d end = VectorSum_2d(origin, world.coord_space_grid.coord_space.resolution_ixj);

//     // Transform local space position to pixel space
//     Vector2d world_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, origin);
//     Vector2d world_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, end);

//     // First: Draw background
//     ColourRgba colour_fill = world.coord_space_grid.colour_fill;
//     ColourRgba colour_line = world.coord_space_grid.colour_line;
//     DrawRectangle(world_pixel_origin.x,
//                   world_pixel_origin.y,
//                   fabsf(world_pixel_end.x - world_pixel_origin.x),
//                   fabsf(world_pixel_end.y - world_pixel_origin.y),
//                   (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

//     // Need to know how the unit steps to take in each direction
//     int stepsU = world.coord_space_grid.coord_space.stepsU; // ceilf((float)world_space.resolution_ixj.x / VectorMagnitude_2d(basis.u));
//     int stepsV = world.coord_space_grid.coord_space.stepsV; // VectorMagnitude_2d(basis.v));

//     // 1. Draw "Horizontal-ish" lines (along the U direction)
//     // Create a line at every 'v' step that spans the entire 'u' width
//     ColourRgba colour = world.coord_space_grid.colour_line;
//     Vector2d line_origin, line_end = {0};
//     Vector2d line_pixel_origin, line_pixel_end = {0};
//     for (int j = 0; j <= stepsV; j++)
//     {
//         // Define the line in LOCAL coordinates (simple units)
//         // Line i starts at (i, 0) and goes to (i, stepsV)
//         line_origin = (Vector2d){origin.x, (float)j};
//         line_end = (Vector2d){(float)stepsU, (float)j};

//         // The Matrix handles everything:
//         // It applies World Position (Origin), Rotation, and Scale in one go.
//         line_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, line_origin);
//         line_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, line_end);
//         // Vector2d screenStart = WorldToScreenCoordinates(screen_basis_transform, localStart);
//         // Vector2d screenEnd = WorldToScreenCoordinates(screen_basis_transform, localEnd);

//         // Draw
//         DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
//                   (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
//     }

//     // 2. Draw "Vertical-ish" lines (along the V direction)
//     // We create a line at every 'u' step that spans the entire 'v' height
//     for (int i = 0; i <= stepsU; i++)
//     {
//         // Define the line in LOCAL coordinates (simple units)
//         // Line i starts at (i, 0) and goes to (i, stepsV)
//         line_origin = (Vector2d){(float)i, origin.y};
//         line_end = (Vector2d){(float)i, (float)stepsV};

//         // The Matrix handles everything:
//         // It applies World Position (Origin), Rotation, and Scale in one go.
//         Vector2d line_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, line_origin);
//         Vector2d line_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, line_end);
//         // Vector2d localStart = {(double)i, 0.0};
//         // Vector2d localEnd = {(double)i, (double)stepsV};

//         // Draw
//         DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
//                   (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
//     }

//     // Draw values as text on top of each field unit
//     int totalUnits = stepsU * stepsV; // (int)ceilf(totalArea / cellArea);
//     Collection *cells = &(world.coord_space_grid.coord_space.cells.coll);

//     for (int k = 0; k < totalUnits; k++)
//     {
//         int i = k / stepsU; // Row index (based on horizontal lines)
//         int j = k % stepsU; // Column index (based on vertical lines)
//         Cell *cell = (Cell *)((char *)cells->items + (k * cells->elemSize));
//         Vector2d cell_coords = cell->coords;
//         Vector2d cell_pixel_coords = TransformCoordinates(camera_world.source_to_dest_mtx, cell_coords);
//         const char *displayText = TextFormat(" %d (%d,%d)\n (%.0f,%.0f)\n", k, i, j, cell_pixel_coords.x, cell_pixel_coords.y);
//         // const char *displayText = TextFormat(" %d (%d,%d)\n (%0.0f,%0.0f)\n", k + 1, i + 1, j + 1, cell_pixel_coords.x, cell_pixel_coords.y);
//         //  const char *displayText = TextFormat("Cell: %d (%d,%d)\nWorldCoord: (%d,%d)\nScreenCoord: (%d,%d)\nValue: %.1f", k + 1, i + 1, j + 1, (int)cell_world_coords.x, (int)cell_world_coords.y, cell->value);
//         //   DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
//         DrawTextEx(font, displayText, (Vector2){cell_pixel_coords.x, cell_pixel_coords.y}, 16, 1, (Color){world_text_colour.r, world_text_colour.g, world_text_colour.b, world_text_colour.a});

//         // Debug print
//         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
//     }
//     // printf("Drew %d cells\n", count);
// }

// void DrawPolygonoids(Collection *polygonoids)
// {
//     if (polygonoids == NULL)
//     {
//         return; // Nothing to draw
//     }
//     // Collection *coll = &polygonoids->coll;
//     for (int i = 0; i < polygonoids->count; i++)
//     {
//         Polygonoid polygonoid = *((Polygonoid *)((char *)polygonoids->items + (i * polygonoids->elemSize)));
//         Vector2d origin_relto_world = polygonoid.newtonian_properties.coords_origin;
//         Vector2d basis_scale = BasisTransform_2d_Scale(camera_world.source_basis, camera_world.destination_basis);

//         // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

//         // Draw polygonoid THEN text so text is on top
//         // Get origin-offset coordinates as they are only relative vectors with no origin offset
//         Collection surf_vectors = polygonoid.newtonian_properties.surface.surface_vectors.coll;
//         Vector2d vertice_start = *((Vector2d *)surf_vectors.items);
//         vertice_start = VectorSum_2d(vertice_start, origin_relto_world);
//         vertice_start = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_start);
//         Vector2d vertice_start_cache = vertice_start;
//         for (int j = 1; j < surf_vectors.count; j++)
//         {
//             Vector2d vertice_end = *(Vector2d *)((char *)surf_vectors.items + (j * sizeof(Vector2d)));
//             vertice_end = VectorSum_2d(vertice_end, origin_relto_world);
//             vertice_end = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_end);
//             DrawLine(vertice_start.x, vertice_start.y, vertice_end.x, vertice_end.y, (Color){polygonoid_line_colour.r, polygonoid_line_colour.g, polygonoid_line_colour.b, polygonoid_line_colour.a});

//             // Current end vertice is used as the starting vertice for the next line, so recycle it
//             vertice_start = vertice_end;
//         }

//         // Draw the line from vertice[0] to vertice[count-1];
//         DrawLine(vertice_start.x, vertice_start.y, vertice_start_cache.x, vertice_start_cache.y, (Color){polygonoid_line_colour.r, polygonoid_line_colour.g, polygonoid_line_colour.b, polygonoid_line_colour.a});
//         float cell_index = ((int)origin_relto_world.y * world_resolution.x) + (int)origin_relto_world.x; // - 1; ((screen_origin.x - 1) * world_resolution.y) + screen_origin.y;
//         const char *display_text = TextFormat("%.0f", cell_index);
//         Vector2d pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, origin_relto_world);
//         float radius_mag_pixel = VectorMagnitude_2d(basis_scale) * polygonoid.radius; // Assuming orthogonal coordinatea
//         Vector2d text_pixel_cords = {pixel_origin.x - (0.6 * radius_mag_pixel), pixel_origin.y - (0.3 * radius_mag_pixel)};
//         DrawTextEx(font, display_text, (Vector2){text_pixel_cords.x, text_pixel_cords.y}, 20, 1, (Color){polygonoid_text_colour.r, polygonoid_text_colour.g, polygonoid_text_colour.b, polygonoid_text_colour.a});

//         // Debug print
//         // printf("Drew Polygonoid %d at Coords (%.1f, %.1f), Pixel (%.1f, %.1f)\n", i, screen_origin.x, screen_origin.y, pixel_origin.x, pixel_origin.y);
//     }
// }

// void UpdatePolygonoidVectors(DynamicArray *polygonoids)
// {
//     // Update Polygonoids
//     if (polygonoids == NULL || polygonoids->coll.count <= 0)
//     {
//         return; // Nothing to update
//     }
//     Circloid *p = Enumerate(&polygonoids->coll);
//     if (p == NULL)
//     {
//         fprintf(stderr, "Failed to retrieve enumerated Polygonoid\n"); // Enumerator failed to retrieve the first item
//     }
//     while (p != NULL)
//     {
//         if (&p->newtonian_properties != NULL)
//         {
//             CalculateVectors(&p->newtonian_properties, frame_counter.delta_time);
//         }
//         p = Enumerate(&polygonoids->coll);
//     }
//     ResetEnumerator(&polygonoids->coll); // Reset enumerator after drawing
// }

// int GetPolygonoidCount(void)
// {
//     return polygonoids->coll.count;
// }

// // Gameplay Screen Unload logic
// void UnloadGameplayScreen(void)
// {
//     // TODO: Unload GAMEPLAY screen variables here!
// }

// // Gameplay Screen should finish?
// int FinishGameplayScreen(void)
// {
//     return finishScreen;
// }

// // void DrawTextFieldsContainer(UIElement *text_fields_container, Camera2d camera)
// // {
// //     // 1. Get the primary anchor in pixel space
// //     Vector2d tcont_local_coords = text_fields_container->origin;
// //     Vector2d tcont_pixel_coords = TransformCoordinates(camera.source_to_dest_mtx, text_fields_container->origin);

// //     // Need to scale dimensions from world units to pixel units using the camera's basis transform
// //     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis);

// //     float tcont_w = text_fields_container->width * basis_scale.x;  // Assuming width is defined in world units and needs to be scaled to pixel units
// //     float tcont_h = text_fields_container->height * basis_scale.y; // Assuming height is defined in world units and needs to be scaled to pixel units

// //     ColourRgba colour_border_o = text_fields_container->colour_border;
// //     Color color_border_o = (Color){colour_border_o.r, colour_border_o.g, colour_border_o.b, colour_border_o.a};
// //     ColourRgba colour_fill_o = text_fields_container->colour_fill;
// //     Color color_fill_o = (Color){colour_fill_o.r, colour_fill_o.g, colour_fill_o.b, colour_fill_o.a};

// //     // 3. DRAW FILL (Outer Background) -- WORKING
// //     DrawRectangle(tcont_pixel_coords.x, tcont_pixel_coords.y, tcont_w, tcont_h, color_fill_o);

// //     //  7. DRAW TEXT FIELDS
// //     UIElement **text_fields = (UIElement **)text_fields_container->children.items;
// //     int tfield_count = text_fields_container->children.count;
// //     for (int i = 0; i < tfield_count; i++)
// //     {
// //         UIElement *text_field = ((UIElement **)text_fields)[i];
// //         Vector2d tfield_pixel_coords = {(text_field->parent_offset.x * basis_scale.x) + tcont_pixel_coords.x, (text_field->parent_offset.y * basis_scale.y) + tcont_pixel_coords.y};
// //         // text_field->origin = (Vector2d){inner_x + text_field_spacing.x, inner_y + text_field_spacing.y}; // Position each text field below the previous one with some spacing
// //         float w = text_field->width * basis_scale.x;
// //         float h = text_field->height * basis_scale.y;

// //         // DEBUG - draw rectangle around the field to check it's in the right place
// //         DrawRectangleLines(tfield_pixel_coords.x, tfield_pixel_coords.y, w, h, BLACK);
// //         if (total_frames % 800 == 0)
// //         {
// //             printf("Drew Text Field %d (%.1f, %.1f) : w = %.1f, h = %.1f\n", i, tfield_pixel_coords.x, tfield_pixel_coords.y, w, h);
// //         }

// //         DrawTextFieldElements(text_field, tfield_pixel_coords, FONT_BASIC, font_scale_m, camera);
// //     }
// // }

// // void DrawTextFieldElements(UIElement *text_field, Vector2d parent_pixel_coords, Bitmap_Font font, int font_scale, Camera2d camera)
// // {
// //     UIElement **children = (UIElement **)text_field->children.items;
// //     UIElement *text_box = children[0];   // Assuming the TextBox is the first child of the TextField
// //     UIElement *text_label = children[1]; // Assuming the TextLabel is the second child of the TextField
// //     Color color_border = (Color){text_box->colour_border.r, text_box->colour_border.g, text_box->colour_border.b, text_box->colour_border.a};
// //     Color color_fill = (Color){text_box->colour_fill.r, text_box->colour_fill.g, text_box->colour_fill.b, text_box->colour_fill.a};

// //     // Need to scale dimensions from world units to pixel units using the camera's basis transform
// //     Vector2d basis_scale = BasisTransform_2d_Scale(camera.source_basis, camera.destination_basis);

// //     // --- DRAW TEXT BOX ---
// //     // Raw Properties (before applying padding)
// //     // These are the raw dimensions of the text box in pixel space before padding is applied, so the actual area available for text will be smaller once we apply padding
// //     Vector2d tbox_pixel_coords = {(text_box->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_box->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
// //     float tbox_w = text_box->width * basis_scale.x;  // Assuming width is defined in world units and needs to be scaled to pixel units
// //     float tbox_h = text_box->height * basis_scale.y; // Assuming height is defined in world units and needs to be scaled to pixel units

// //     // Text Box - raw
// //     DrawRectangleLines(tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h, color_border);

// //     // Inner Properties (after applying padding).
// //     Vector2d padding = {text_box->padding.x * basis_scale.x, text_box->padding.y * basis_scale.y}; // Outer padding (margin)
// //     float tbox_inner_x = tbox_pixel_coords.x + padding.x;
// //     float tbox_inner_y = tbox_pixel_coords.y + padding.y;
// //     float tbox_inner_w = tbox_w - (2 * padding.x);
// //     float tbox_inner_h = tbox_h - (2 * padding.y);

// //     // Text Box - inner
// //     DrawRectangleLines(tbox_inner_x, tbox_inner_y, tbox_inner_w, tbox_inner_h, color_border);

// //     // 7. DRAW TEXT
// //     // 7.1 Draw the label of the text field above the text box (using the same x coordinate but a y coordinate above the box with some spacing)
// //     // DrawTextCustom(text_box->text, text_x, text_y, font_scale, font_default, font.colour);
// //     // DrawText(text_box->text, text_x, text_y, font_size, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});

// //     // --- DRAW TEXT LABEL ---
// //     // Raw Properties (before applying padding)
// //     Vector2d tlabel_pixel_coords = {(text_label->parent_offset.x * basis_scale.x) + parent_pixel_coords.x, (text_label->parent_offset.y * basis_scale.y) + parent_pixel_coords.y};
// //     float tlabel_w = text_label->width * basis_scale.x;  //-(2 * padding.x);
// //     float tlabel_h = text_label->height * basis_scale.y; //-(2 * padding.y);

// //     // Text Label - raw
// //     DrawRectangleLines(tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h, color_border);

// //     // Inner Properties (after applying padding).
// //     float tlabel_inner_x = tlabel_pixel_coords.x + padding.x;
// //     float tlabel_inner_y = tlabel_pixel_coords.y + padding.y;
// //     float tlabel_inner_w = tlabel_w - (2 * padding.x);
// //     float tlabel_inner_h = tlabel_h - (2 * padding.y);

// //     // Text Label- inner
// //     DrawRectangleLines(tlabel_inner_x, tlabel_inner_y, tlabel_inner_w, tlabel_inner_h, color_border);
// //     // DrawTextCustom(text_label->text, tlabel_pixel_coords.x, tlabel_pixel_coords.y, font_scale, font_default, font.colour);

// //     if (total_frames % 800 == 0)
// //     {
// //         printf("Drew Text Box at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tbox_pixel_coords.x, tbox_pixel_coords.y, tbox_w, tbox_h);
// //         printf("Drew Text Label at (%.1f, %.1f) : w = %.1f, h = %.1f\n", tlabel_pixel_coords.x, tlabel_pixel_coords.y, tlabel_w, tlabel_h);
// //     }
// //     // DRAW CURSOR
// //     // if (text_field->text_box.is_focused)
// //     // {
// //     //     // Simple blinking cursor logic
// //     //     static int frames = 0;
// //     //     char *text = text_box->text;
// //     //     if ((frames++ / 30) % 2 == 0)
// //     //     {
// //     //         int tw = MeasureTextWidth(text, font.spacing, font_scale);
// //     //         int th = font_scale * 8;
// //     //         int spacing_abs = abs(font.spacing);
// //     //         int spacing_split = spacing_abs / 2 < 1 ? 1 : spacing_abs / 2;

// //     //         int cursor_x_start = font.spacing > 0 ? text_x + tw + spacing_split : text_x + tw + spacing_abs;
// //     //         int cursor_y_start = text_y - spacing_split;
// //     //         int cursor_y_end = text_y + th + spacing_split;
// //     //         // DEBUGGING - Boxing the text to check it's drawn correctly
// //     //         // DrawRectangleLines(text_x, text_y, tw, text_y + th, (Color){colour_border.r, colour_border.g, colour_border.b, colour_border.a});

// //     //         DrawLine(cursor_x_start, cursor_y_start, cursor_x_start, cursor_y_end, WHITE);
// //     //     }
// //     // }

// //     // DrawTextCustom(text_label->text, text_x + label_w, text_y, font_scale, font_default, font.colour);
// // }
// void UpdateFields()
// {
//     // Update the collision/position field

//     // Update Circloids
//     // if (circloids == NULL || circloids->coll == NULL || circloids->coll->count <= 0)
//     // {
//     //     return; // No circloids to update
//     // }
//     // Circloid *circloid = Enumerate(circloids->coll);
//     // if (circloid == NULL)
//     // {
//     //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
//     // }
//     // while (circloid != NULL)
//     // {
//     //     if (&circloid->object != NULL)
//     //     {
//     //         CalculateVectors(&circloid->object, GetFrameDeltaTime());
//     //     }
//     //     circloid = Enumerate(circloids->coll);
//     // }
//     // ResetEnumerator(circloids->coll); // Reset enumerator after drawing

//     // Update Container
// }

// // Gameplay Screen should finish
// // Vector2d WorldToScreenCoordinates(Matrix3x3 basis_transform, Vector2d world_coordinates)
// // {
// //     Vector2d screen_coords;

// //     // 1. Get the "transformation" or "mapping" basis to go from world to screen.
// //     // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

// //     // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
// //     //  Multiply: (Row 1 * WorldColumn)
// //     //  screenX = (m0 * x) + (m3 * y) + m6
// //     screen_coords.x = (world_coordinates.x * basis_transform.m0) + (world_coordinates.y * basis_transform.m3) + basis_transform.m6;

// //     // Multiply: (Row 2 * WorldColumn)
// //     // screenY = (m1 * x) + (m4 * y) + m7
// //     screen_coords.y = (world_coordinates.x * basis_transform.m1) + (world_coordinates.y * basis_transform.m4) + basis_transform.m7;

// //     return screen_coords;
// // }

// // Vector2d WorldToScreenBasisTransformVector(Basis2d world_basis, Basis2d screen_basis, Vector2d screen_origin_coordinates, Vector2d world_coordinates)
// // {
// //     Vector2d screen_coordinates;

// //     // Need to translate the world's basis to screen basis. Therefore need basis_u and basis_v scaling factor
// //     float world_basis_u_mag = VectorMagnitude_2d(world_basis.u);
// //     float world_basis_v_mag = VectorMagnitude_2d(world_basis.v);

// //     float screen_basis_u_mag = VectorMagnitude_2d(screen_basis.u);
// //     float screen_basis_v_mag = VectorMagnitude_2d(screen_basis.v);

// //     float scale_u = screen_basis_u_mag / world_basis_u_mag;
// //     float scale_v = screen_basis_v_mag / world_basis_v_mag;

// //     // Scale the world coordinates by the basis scaling factors to get the coordinates in terms of the screen basis
// //     screen_coordinates.x = (world_coordinates.x * scale_u) + screen_origin_coordinates.x;
// //     screen_coordinates.y = (world_coordinates.y * scale_v) + screen_origin_coordinates.y;
// //     return screen_coordinates;
// // }

// // Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
// // {
// //    Vector2d origin = field.shape.object.position;
// //    Vector2d u = field.coordinateSpace.basis.u;
// //    Vector2d v = field.coordinateSpace.basis.v;

// //    // // Get position relative to the grid origin
// //    // float px = objectPos.x - origin.x;
// //    // float py = objectPos.y - origin.y;

// //    // // Calculate the Determinant
// //    // float det = (u.x * v.y) - (u.y * v.x);

// //    // // If determinant is 0, the grid is collapsed (invalid)
// //    // if (fabs(det) < 0.0001f)
// //    //    return (Vector2d){-1, -1};

// //    // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
// //    // float c = (px * v.y - py * v.x) / det;
// //    // float r = (py * u.x - px * u.y) / det;

// //    // // Use floor() to get the integer index of the cell
// //    // return (Vector2d){floorf(c), floorf(r)};
// // }
// // // Gameplay Screen Draw logic
// // void DrawGameplayScreen(void)
// // {
// //     // TODO: Draw GAMEPLAY screen here!
// //     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
// //     Vector2 pos = { 20, 100 };
// //     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
// //     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// // }
// // Gameplay Screen - Main stage Draw
// // void DrawGameplayScreenStage(int startX, int startY, int width, int height, Color color)
// // {
// //     // Stage canvas for circloids to interact on
// //     DrawRectangle(startX, startY, width, height, color);

// //     // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
// //     // DrawWorld(world);
// //     // DrawWorldCoordinateSpace(world.world_space);
// //     //  Draw fields here
// //     //  DrawFields_Rect();

// //     // Draw circloids last so that they are on top of the fields
// //     // DrawCircloids();
// // }

// // void DrawCircloids(void)
// // {
// //     if (circloids == NULL) // || circloids->coll == NULL || circloids->coll->count <= 0)
// //     {
// //         return; // No circloids to draw
// //     }
// //     Collection *circloid_coll = &circloids->coll;
// //     for (int i = 0; i < circloid_coll->count; i++)
// //     {
// //         Circloid *circloid = (Circloid *)((char *)circloid_coll->items + (i * circloid_coll->elemSize));
// //         Vector2d circloidPos = circloid->newtonian_properties.world_position;
// //         Vector2d cellIndices = GetCellIndicesFromCoordinates(position_field.shape.newtonian_properties.world_position, circloid->newtonian_properties.world_position, position_field.coordinateSpace.basis);

// //         // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

// //         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)", ((int)cellIndices.x + 1) * ((int)cellIndices.y + 1), (int)cellIndices.x + 1, (int)cellIndices.y + 1, (int)circloidPos.x, (int)circloidPos.y);
// //         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

// //         // Draw circloid THEN text so text is on top
// //         DrawCircle(circloidPos.x, circloidPos.y, circloid->radius, (Color)DARKBROWN_RGBA);
// //         DrawTextEx(font, displayText, (Vector2){circloidPos.x - 0.7 * circloid->radius, circloidPos.y - 0.7 * circloid->radius}, font.baseSize, 1, (Color)BEIGE_RGBA);

// //         // Debug print
// //         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
// //     }
// //     // Circloid *circloid = Enumerate(circloids->coll);
// //     // if (circloid == NULL)
// //     // {
// //     //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
// //     // }
// //     // while (circloid != NULL)
// //     // {
// //     //     Vector2d pos = circloid->object.pos;
// //     //     Vector2d cell = GetCellFromCoordinates(position_field, circloid->object.pos);
// //     //     DrawCircle(pos.x, pos.y, circloid->radius, (Color)DARKBROWN_RGBA);

// //     //     // Output the circloid's position and cell as text on top of it
// //     //     const char *cellText = TextFormat("Cell: (%d,%d)", (int)cell.x, (int)cell.y);
// //     //     const char *posText = TextFormat("Coord: (%d,%d)", (int)pos.x, (int)pos.y);
// //     //     const char *allText = TextFormat("%s\n%s", cellText, posText);
// //     //     DrawTextEx(font, allText, (Vector2){pos.x, pos.y}, font.baseSize, 1, (Color)BEIGE_RGBA);
// //     //     // DrawTextEx(font, cellText, (Vector2){pos.x - (circloid->radius / 2), pos.y - (circloid->radius / 2)}, font.baseSize, 1, (Color)DARKGREEN_RGBA);

// //     //     circloid = Enumerate(circloids->coll);
// //     // }
// //     // ResetEnumerator(circloids->coll); // Reset enumerator after drawing
// // }

// // void DrawFields_Rect(void)
// // {
// //     if (position_field.coordinateSpace.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
// //     {
// //         return; // No field to draw
// //     }
// //     // Draw background
// //     DrawRectangle(position_field.shape.newtonian_properties.world_position.x, position_field.shape.newtonian_properties.world_position.y, position_field.shape.width, position_field.shape.height, (Color){world_bg_colour.r, world_bg_colour.g, world_bg_colour.b, world_bg_colour.a});

// //     int rows = position_field.coordinateSpace.rows;
// //     int cols = position_field.coordinateSpace.columns;
// //     int totalUnits = rows * cols;
// //     CoordinateSpace coordinateSpace = position_field.coordinateSpace;
// //     ColourRgba colour = position_field.lineColour;
// //     Color color = (Color){colour.r, colour.g, colour.b, colour.a};

// //     // Draw "Horizontal-ish" lines (Rows)
// //     // These lines start at (origin + r*v) and end at (origin + r*v + cols*u)
// //     Vector2d origin = position_field.shape.newtonian_properties.world_position;
// //     for (int r = 0; r < coordinateSpace.lineSegments_u.coll.count; r++)
// //     {
// //         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_u.coll.items + (r * coordinateSpace.lineSegments_u.coll.elemSize));
// //         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
// //     }

// //     // Draw "Vertical-ish" lines (Columns)
// //     // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
// //     for (int c = 0; c < coordinateSpace.lineSegments_v.coll.count; c++)
// //     {
// //         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_v.coll.items + (c * coordinateSpace.lineSegments_v.coll.elemSize));
// //         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
// //     }

// //     // Draw field unit values as text on top of each field unit
// //     Collection *cells = &position_field.coordinateSpace.cells.coll;
// //     // int textOffsetX = (position_field.coordinateSpace.basis.u.x + position_field.coordinateSpace.basis.v.x) / 2;
// //     // int textOffsetY = (position_field.coordinateSpace.basis.u.y + position_field.coordinateSpace.basis.v.y) / 2;
// //     for (int i = 0; i < totalUnits; i++)
// //     {
// //         int row = i / cols;
// //         int col = i % cols;
// //         Cell *cell = (Cell *)((char *)cells->items + (i * cells->elemSize));
// //         Vector2d cellPos = cell->coords;
// //         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)\nValue: %.1f", i + 1, row + 1, col + 1, (int)cellPos.x, (int)cellPos.y, cell->value);
// //         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
// //         DrawTextEx(font, displayText, (Vector2){cellPos.x, cellPos.y}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

// //         // Debug print
// //         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
// //     }
// //     // printf("Drew %d cells\n", count);
// //     //  Use the number of rows, columns, and their dimensions to draw field lines as rectangles
// //     //  Method 1: Enumerate the grid
// //     //  float *cell = Enumerate(position_field.cells->coll);
// //     //  if (cell == NULL)
// //     //  {
// //     //      fprintf(stderr, "Failed to retrieve enumerated field unit\n"); // Enumerator failed to retrieve the first item
// //     //  }
// // }