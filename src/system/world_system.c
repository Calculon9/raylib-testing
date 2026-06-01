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
static ColourRgba world_line_colour = YELLOW_RGBA;    //{128, 99, 42, 100};
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
static float polygonoid_radius_default = 1.2;
static float polygonoid_mass_default = 1.0;
static Vector2d polygonoid_velocity_default = {1.40, 0.60};
static Vector2d polygonoid_acceleration_default = {0.0, 0.0f};
static int initObjectCount = 8;

// Logical->pixel-space conversion properties
// static Vector2d screen_game_origin, screen_game_end = {0};
Vector2d world_pixel_u = {0};
Vector2d world_pixel_v = {0};
Vector2d local_to_world_scale = {0};
Vector2d world_to_local_scale = {0};
Camera2d camera_world = {0};
static float camera_world_zoom = 1.0;
static float camera_world_rotation = 0.0;

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
// void InitCoordinateSpaceProperties();
void InitPanelTextContainers();
// void DrawCircloids();
void DrawPolygonoids(LArray *polygonoids);
void DrawObjectVertices(LArray local_vertices, Vector2d local_offset, Camera2d camera, ColourRgba line_colour);
void DrawWorldRegion(World2d *world, Camera2d world_camera);
void DrawWorldCoordinateGrid();
void UpdatePolygonoidVectors(DArray *polygonoids);
void UpdateWorldRegion(int mouse_x, int mouse_y, bool cursor_in_region);
void CreateAddPolygonoid_Circle(float radius, float mass, ColourRgba colour, Vector2d origin, Vector2d velocity, Vector2d acceleration);
// Vector2d WorldToScreenCoordinates(Matrix3x3 screen_basis_transform, Vector2d world_coordinates);

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
    // 3.2 Create the space then world
    CoordSpace2d_Grid space_g = NewCoordSpace2d_Grid(world_origin, world_resolution, world_basis, world_fill_colour, world_line_colour);
    world = CreateWorld(space_g, gravity);
    world.objects = objects;
    // 4 CREATE TEST POLYGONOIDS
    // 4.1 Static
    // CreateAddPolygonoid_Circle(polygonoid_radius_default, polygonoid_mass_default, polygonoid_line_colour, (Vector2d){world_resolution.x / 1.70, world_resolution.y / 1.65}, polygonoid_velocity_default, polygonoid_acceleration_default);
    // 4.2 Moving
    // AddPolygonoid_Circle(0.4, 2.0, (Vector2d){world_resolution.x / 1.70, world_resolution.y / 1.65}, (Vector2d){0.0f, 0.3f}, (Vector2d){0.0f, 0.0f});

    // 5. Initialise UI elements (text boxes, etc.) using the coordinate space properties from Step 0
    // InitPanelTextContainers();
    // text_boxes = NEW_DYNAMIC_ARRAY(8, TextBox);
    // 5.1 Create text box for displaying object properties in the panel, using the panel coordinate space properties from Step 0
    // lpanel_properties_tbox = CreateTextBox(lpanel_tbox_default_dims.x, lpanel_tbox_default_dims.y, lpanel_properties_tbox_coords, lpanel_tbox_default_padding_inner, lpanel_tbox_default_padding_outer, lpanel_tbox_default_colour_border_outer, lpanel_tbox_default_colour_fill_outer, lpanel_tbox_default_colour_border_inner, lpanel_tbox_default_colour_fill_inner);

    // framesCounter = 0;
    // finishScreen = 0;
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

    // Update vectors of all objects
    // DEBUGGING - we will update object vectors if button is pressed
    bool keyDown = IsKeyDown(KEY_LEFT_CONTROL);
    if (keyDown)
    {
        UpdateWorld(&world, frame_counter.delta_time);
        // UpdatePolygonoidVectors(polygonoids);
        // UpdateWorldPhysics(*polygonoids);
    }

    // Update Fields

    // Draw a circle where the mouse clicks and add it to the state
    if (IsKeyPressed(KEY_UP) && cursor_in_region)
    {
        // 1. Add a new polygonoid to the state with the position of the mouse click - give an initial velocity
        // 1.1 Convert mouse pixel coords to world coords
        Vector2d click_pixel_coords = {mouse_x, mouse_y};
        Vector2d click_world_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords);

        float radius = polygonoid_radius_default;
        float mass = polygonoid_mass_default;
        Vector2d velocity = polygonoid_velocity_default;
        Vector2d acceleration = polygonoid_acceleration_default;
        ColourRgba colour = polygonoid_line_colour;
        CreateAddPolygonoid_Circle(radius, mass, colour, click_world_coords, velocity, acceleration);

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

    char log[256] = "";
    int offset = 0;
    offset += snprintf(log + offset, sizeof(log) - offset, "WORLD (%.1f, %.1f) --> ", world_origin.x, world_origin.y);

    // Check if a click is on an object and print some info about that object if so
    Vector2d click_pixel_coords = {mouse_x, mouse_y};                                                        // This is in pixel coordinates relative to the top left of the screen, so we need to convert it to world coordinates before we can compare it to object coordinates which are relative to the world origin
    Vector2d click_world_coords = TransformCoordinates(camera_world.dest_to_source_mtx, click_pixel_coords); // This is relative to the world origin, so we can use it directly to compare to object coordinates which are also relative to the world origin

    Polygonoid *polygonoids = world.objects.items;
    int cell_index = ((int)click_world_coords.y * (int)world_resolution.x) + (int)click_world_coords.x;

    // Check if there are any objects in that cell and print info about those objects if so
    Cell *cells = world.coord_space_grid.coord_space.cells.items;
    Cell cell = cells[cell_index];
    G_WorldState.selected_cell = &cell;
    // selectedCell = &cells[cell_index];

    offset += snprintf(log + offset, sizeof(log) - offset, "CELL %d (%.1f, %.1f), Occ. %d, Val. %.1f  --> ", cell_index, cell.coords.x, cell.coords.y, cell.occupancy, cell.value);

    // The cell stores the object IDs of occupying objects, the object/s can then be retrieved by ID.
    // int object_ids[] = cell.object_ids;

    // Check World objects for the object with the same ID as the one in the cell and print its properties if found
    Polygonoid *objs = (Polygonoid *)world.objects.items;
    Vector2d click_to_cell_dist = VectorSum_2d(VectorScale_2d(cell.coords, -1), click_world_coords);
    float click_to_cell_mag = fabs(VectorMagnitude_2d(click_to_cell_dist));
    Polygonoid *p_closest = NULL;
    for (int i = 0; i < MAX_CELL_OCCUPANCY; i++)
    {
        int id = cell.object_ids[i];
        if (id == 0)
        {
            // No object Id  at this index yet (still at initialised value).
            continue;
        }
        if (id < 0)
        {
            printf("ERROR. Object Id stored in Cell is < 0 (%.1f)\n", id);
            continue;
        }

        Polygonoid *p = &objs[id - 1]; // Minus 1 because the Id starts at 1, not 0;

        if (p->id == cell.object_ids[i])
        {
            Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(p->newtonian_properties.coords_origin, -1), click_world_coords);
            float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
            p_closest = click_to_obj_mag < click_to_cell_mag ? p : p_closest;
            // Check if the object's distance to the clicked coords is smaller than the previous and overwrite if so
            offset += snprintf(log + offset, sizeof(log) - offset, "Object %d: ID = %d (%.1f, %.1f), ", i + 1, p->id, p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y);
        }
        else
        {
            printf("ERROR. Object Id stored in Cell doesn't match the Id in the object OR the array index-object Id no longer match. Id in Cell = %d. Id in Object = %d.\n", id, p->id);
            continue;
        }

        // for (size_t j = 0; j < world.objects.coll.count; j++)
        // {
        //     Polygonoid *p = &objs[j];

        //     if (p->id == cell.object_ids[i])
        //     {
        //         Vector2d click_to_obj_dist = VectorSum_2d(VectorScale_2d(p->newtonian_properties.coords_origin, -1), click_world_coords);
        //         float click_to_obj_mag = fabs(VectorMagnitude_2d(click_to_obj_dist));
        //         p_closest = click_to_obj_mag < click_to_cell_mag ? p : p_closest;
        //         // Check if the object's distance to the clicked coords is smaller than the previous and overwrite if so
        //         offset += snprintf(log + offset, sizeof(log) - offset, "Object %d: ID = %d (%.1f, %.1f), ", i + 1, p->id, p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y);

        //         // printf("Object with ID %d is in the cell. Object properties - Position: (%.1f, %.1f), Velocity: (%.1f, %.1f), Acceleration: (%.1f, %.1f)\n",
        //         //        p->newtonian_properties.id,
        //         //        p->newtonian_properties.coords_origin.x, p->newtonian_properties.coords_origin.y,
        //         //        p->newtonian_properties.velocity.x, p->newtonian_properties.velocity.y,
        //         //        p->newtonian_properties.acceleration.x, p->newtonian_properties.acceleration.y);
        //     }
        // }
    }
    selectedObject = p_closest; // Set the selected object to the one in the cell that was clicked, so that its properties can be displayed in the panel
    G_WorldState.selected_object = p_closest;
    if (!p_closest)
    {
        offset += snprintf(log + offset, sizeof(log) - offset, "Object: Nill");
    }
    else
    {
        offset += snprintf(log + offset, sizeof(log) - offset, "SELECTED Object: ID = %d (%.1f, %.1f)", p_closest->id, p_closest->newtonian_properties.coords_origin.x, p_closest->newtonian_properties.coords_origin.y);
    }
    printf("CLICKED (%d, %d) | { %s }\n", mouse_x, mouse_x, log);
}

