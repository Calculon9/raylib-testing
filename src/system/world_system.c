/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 *   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
 *
 *   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
 *
 *   This software is provided "as-is", without any express or implied warranty. In no event
 *   will the authors be held liable for any damages arising from the use of this software.
 *
 *   Permission is granted to anyone to use this software for any purpose, including commercial
 *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *     wrote the original software. If you use this software in a product, an acknowledgment
 *     in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *     as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
 *
 **********************************************************************************************/
#include <stdint.h>
#include "raylib.h"
#include "system/utility_system.h"
#include "system/world_system.h"
#include "system/systems.h"
#include "physics/polygonoid.h"
#include "common/common.h"
#include "world/world.h"
#include "camera/camera.h"
#include <math/helpers.h>
// #include "screens.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
WorldState G_WorldState = (WorldState){0};
// ----------WORLD SCREEN----------
static int finishScreen = 0;
static Polygonoid *selectedObject = NULL; // Pointer to the currently selected object (if any) for displaying its properties in the panel
static Cell *selectedCell = NULL;         // Pointer to the currently selected cell (if any) for displaying its properties in the panel
// Visual Properties
static ColourRgba world_text_colour = BROWN_1_RGBA_4; //{55, 97, 0, 200};
static ColourRgba world_fill_colour = WHITE_RGBA;     //{48, 104, 68, 70}; // MAROON_RGBA;// {150, 255, 220,180};//DARKGREEN_RGBA;
static ColourRgba world_line_colour = LIGHTGRAY_RGBA; //{128, 99, 42, 100};
// Coordinate Space Properties
World2d world = {0};
Vector2d world_origin, world_end = {0};
Vector2d world_pixel_origin, world_pixel_end = {0};
Vector2d world_u = {1, 0};
Vector2d world_v = {0, 1};
Vector2d world_resolution = {0};
static float gravity = 10;
// Objects and properties
static int next_object_id = 1; // Global variable to keep track of the next available ID for NewtonObjects
static ColourRgba polygonoid_line_colour = {155, 0, 0, 255};
static ColourRgba polygonoid_text_colour = {64, 64, 64, 255};
static float polygonoid_radius_default = 1.0;
static float polygonoid_mass_default = 1.0;
static Vector2d polygonoid_velocity_default = {1.0, 1.0};
static Vector2d polygonoid_acceleration_default = {0.0, 0.0f};
static int initObjectCount = 4;

// Logical->pixel-space conversion properties
// static Vector2d screen_game_origin, screen_game_end = {0};
Vector2d world_pixel_u = {0};
Vector2d world_pixel_v = {0};
Vector2d local_to_world_scale = {0};
Vector2d world_to_local_scale = {0};
Camera2d camera_world = {0};
static float camera_world_zoom = 1.0;
static float camera_world_rotation = 0.0;
LArray *test = NULL;
//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void InitPanelTextContainers();
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region);
void CreateAddPolygonoid(int vertice_count, float radius, ShapeType shape_type, float mass, ColourRgba colour, Vector2d coords_center, Vector2d velocity, Vector2d acceleration);

// FIRST: Initialisation of Gameplay Screen
void InitGameWorld(void)
{
    // 1. INIT CAMERAS using using the resolutions, sceen basis, origins etc. from Step 0
    // 1.1 Game world camera
    Basis2d world_basis = (Basis2d){world_u, world_v};
    Basis2d world_pixel_basis = (Basis2d){world_pixel_u, world_pixel_v};
    camera_world = CreateCamera2d(world_pixel_basis, world_basis, world_pixel_origin, world_origin, camera_world_zoom, camera_world_rotation);

    // 3 CREATE GAME WORLD using the resolutions, origins etc. from Step 0
    // 3.1 Create the coordinate space for the world
    // 3.11 Initialise Objects
    LArray objects = MakeLArray(initObjectCount, sizeof(Polygonoid));
    LArray collisions = MakeLArray(initObjectCount, sizeof(Matrix2x2));
    // 3.2 Create the space then world
    CoordSpace2d_Grid space_g = NewCoordSpace2d_Grid(world_origin, world_resolution, world_basis, world_fill_colour, world_line_colour);
    space_g.object.id = 0; // Parent object is 0
    world = CreateWorld(space_g, gravity);
    world.objects = objects;
    world.collisions = collisions;
    G_WorldState.world_coord_space = &world.coord_space_grid;

    // framesCounter = 0;
    // finishScreen = 0;
}

