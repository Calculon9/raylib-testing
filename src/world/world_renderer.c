/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "common/common.h"
#include "camera/camera.h"
#include "system/systems.h"
#include "system/world_system.h"
#include "common/common.h"
#include "world/world.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

void DrawNewtonoids(LArray *newtonoids);
void DrawCollisions(LArray *collisions);
// void DrawObjectVertices(LArray local_vertices, Vector2d coords_center, Camera2d camera, ColourRgba line_colour);
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset, Camera2d camera, ColourRgba line_colour);
void DrawWorldCoordinateGrid(CoordSpace2d_Grid *coord_space_grid, Camera2d *camera_world);

void DrawWorldRegion(World2d *world, Camera2d *camera_world)
{
    // Draw the world's coordinate space
    // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
    DrawWorldCoordinateGrid(&world->coord_space_grid, camera_world);
    // Draw objects in the world (circloids, polygonoids, etc.)
    DrawNewtonoids(&world->objects);
    DrawNewtonoids(&world->temp_objects);
    // DrawCollisions(&world->collisions);
}

void DrawWorldCoordinateGrid(CoordSpace2d_Grid *coord_space_grid, Camera2d *camera_world)
{
    if (coord_space_grid->coord_space.cells.capacity < 1)
    {
        return; // No field to draw
    }

    // Need to convert world coordinates to screen coordinates
    Basis2d basis = coord_space_grid->coord_space.basis;

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0).
    // But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d origin = coord_space_grid->coord_space.coords_origin;
    Vector2d end = VectorSum_2d(origin, coord_space_grid->coord_space.resolution_ixj);

    // Transform local space position to pixel space
    Vector2d world_pixel_origin = TransformCoordinates(camera_world->source_to_dest_mtx, origin);
    Vector2d world_pixel_end = TransformCoordinates(camera_world->source_to_dest_mtx, end);

    // First: Draw background
    ColourRgba colour_fill = coord_space_grid->colour_fill;
    ColourRgba colour_line = coord_space_grid->colour_line;
    DrawRectangle(world_pixel_origin.x,
                  world_pixel_origin.y,
                  fabsf(world_pixel_end.x - world_pixel_origin.x),
                  fabsf(world_pixel_end.y - world_pixel_origin.y),
                  (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a});

    // Need to know how the unit steps to take in each direction
    int stepsU = coord_space_grid->coord_space.stepsU; // ceilf((float)world_space.resolution_ixj.x / VectorMagnitude_2d(basis.u));
    int stepsV = coord_space_grid->coord_space.stepsV; // VectorMagnitude_2d(basis.v));

    // Draw Horizontal-ish lines (along the U direction)
    // Create a line at every 'v' step that spans the entire 'u' width
    ColourRgba colour = coord_space_grid->colour_line;
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
        line_pixel_origin = TransformCoordinates(camera_world->source_to_dest_mtx, line_origin);
        line_pixel_end = TransformCoordinates(camera_world->source_to_dest_mtx, line_end);
        // Vector2d screenStart = WorldToScreenCoordinates(screen_basis_transform, localStart);
        // Vector2d screenEnd = WorldToScreenCoordinates(screen_basis_transform, localEnd);

        // Draw
        DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
                  (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
    }

    // Draw Vertical-ish lines (along the V direction)
    // We create a line at every 'u' step that spans the entire 'v' height
    for (int i = 0; i <= stepsU; i++)
    {
        // Define the line in LOCAL coordinates (simple units)
        // Line i starts at (i, 0) and goes to (i, stepsV)
        line_origin = (Vector2d){(float)i, origin.y};
        line_end = (Vector2d){(float)i, (float)stepsV};

        // The Matrix handles everything:
        // It applies World Position (Origin), Rotation, and Scale in one go.
        Vector2d line_pixel_origin = TransformCoordinates(camera_world->source_to_dest_mtx, line_origin);
        Vector2d line_pixel_end = TransformCoordinates(camera_world->source_to_dest_mtx, line_end);
        // Vector2d localStart = {(double)i, 0.0};
        // Vector2d localEnd = {(double)i, (double)stepsV};

        // Draw
        DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
                  (Vector2){line_pixel_end.x, line_pixel_end.y}, (Color){colour.r, colour.g, colour.b, colour.a});
    }

    // Draw values as text on top of each field unit
    int totalUnits = stepsU * stepsV; // (int)ceilf(totalArea / cellArea);
    DArray cells = coord_space_grid->coord_space.cells;
    Color text_colour = (Color){COLOUR_WORLD__XIGHT_1.r, COLOUR_WORLD__XIGHT_1.g, COLOUR_WORLD__XIGHT_1.b, COLOUR_WORLD__XIGHT_1.a};
    for (int k = 0; k < totalUnits; k++)
    {
        int i = k / stepsU; // Row index (based on horizontal lines)
        int j = k % stepsU; // Column index (based on vertical lines)
        Cell *cell = (Cell *)((char *)cells.items + (k * cells.elem_bytes));
        Vector2d cell_coords = cell->coords_origin;
        Vector2d cell_pixel_coords = TransformCoordinates(camera_world->source_to_dest_mtx, cell_coords);
        const char *displayText = TextFormat(" %d (%d,%d)\n (%.0f,%.0f)\n", k, i, j, cell_pixel_coords.x, cell_pixel_coords.y);
        // const char *displayText = TextFormat(" %d (%d,%d)\n (%0.0f,%0.0f)\n", k + 1, i + 1, j + 1, cell_pixel_coords.x, cell_pixel_coords.y);
        //  const char *displayText = TextFormat("Cell: %d (%d,%d)\nWorldCoord: (%d,%d)\nScreenCoord: (%d,%d)\nValue: %.1f", k + 1, i + 1, j + 1, (int)cell_world_coords.x, (int)cell_world_coords.y, cell->value);
        //   DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

        DrawTextEx(font, displayText, (Vector2){cell_pixel_coords.x, cell_pixel_coords.y}, 16, 1, text_colour);

        // Debug print
        // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
    }
    // printf("Drew %d cells\n", count);
}