void CreateAddPolygonoid_Circle(float radius, float mass, ColourRgba colour, Vector2d origin, Vector2d velocity, Vector2d acceleration)
{
    Polygonoid new_polygonoid = CreatePolygonoid_Symmetric(7, radius, colour, mass, origin, velocity, acceleration);

    AddObjectToWorld(&world, &new_polygonoid);

    // Array_Push(polygonoids, &newPolygonoid);
}

// Gameplay Screen Draw logic
void DrawGameWorld()
{
    // 1. Draw the side panel
    // DrawPanelRegion(lpanel_space, (Color){lpanel_fill_colour.r, lpanel_fill_colour.g, lpanel_fill_colour.b, lpanel_fill_colour.a});

    // 2. Draw the game world
    DrawWorldRegion(&world, camera_world);
    // DrawWorldRegion(panelWidth, 0, stageWidth, stageHeight, (Color){stageBackgroundColour.r, stageBackgroundColour.g, stageBackgroundColour.b, stageBackgroundColour.a});
}

void DrawWorldRegion(World2d *world, Camera2d camera)
{
    // Draw the world's coordinate space
    // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
    DrawWorldCoordinateGrid();
    // Draw objects in the world (circloids, polygonoids, etc.)
    DrawPolygonoids(&world->objects);
}