void DrawGameWorld()
{
    // Draw the game world
    DrawWorldRegion(&world, &camera_world);
    // DrawWorldRegion(panelWidth, 0, stageWidth, stageHeight, (Color){stageBackgroundColour.r, stageBackgroundColour.g, stageBackgroundColour.b, stageBackgroundColour.a});
}

// Gameplay Screen Update logic
void UpdateWorldSystem(int mouse_x, int mouse_y)
{
    bool cursor_in_world = mouse_x >= world_pixel_origin.x && mouse_x <= (world_pixel_origin.x + (world_pixel_u.x * world_resolution.x)) && mouse_y >= world_pixel_origin.y && mouse_y <= (world_pixel_origin.y + (world_pixel_v.y * world_resolution.y));

    UpdateWorldRegion(mouse_x, mouse_y, cursor_in_world);
}
// Gameplay Screen Stage Update logic
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region)
{
    Vector2d click_pixel_coords = {mouse_x, mouse_y};
    Vector2d click_world_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords);

    // DEFAULT TESTING SPAWN of polygonoids with random properties
    float radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.8);
    float mass = radius * polygonoid_mass_default;
    int vertice_count = 7;
    Vector2d velocity = {.x = GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), .y = GetRandomFloat(polygonoid_velocity_default.y * 8, polygonoid_velocity_default.y * -8)};
    Vector2d acceleration = polygonoid_acceleration_default;
    ColourRgba colour = polygonoid_line_colour;


    // DEBUGGING - we will update object vectors if button is pressed
    bool keyDown = IsKeyDown(KEY_LEFT_CONTROL);
    if (keyDown)
    {
        // PrintCurrentBytesAlloc();
        UpdateWorld(&world, frame_counter.delta_time);
        // PrintCurrentBytesAlloc();
    }
    // DEBUGGING - 1 Frame setp-through
    bool keyPressed = IsKeyPressed(KEY_LEFT_SHIFT);
    if (keyPressed)
    {
        // PrintCurrentBytesAlloc();
        UpdateWorld(&world, frame_counter.delta_time);
        // PrintCurrentBytesAlloc();
    }

    // DEBUGGING - Rapid firing of polygonoids
    keyDown = IsKeyDown(KEY_ONE);
    if (keyDown & frame_counter.total_frames % 3 == 0)
    {
        radius = GetRandomFloat(0.1, polygonoid_radius_default * 0.25);
        mass = radius * polygonoid_mass_default;
        velocity = (Vector2d){GetRandomFloat(polygonoid_velocity_default.x * -8, polygonoid_velocity_default.x * 8), GetRandomFloat(polygonoid_velocity_default.y * 0, polygonoid_velocity_default.y * 16)};
        Vector2d top_middle_world = (Vector2d){world.coord_space_grid.object.boxed_dimensions.x * 0.5, 0.3};
        //Vector2d top_middle_world_pixel = TransformCoordinates(camera_world.source_to_dest_mtx, top_middle_world);
        CreateAddPolygonoid(vertice_count,radius, SHAPE_MATH_POLY_HULL, mass, colour, top_middle_world, velocity, acceleration);
        UpdateWorld(&world, frame_counter.delta_time);
    }

    // DEBUGGING - Collisions
    keyPressed = IsKeyPressed(KEY_THREE);
    if (keyPressed)
    {
        radius = 1.5;
        mass = radius * polygonoid_mass_default;
        velocity = (Vector2d){ -3,  3};
        CreateAddPolygonoid(3,radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // triangle
        UpdateWorld(&world, frame_counter.delta_time);
    }
    keyPressed = IsKeyPressed(KEY_FOUR);
    if (keyPressed)
    {
        radius = 1.5;
        mass = radius * polygonoid_mass_default;
        velocity = (Vector2d){ -3,  3};
        CreateAddPolygonoid(4,radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration); // rectangle
        UpdateWorld(&world, frame_counter.delta_time);
    }
    // Draw a circle where the mouse clicks and add it to the state
    if (IsKeyPressed(KEY_UP) && cursor_in_region)
    {
        // test = AllocLArray(4, sizeof(float));
        // float v = 40.5;
        // unsigned char *check = (unsigned char *)&v;
        // LArray_Push(test, &v);
        // printf("Expected Hex: %02X %02X %02X %02X\n", check[0], check[1], check[2], check[3]);

        // 1. Add a new polygonoid to the state with the position of the mouse click - give an initial velocity
        // 1.1 Convert mouse pixel coords to world coords
        // CreateAddPolygonoid(radius, SHAPE_MATH_EQUIDISTANT, mass, colour, click_world_coords, velocity, acceleration);
        CreateAddPolygonoid(vertice_count,radius, SHAPE_MATH_POLY_HULL, mass, colour, click_world_coords, velocity, acceleration);
        // PrintCurrentBytesAlloc();
        finishScreen = 1;
        // PlaySound(fxCoin);
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return;
    }
    if (!cursor_in_region)
    {
        return;
    }

    // ----DEBUG
    char log[256] = "";
    int offset = 0;
    offset += snprintf(log + offset, sizeof(log) - offset, "WORLD (%.1f,%.1f) --> ", world_origin.x, world_origin.y);
    // DEBUG--- //

    // Check if a click is on an object and print some info about that object if so
    click_pixel_coords = (Vector2d){mouse_x, mouse_y};                                                        // This is in pixel coordinates relative to the top left of the screen, so we need to convert it to world coordinates before we can compare it to object coordinates which are relative to the world origin
    Vector2d click_local_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords); // This is relative to the world origin, so we can use it directly to compare to object coordinates which are also relative to the world origin

    int cell_index = ((int)click_local_coords.y * (int)world_resolution.x) + (int)click_local_coords.x;
    if (click_local_coords.x < 0 || click_local_coords.y < 0 ||
        click_local_coords.x >= world_resolution.x || click_local_coords.y >= world_resolution.y)
    {
        // Click is outside the structural world viewport boundaries! Avoid resolving cell.
        return;
    }

    // Check if there are any objects in that cell and print info about those objects if so
    Cell *cells = world.coord_space_grid.coord_space.cells.items;
    Cell *p_cell = &cells[cell_index];
    Cell cell = *p_cell;
    G_WorldState.selected_cell = p_cell;

    // ----DEBUG--- //
    offset += snprintf(log + offset, sizeof(log) - offset, "CELL %d(%.0f,%.0f) Occ:%d Val:%.1f --> ENTITIES ", cell_index, cell.coords_origin.x, cell.coords_origin.y, cell.occupancy, cell.value);

    // Check World objects for the object with the same ID as the one in the cell and print its properties if found
    Polygonoid *objs = (Polygonoid *)world.objects.items;
    // Vector2d click_to_cell_dist = VectorSum_2d(VectorScale_2d(cell.coords_center, -1), click_local_coords);
    // float click_to_cell_mag = fabs(VectorMagnitude_2d(click_to_cell_dist));
    Vector2d click_to_obj_vec = {world.coord_space_grid.coord_space.resolution_ixj.x, world.coord_space_grid.coord_space.resolution_ixj.y}; // Init with the max value in the world's coord system
    float shortest_dist = fabs(VectorMagnitude_2d(click_to_obj_vec));
    Polygonoid *p_closest = NULL;
    for (int i = 0; i < cell.occupancy; i++)
    {
        int cell_id = cell.object_ids[i];
        if (cell_id == 0)
        {
            // No object Id  at this index yet (still at initialised value).
            continue;
        }
        if (cell_id < 0)
        {
            LOG_ERROR("Object Id stored in Cell is < 0 (%.1f)\n", cell_id);
            continue;
        }

        Polygonoid *p = &objs[cell_id - 1]; // Minus 1 because the Id starts at 1, not 0;
        int obj_id = p->newtonian_properties.id;
        // It SHOULD equal the object ID...
        if (obj_id == cell_id)
        {
            // Check if the click was within p_closest
            Surface2d surface = p->newtonian_properties.surface;
            Vector2d vertice_offset = p->newtonian_properties.coords_center;
            bool click_in_object = IsPointInPolygon(click_local_coords, (Vector2d *)surface.surface_vectors.items, vertice_offset, surface.surface_vectors.count);

            Vector2d click_to_obj_dist_i = VectorSum_2d(VectorScale_2d(p->newtonian_properties.coords_center, -1), click_local_coords);
            float click_to_obj_mag_i = fabs(VectorMagnitude_2d(click_to_obj_dist_i));
            if (click_to_obj_mag_i < shortest_dist && click_in_object)
            {
                shortest_dist = click_to_obj_mag_i;
                p_closest = p;
            }

            // ----DEBUG--- //
            offset += snprintf(log + offset, sizeof(log) - offset, "[ID:%d POS:%.1f,%.1f] ", i + 1, obj_id, p->newtonian_properties.coords_center.x, p->newtonian_properties.coords_center.y);
        }
        else
        {
            LOG_ERROR("Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. ID in CELL = %d. ID in ENTITY = %d.\n", cell_id, obj_id);
            continue;
        }
    }

    if (p_closest)
    {
        G_WorldState.selected_object = p_closest;
        // ----DEBUG---- //
        offset += snprintf(log + offset, sizeof(log) - offset, "--> SELECTED ENTITY: ID:%d", p_closest->newtonian_properties.id);
    }
    else
    {
        G_WorldState.selected_object = NULL;
        // ----DEBUG---- //
        offset += snprintf(log + offset, sizeof(log) - offset, " --> SELECTED ENTITY: NULL");
    }
    // ----DEBUG---- //
    LOG_INFO("CLICKED (%d,%d) | %s\n", mouse_x, mouse_y, log);
}