void DrawNewtonoids(LArray *newtonoids)
{
    if (newtonoids == NULL)
    {
        return; // Nothing to draw
    }
    // Collection *coll = &polygonoids->coll;
    for (int i = 0; i < newtonoids->count; i++)
    {
        Newtonoid2d newtonoid = *((Newtonoid2d *)((char *)newtonoids->items + (i * newtonoids->elem_bytes)));
        Vector2d obj_center_coords = newtonoid.coords_center;

        // ----DEBUG----- draw the bounding box of the polygonoid to check it is correct
        Surface2d obj_box_surface = CreateSurface_Rectangular(newtonoid.boxed_dimensions, ZERO_VECTOR_2D);
        //DrawObjectVertices(obj_box_surface.surface_vectors.items, obj_box_surface.surface_vectors.count, obj_center_coords, camera_world, OLIVE_GARDEN_GREEN_XL);
        ClearLArray(&obj_box_surface.surface_vectors);

        // Draw polygonoid THEN text so text is on top
        // Get origin-offset coordinates as they are only relative vectors with no origin offset
        LArray surf_vectors = newtonoid.surface.surface_vectors;
        DrawObjectVertices(surf_vectors.items, surf_vectors.count, obj_center_coords, camera_world, newtonoid.line_colour);

        // ----DEBUG-----  draw the footprint box of the polygonoid to check it is correct
        // LArray footprint_vertices = CalcSnappedAABB(camera_world.source_basis, polygonoid.newtonian_properties.surface.surface_vectors, obj_center_coords);
        // Matrix2x2 snapped_aabb_box = CalcAABBCoords_Tight(&footprint_vertices, ZERO_VECTOR_2D);
        // DrawObjectVertices(footprint_vertices, ZERO_VECTOR_2D, camera_world, polygonoid.colourRgba); // No object offset needed as its vertices have been snapped using object center coords
        // ClearLArray(&footprint_vertices);

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

void DrawCollisions(LArray *collisions)
{
    if (collisions == NULL)
    {
        return; // Nothing to draw
    }
    // Collection *coll = &polygonoids->coll;
    for (int i = 0; i < collisions->count; i++)
    {
        Matrix2x2 collision_box = *((Matrix2x2 *)((char *)collisions->items + (i * collisions->elem_bytes)));

        // ----DEBUG----- draw the collision box to check it is correct
        Vector2d collision_vertices[4] = {0};
        Vector2d dimensions = {collision_box.col2.x - collision_box.col1.x, collision_box.col2.y - collision_box.col1.y};
        // Vector2d offset = {(collision_box.col1.x + collision_box.col2.x) * 0.5, (collision_box.col1.y + collision_box.col2.y) * 0.5}; // {collision_box.col1.x, collision_box.col1.y};
        Vector2d coords_center = CalcGeometricCentre_FromBox(collision_box);
        CalcBoxVertices(dimensions, coords_center, collision_vertices); // {collision_box.col1, collision_box.col2, collision_box.col2, collision_box.col1}; // Create a rect from the box coords = CreateSurface_Rectangular(collision_box, ZERO_VECTOR_2D);
        DrawObjectVertices(collision_vertices, 4, ZERO_VECTOR_2D, camera_world, OLIVE_GARDEN_GREEN_D);
    }
}

// Provide the boxed coords of the object and the object's vertices to render the vertices within the box
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset, Camera2d camera, ColourRgba line_colour)
{
    // Vector2d vertice_start = *((Vector2d *)local_vertices.items);
    Vector2d vertice_start = local_vertices[0];
    vertice_start = VectorSum_2d(vertice_start, offset); // apply positional offset
    vertice_start = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_start);
    Vector2d vertice_start_cache = vertice_start;
    for (int i = 1; i < vertices_count; i++)
    {
        // Vector2d vertice_end = *(Vector2d *)((char *)local_vertices.items + (i * sizeof(Vector2d)));
        // Vector2d vertice_end = *(Vector2d *)LArray_Get(&local_vertices, i);
        Vector2d vertice_end = local_vertices[i];
        vertice_end = VectorSum_2d(vertice_end, offset);
        vertice_end = TransformCoordinates(camera_world.source_to_dest_mtx, vertice_end);
        DrawLine(vertice_start.x, vertice_start.y, vertice_end.x, vertice_end.y, (Color){line_colour.r, line_colour.g, line_colour.b, line_colour.a});

        // Current end vertice is used as the starting vertice for the next line, so recycle it
        vertice_start = vertice_end;
    }

    // Draw the line from vertice[0] to vertice[count-1];
    DrawLine(vertice_start.x, vertice_start.y, vertice_start_cache.x, vertice_start_cache.y, (Color){line_colour.r, line_colour.g, line_colour.b, line_colour.a});
}