void DrawWorldCoordinateGrid()
{
    if (!world.coord_space_grid.coord_space.cells.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
    {
        return; // No field to draw
    }

    // Need to convert world coordinates to screen coordinates
    Basis2d basis = world.coord_space_grid.coord_space.basis;

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
    // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d origin = world.coord_space_grid.coord_space.coords_origin;
    Vector2d end = VectorSum_2d(origin, world.coord_space_grid.coord_space.resolution_ixj);

    // Transform local space position to pixel space
    Vector2d world_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, origin);
    Vector2d world_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, end);

    // First: Draw background
    ColourRgba colour_fill = world.coord_space_grid.colour_fill;
    ColourRgba colour_line = world.coord_space_grid.colour_line;
    DrawRectangle(world_pixel_origin.x,
                  world_pixel_origin.y,
                  fabsf(world_pixel_end.x - world_pixel_origin.x),
                  fabsf(world_pixel_end.y - world_pixel_origin.y),
                  (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

    // Need to know how the unit steps to take in each direction
    int stepsU = world.coord_space_grid.coord_space.stepsU; // ceilf((float)world_space.resolution_ixj.x / VectorMagnitude_2d(basis.u));
    int stepsV = world.coord_space_grid.coord_space.stepsV; // VectorMagnitude_2d(basis.v));

    // 1. Draw "Horizontal-ish" lines (along the U direction)
    // Create a line at every 'v' step that spans the entire 'u' width
    ColourRgba colour = world.coord_space_grid.colour_line;
    Vector2d line_origin, line_end = {0};
    Vector2d line_pixel_origin, line_pixel_end = {0};
    for (int j = 0; j <= stepsV; j++)
    {
        // Define the line in LOCAL coordinates (simple units)
        // Line i starts at (i, 0) and goes to (i, stepsV)
        line_origin = (Vector2d){origin.x, (float)j};
        line_end = (Vector2d){(float)stepsU, (float)j};

        // The Matrix handles everything:
        // It applies World Position (Origin), Rotation, and Scale in one go.
        line_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, line_origin);
        line_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, line_end);
        // Vector2d screenStart = WorldToScreenCoordinates(screen_basis_transform, localStart);
        // Vector2d screenEnd = WorldToScreenCoordinates(screen_basis_transform, localEnd);

        // Draw
        DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
                  (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
    }

    // 2. Draw "Vertical-ish" lines (along the V direction)
    // We create a line at every 'u' step that spans the entire 'v' height
    for (int i = 0; i <= stepsU; i++)
    {
        // Define the line in LOCAL coordinates (simple units)
        // Line i starts at (i, 0) and goes to (i, stepsV)
        line_origin = (Vector2d){(float)i, origin.y};
        line_end = (Vector2d){(float)i, (float)stepsV};

        // The Matrix handles everything:
        // It applies World Position (Origin), Rotation, and Scale in one go.
        Vector2d line_pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, line_origin);
        Vector2d line_pixel_end = TransformCoordinates(camera_world.source_to_dest_mtx, line_end);
        // Vector2d localStart = {(double)i, 0.0};
        // Vector2d localEnd = {(double)i, (double)stepsV};

        // Draw
        DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
                  (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
    }

    // Draw values as text on top of each field unit
    int totalUnits = stepsU * stepsV; // (int)ceilf(totalArea / cellArea);
    DArray cells = world.coord_space_grid.coord_space.cells;

    for (int k = 0; k < totalUnits; k++)
    {
        int i = k / stepsU; // Row index (based on horizontal lines)
        int j = k % stepsU; // Column index (based on vertical lines)
        Cell *cell = (Cell *)((char *)cells.items + (k * cells.elem_bytes));
        Vector2d cell_coords = cell->coords;
        Vector2d cell_pixel_coords = TransformCoordinates(camera_world.source_to_dest_mtx, cell_coords);
        const char *displayText = TextFormat(" %d (%d,%d)\n (%.0f,%.0f)\n", k, i, j, cell_pixel_coords.x, cell_pixel_coords.y);
        // const char *displayText = TextFormat(" %d (%d,%d)\n (%0.0f,%0.0f)\n", k + 1, i + 1, j + 1, cell_pixel_coords.x, cell_pixel_coords.y);
        //  const char *displayText = TextFormat("Cell: %d (%d,%d)\nWorldCoord: (%d,%d)\nScreenCoord: (%d,%d)\nValue: %.1f", k + 1, i + 1, j + 1, (int)cell_world_coords.x, (int)cell_world_coords.y, cell->value);
        //   DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
        DrawTextEx(font, displayText, (Vector2){cell_pixel_coords.x, cell_pixel_coords.y}, 16, 1, (Color){world_text_colour.r, world_text_colour.g, world_text_colour.b, world_text_colour.a});

        // Debug print
        // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
    }
    // printf("Drew %d cells\n", count);
}