void CreateAddPolygonoid(int vertice_count, float radius, ShapeType shape_type, float mass, ColourRgba colour, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
    Polygonoid new_polygonoid = {0};
    switch (shape_type)
    {
    case SHAPE_MATH_EQUIDISTANT:
        new_polygonoid = CreatePolygonoid_Symmetric(vertice_count, radius, colour, mass, coords_center, velocity, acceleration);
        break;
    case SHAPE_MATH_POLY_HULL:
        float min_radius = GetRandomFloat(0, radius);
        new_polygonoid = CreatePolygonoid_Irregular(vertice_count, min_radius, radius, colour, mass, coords_center, velocity, acceleration);
        break;
    default:
        break;
    }

    if (new_polygonoid.radius > 0.0)
        AddObjectToWorld(&world, &new_polygonoid, world.coord_space_grid.object.id);

    // Array_Push(polygonoids, &newPolygonoid);
}



int GetPolygonoidCount(void)
{
    return world.objects.count;
}

// Gameplay Screen Unload logic
void UnloadGameplayScreen(void)
{
    // TODO: Unload GAMEPLAY screen variables here!
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void)
{
    return finishScreen;
}

// void UpdatePolygonoidVectors(DArray *polygonoids)
// {
//     // Update Polygonoids
//     if (polygonoids == NULL || polygonoids->count <= 0)
//     {
//         return; // Nothing to update
//     }
//     Polygonoid *p = Enumerate(polygonoids);
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
//         p = Enumerate(polygonoids);
//     }
//     ResetEnumerator(polygonoids); // Reset enumerator after drawing
// }