void DrawPolygonoids(LArray *polygonoids)
{
    if (polygonoids == NULL)
    {
        return; // Nothing to draw
    }
    // Collection *coll = &polygonoids->coll;
    for (int i = 0; i < polygonoids->count; i++)
    {
        Polygonoid polygonoid = *((Polygonoid *)((char *)polygonoids->items + (i * polygonoids->elem_bytes)));
        Vector2d obj_center_coords = polygonoid.newtonian_properties.coords_center; // the top left
        Vector2d basis_scale = BasisTransform_2d_Scale(camera_world.source_basis, camera_world.destination_basis);

        // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

        // Draw polygonoid THEN text so text is on top
        // Get origin-offset coordinates as they are only relative vectors with no origin offset
        LArray surf_vectors = polygonoid.newtonian_properties.surface.surface_vectors;
        DrawObjectVertices(surf_vectors, obj_center_coords, camera_world, polygonoid.colourRgba);

        // Debug - draw the bounding box of the polygonoid to check it is correct
        Surface2d obj_box_surface = CreateSurface_Rectangular(polygonoid.newtonian_properties.boxed_dimensions);
        DrawObjectVertices(obj_box_surface.surface_vectors, obj_center_coords, camera_world, polygonoid.colourRgba);
        ClearLArray(&obj_box_surface.surface_vectors);
    
        // Debug - draw the footprint box of the polygonoid to check it is correct
        Matrix2x2 footprint_coords = GetObjectFootprint_AsBox(camera_world.source_basis, polygonoid.newtonian_properties.surface);
        Vector2d footprint_box_dimensions = (Vector2d){footprint_coords.col2.x - footprint_coords.col1.x, footprint_coords.col2.y - footprint_coords.col1.y};
        Surface2d footprint_surface = CreateSurface_Rectangular(footprint_box_dimensions);
        DrawObjectVertices(footprint_surface.surface_vectors, obj_center_coords, camera_world, polygonoid.colourRgba);
        ClearLArray(&footprint_surface.surface_vectors);

         // float cell_index = ((int)local_origin_coords.y * world_resolution.x) + (int)local_origin_coords.x; // - 1; ((screen_origin.x - 1) * world_resolution.y) + screen_origin.y;
        // const char *display_text = TextFormat("%.0f", cell_index);
        // Vector2d pixel_origin = TransformCoordinates(camera_world.source_to_dest_mtx, local_origin_coords);
        // float radius_mag_pixel = VectorMagnitude_2d(basis_scale) * polygonoid.radius; // Assuming orthogonal coordinatea
        // Vector2d text_pixel_cords = {pixel_origin.x - (0.6 * radius_mag_pixel), pixel_origin.y - (0.3 * radius_mag_pixel)};
        // DrawTextEx(font, display_text, (Vector2){text_pixel_cords.x, text_pixel_cords.y}, 20, 1, (Color){polygonoid_text_colour.r,
        // Debug print
        // printf("Drew Polygonoid %d at Coords (%.1f, %.1f), Pixel (%.1f, %.1f)\n", i, screen_origin.x, screen_origin.y, pixel_origin.x, pixel_origin.y);
    }
}

// Provide the boxed coords of the object and the object's vertices to render the vertices within the box
void DrawObjectVertices(LArray local_vertices, Vector2d local_offset, Camera2d camera, ColourRgba line_colour)
{
    Vector2d vertice_start = *((Vector2d *)local_vertices.items);
    vertice_start = VectorSum_2d(vertice_start, local_offset); // apply positional offset
    vertice_start = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_start);
    Vector2d vertice_start_cache = vertice_start;
    for (int i = 1; i < local_vertices.count; i++)
    {
        Vector2d vertice_end = *(Vector2d *)((char *)local_vertices.items + (i * sizeof(Vector2d)));
        vertice_end = VectorSum_2d(vertice_end, local_offset);
        vertice_end = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_end);
        DrawLine(vertice_start.x, vertice_start.y, vertice_end.x, vertice_end.y, (Color){line_colour.r, line_colour.g, line_colour.b, line_colour.a});

        // Current end vertice is used as the starting vertice for the next line, so recycle it
        vertice_start = vertice_end;
    }

    // Draw the line from vertice[0] to vertice[count-1];
    DrawLine(vertice_start.x, vertice_start.y, vertice_start_cache.x, vertice_start_cache.y, (Color){polygonoid_line_colour.r, polygonoid_line_colour.g, polygonoid_line_colour.b, polygonoid_line_colour.a});
}

void UpdatePolygonoidVectors(DArray *polygonoids)
{
    // Update Polygonoids
    if (polygonoids == NULL || polygonoids->count <= 0)
    {
        return; // Nothing to update
    }
    Polygonoid *p = Enumerate(polygonoids);
    if (p == NULL)
    {
        fprintf(stderr, "Failed to retrieve enumerated Polygonoid\n"); // Enumerator failed to retrieve the first item
    }
    while (p != NULL)
    {
        if (&p->newtonian_properties != NULL)
        {
            CalculateVectors(&p->newtonian_properties, frame_counter.delta_time);
        }
        p = Enumerate(polygonoids);
    }
    ResetEnumerator(polygonoids); // Reset enumerator after drawing
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