// Gameplay Screen should finish
// Vector2d WorldToScreenCoordinates(Matrix3x3 basis_transform, Vector2d world_coordinates)
// {
//     Vector2d screen_coords;

//     // 1. Get the "transformation" or "mapping" basis to go from world to screen.
//     // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

//     // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
//     //  Multiply: (Row 1 * WorldColumn)
//     //  screenX = (m0 * x) + (m3 * y) + m6
//     screen_coords.x = (world_coordinates.x * basis_transform.m0) + (world_coordinates.y * basis_transform.m3) + basis_transform.m6;

//     // Multiply: (Row 2 * WorldColumn)
//     // screenY = (m1 * x) + (m4 * y) + m7
//     screen_coords.y = (world_coordinates.x * basis_transform.m1) + (world_coordinates.y * basis_transform.m4) + basis_transform.m7;

//     return screen_coords;
// }

// Vector2d WorldToScreenBasisTransformVector(Basis2d world_basis, Basis2d screen_basis, Vector2d screen_origin_coordinates, Vector2d world_coordinates)
// {
//     Vector2d screen_coordinates;

//     // Need to translate the world's basis to screen basis. Therefore need basis_u and basis_v scaling factor
//     float world_basis_u_mag = VectorMagnitude_2d(world_basis.u);
//     float world_basis_v_mag = VectorMagnitude_2d(world_basis.v);

//     float screen_basis_u_mag = VectorMagnitude_2d(screen_basis.u);
//     float screen_basis_v_mag = VectorMagnitude_2d(screen_basis.v);

//     float scale_u = screen_basis_u_mag / world_basis_u_mag;
//     float scale_v = screen_basis_v_mag / world_basis_v_mag;

//     // Scale the world coordinates by the basis scaling factors to get the coordinates in terms of the screen basis
//     screen_coordinates.x = (world_coordinates.x * scale_u) + screen_origin_coordinates.x;
//     screen_coordinates.y = (world_coordinates.y * scale_v) + screen_origin_coordinates.y;
//     return screen_coordinates;
// }

// Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
// {
//    Vector2d origin = field.shape.object.position;
//    Vector2d u = field.coordinateSpace.basis.u;
//    Vector2d v = field.coordinateSpace.basis.v;

//    // // Get position relative to the grid origin
//    // float px = objectPos.x - origin.x;
//    // float py = objectPos.y - origin.y;

//    // // Calculate the Determinant
//    // float det = (u.x * v.y) - (u.y * v.x);

//    // // If determinant is 0, the grid is collapsed (invalid)
//    // if (fabs(det) < 0.0001f)
//    //    return (Vector2d){-1, -1};

//    // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
//    // float c = (px * v.y - py * v.x) / det;
//    // float r = (py * u.x - px * u.y) / det;

//    // // Use floor() to get the integer index of the cell
//    // return (Vector2d){floorf(c), floorf(r)};
// }
// // Gameplay Screen Draw logic
// void DrawGameplayScreen(void)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
//     Vector2 pos = { 20, 100 };
//     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
//     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// }
// Gameplay Screen - Main stage Draw
// void DrawGameplayScreenStage(int startX, int startY, int width, int height, Color color)
// {
//     // Stage canvas for circloids to interact on
//     DrawRectangle(startX, startY, width, height, color);

//     // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
//     // DrawWorld(world);
//     // DrawWorldCoordinateSpace(world.world_space);
//     //  Draw fields here
//     //  DrawFields_Rect();

//     // Draw circloids last so that they are on top of the fields
//     // DrawCircloids();
// }

// void DrawCircloids(void)
// {
//     if (circloids == NULL) // || circloids->coll == NULL || circloids->coll->count <= 0)
//     {
//         return; // No circloids to draw
//     }
//     Collection *circloid_coll = &circloids->coll;
//     for (int i = 0; i < circloid_coll->count; i++)
//     {
//         Circloid *circloid = (Circloid *)((char *)circloid_coll->items + (i * circloid_coll->elemSize));
//         Vector2d circloidPos = circloid->newtonian_properties.world_position;
//         Vector2d cellIndices = GetCellIndicesFromCoordinates(position_field.shape.newtonian_properties.world_position, circloid->newtonian_properties.world_position, position_field.coordinateSpace.basis);

//         // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

//         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)", ((int)cellIndices.x + 1) * ((int)cellIndices.y + 1), (int)cellIndices.x + 1, (int)cellIndices.y + 1, (int)circloidPos.x, (int)circloidPos.y);
//         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

//         // Draw circloid THEN text so text is on top
//         DrawCircle(circloidPos.x, circloidPos.y, circloid->radius, (Color)DARKBROWN_RGBA);
//         DrawTextEx(font, displayText, (Vector2){circloidPos.x - 0.7 * circloid->radius, circloidPos.y - 0.7 * circloid->radius}, font.baseSize, 1, (Color)BEIGE_RGBA);

//         // Debug print
//         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
//     }
//     // Circloid *circloid = Enumerate(circloids->coll);
//     // if (circloid == NULL)
//     // {
//     //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
//     // }
//     // while (circloid != NULL)
//     // {
//     //     Vector2d pos = circloid->object.pos;
//     //     Vector2d cell = GetCellFromCoordinates(position_field, circloid->object.pos);
//     //     DrawCircle(pos.x, pos.y, circloid->radius, (Color)DARKBROWN_RGBA);

//     //     // Output the circloid's position and cell as text on top of it
//     //     const char *cellText = TextFormat("Cell: (%d,%d)", (int)cell.x, (int)cell.y);
//     //     const char *posText = TextFormat("Coord: (%d,%d)", (int)pos.x, (int)pos.y);
//     //     const char *allText = TextFormat("%s\n%s", cellText, posText);
//     //     DrawTextEx(font, allText, (Vector2){pos.x, pos.y}, font.baseSize, 1, (Color)BEIGE_RGBA);
//     //     // DrawTextEx(font, cellText, (Vector2){pos.x - (circloid->radius / 2), pos.y - (circloid->radius / 2)}, font.baseSize, 1, (Color)DARKGREEN_RGBA);

//     //     circloid = Enumerate(circloids->coll);
//     // }
//     // ResetEnumerator(circloids->coll); // Reset enumerator after drawing
// }

// void DrawFields_Rect(void)
// {
//     if (position_field.coordinateSpace.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
//     {
//         return; // No field to draw
//     }
//     // Draw background
//     DrawRectangle(position_field.shape.newtonian_properties.world_position.x, position_field.shape.newtonian_properties.world_position.y, position_field.shape.width, position_field.shape.height, (Color){world_bg_colour.r, world_bg_colour.g, world_bg_colour.b, world_bg_colour.a});

//     int rows = position_field.coordinateSpace.rows;
//     int cols = position_field.coordinateSpace.columns;
//     int totalUnits = rows * cols;
//     CoordinateSpace coordinateSpace = position_field.coordinateSpace;
//     ColourRgba colour = position_field.lineColour;
//     Color color = (Color){colour.r, colour.g, colour.b, colour.a};

//     // Draw "Horizontal-ish" lines (Rows)
//     // These lines start at (origin + r*v) and end at (origin + r*v + cols*u)
//     Vector2d origin = position_field.shape.newtonian_properties.world_position;
//     for (int r = 0; r < coordinateSpace.lineSegments_u.coll.count; r++)
//     {
//         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_u.coll.items + (r * coordinateSpace.lineSegments_u.coll.elemSize));
//         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
//     }

//     // Draw "Vertical-ish" lines (Columns)
//     // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
//     for (int c = 0; c < coordinateSpace.lineSegments_v.coll.count; c++)
//     {
//         LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_v.coll.items + (c * coordinateSpace.lineSegments_v.coll.elemSize));
//         DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
//     }

//     // Draw field unit values as text on top of each field unit
//     Collection *cells = &position_field.coordinateSpace.cells.coll;
//     // int textOffsetX = (position_field.coordinateSpace.basis.u.x + position_field.coordinateSpace.basis.v.x) / 2;
//     // int textOffsetY = (position_field.coordinateSpace.basis.u.y + position_field.coordinateSpace.basis.v.y) / 2;
//     for (int i = 0; i < totalUnits; i++)
//     {
//         int row = i / cols;
//         int col = i % cols;
//         Cell *cell = (Cell *)((char *)cells->items + (i * cells->elemSize));
//         Vector2d cellPos = cell->coords;
//         const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)\nValue: %.1f", i + 1, row + 1, col + 1, (int)cellPos.x, (int)cellPos.y, cell->value);
//         // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
//         DrawTextEx(font, displayText, (Vector2){cellPos.x, cellPos.y}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

//         // Debug print
//         // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
//     }
//     // printf("Drew %d cells\n", count);
//     //  Use the number of rows, columns, and their dimensions to draw field lines as rectangles
//     //  Method 1: Enumerate the grid
//     //  float *cell = Enumerate(position_field.cells->coll);
//     //  if (cell == NULL)
//     //  {
//     //      fprintf(stderr, "Failed to retrieve enumerated field unit\n"); // Enumerator failed to retrieve the first item
//     //  }
// }