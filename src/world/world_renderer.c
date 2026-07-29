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
#include "world/world.h"
#include "world/world.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx);
void DrawCollisions(LArray *collisions, Matrix3x3 space_to_pixel_mtx);
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset, Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour);
void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 space_to_pixel_mtx);

static Color ToRaylibColor(ColourRgba colour)
{
    return (Color){colour.r, colour.g, colour.b, colour.a};
}

static void TransformLineEndpoints(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, Vector2d *out_start, Vector2d *out_end)
{
    if (out_start != NULL)
    {
        *out_start = TransformCoordinates(world_to_pixel_mtx, start);
    }

    if (out_end != NULL)
    {
        *out_end = TransformCoordinates(world_to_pixel_mtx, end);
    }
}

static void DrawTransformedLineV(Vector2d start, Vector2d end, Matrix3x3 world_to_pixel_mtx, ColourRgba line_colour)
{
    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(start, end, world_to_pixel_mtx, &line_pixel_origin, &line_pixel_end);

    DrawLineV((Vector2){line_pixel_origin.x, line_pixel_origin.y},
              (Vector2){line_pixel_end.x, line_pixel_end.y},
              ToRaylibColor(line_colour));
}

void DrawWorldRegion(World2d *world, Camera2d *universe_camera)
{
    Matrix3x3 world_to_game_viewport_mtx = MatrixMultiply_3x3_3x3(universe_camera->tunnel.source_to_dest_mtx,
                                                                  world->tunnel.source_to_dest_mtx);
    Matrix3x3 world_to_pixel_mtx = MatrixMultiply_3x3_3x3(game_viewport.tunnel.source_to_dest_mtx,
                                                          world_to_game_viewport_mtx);

    DrawGridSpace(&world->grid_space, world_to_pixel_mtx);
    DrawNewtonoids(&world->objects, world_to_pixel_mtx);
    DrawNewtonoids(&world->temp_objects, world_to_pixel_mtx);
    // DrawCollisions(&world->collisions, world_to_pixel_mtx);
}

void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 world_to_pixel_mtx)
{
    if (grid_space->space.cells.capacity < 1)
    {
        return;
    }

    Basis2d basis = grid_space->space.frame.basis;

    // Grid vertices are authored in world-local coordinates; world placement is
    // already encoded in world_to_pixel_mtx via world->tunnel.source_to_dest_mtx.
    Vector2d origin = ZERO_VECTOR_2D;
    Vector2d end = VectorSum_2d(origin, (Vector2d){(float)grid_space->space.columns, (float)grid_space->space.rows});

    Vector2d corner_local_0 = origin;
    Vector2d corner_local_1 = VectorSum_2d(origin, VectorScale_2d(basis.u, (float)grid_space->space.columns));
    Vector2d corner_local_2 = VectorSum_2d(corner_local_1, VectorScale_2d(basis.v, (float)grid_space->space.rows));
    Vector2d corner_local_3 = VectorSum_2d(origin, VectorScale_2d(basis.v, (float)grid_space->space.rows));

    Vector2d corner_pixel_0 = TransformCoordinates(world_to_pixel_mtx, corner_local_0);
    Vector2d corner_pixel_1 = TransformCoordinates(world_to_pixel_mtx, corner_local_1);
    Vector2d corner_pixel_2 = TransformCoordinates(world_to_pixel_mtx, corner_local_2);
    Vector2d corner_pixel_3 = TransformCoordinates(world_to_pixel_mtx, corner_local_3);

    ColourRgba colour_fill = grid_space->colour_fill;
    ColourRgba colour_line = grid_space->colour_line;
    Color fill = (Color){colour_fill.r, colour_fill.g, colour_fill.b, colour_fill.a};
    DrawTriangle((Vector2){corner_pixel_2.x, corner_pixel_2.y},
                 (Vector2){corner_pixel_1.x, corner_pixel_1.y},
                 (Vector2){corner_pixel_0.x, corner_pixel_0.y},
                 fill);
    DrawTriangle((Vector2){corner_pixel_3.x, corner_pixel_3.y},
                 (Vector2){corner_pixel_2.x, corner_pixel_2.y},
                 (Vector2){corner_pixel_0.x, corner_pixel_0.y},
                 fill);

    int columns = grid_space->space.columns;
    int rows = grid_space->space.rows;

    ColourRgba colour = grid_space->colour_line;
    for (int j = 0; j <= rows; j++)
    {
        Vector2d row_offset = VectorScale_2d(basis.v, (float)j);
        Vector2d width_extent = VectorScale_2d(basis.u, (float)columns);

        Vector2d line_origin = VectorSum_2d(origin, row_offset);
        Vector2d line_end = VectorSum_2d(line_origin, width_extent);
        DrawTransformedLineV(line_origin, line_end, world_to_pixel_mtx, colour);
    }

    for (int i = 0; i <= columns; i++)
    {
        Vector2d column_offset = VectorScale_2d(basis.u, (float)i);
        Vector2d height_extent = VectorScale_2d(basis.v, (float)rows);

        Vector2d line_origin = VectorSum_2d(origin, column_offset);
        Vector2d line_end = VectorSum_2d(line_origin, height_extent);
        DrawTransformedLineV(line_origin, line_end, world_to_pixel_mtx, colour);
    }

    int totalUnits = columns * rows;
    DArray cells = grid_space->space.cells;
    Color text_colour = (Color){COLOUR_WORLD__XIGHT_1.r, COLOUR_WORLD__XIGHT_1.g, COLOUR_WORLD__XIGHT_1.b, COLOUR_WORLD__XIGHT_1.a};

    if (!world_grid_debug_labels_enabled)
    {
        return;
    }

    Vector2d p00 = TransformCoordinates(world_to_pixel_mtx, origin);
    Vector2d p10 = TransformCoordinates(world_to_pixel_mtx, VectorSum_2d(origin, basis.u));
    Vector2d p01 = TransformCoordinates(world_to_pixel_mtx, VectorSum_2d(origin, basis.v));
    float cell_px_w = VectorMagnitude_2d(VectorSum_2d(p10, VectorScale_2d(p00, -1.0f)));
    float cell_px_h = VectorMagnitude_2d(VectorSum_2d(p01, VectorScale_2d(p00, -1.0f)));

    if (cell_px_w < 40.0f)
    {
        return;
    }

    for (int k = 0; k < totalUnits; k++)
    {
        int i = k / columns;
        int j = k % columns;
        Cell *cell = (Cell *)((char *)cells.items + (k * cells.elem_bytes));
        Vector2d cell_coords = cell->local_origin;
        Vector2d cell_pixel_coords = TransformCoordinates(world_to_pixel_mtx, cell_coords);
        const char *displayText = TextFormat("%d(%d,%d)\n(%.0f,%.0f)\n", k, i, j, cell_pixel_coords.x, cell_pixel_coords.y);

        DrawTextEx(font, displayText, (Vector2){cell_pixel_coords.x + 2, cell_pixel_coords.y + 2}, 16, 1, text_colour);
    }
}

void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx)
{
    if (!LArray_IsValid(newtonoids))
    {
        return;
    }

    Newtonoid2d *newtonoid;
    LArray_ForEach(newtonoids, Newtonoid2d*, newtonoid)
    {
        Vector2d obj_center_coords = newtonoid->coords_center;
        LArray surf_vectors = newtonoid->surface.surface_vectors;
        DrawObjectVertices(surf_vectors.items, surf_vectors.count, obj_center_coords, space_to_pixel_mtx, newtonoid->line_colour);
    }
}

void DrawCollisions(LArray *collisions, Matrix3x3 space_to_pixel_mtx)
{
    if (!LArray_IsValid(collisions))
    {
        return;
    }

    Matrix2x2 *collision_box;
    LArray_ForEach(collisions, Matrix2x2*, collision_box)
    {
        Vector2d collision_vertices[4] = {0};
        Vector2d dimensions = {collision_box->col2.x - collision_box->col1.x, collision_box->col2.y - collision_box->col1.y};
        Vector2d coords_center = CalcGeometricCentre_FromBox(*collision_box);
        CalcBoxVertices(dimensions, coords_center, collision_vertices);
        DrawObjectVertices(collision_vertices, 4, ZERO_VECTOR_2D, space_to_pixel_mtx, OLIVE_GARDEN_GREEN_D);
    }
}
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset, Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour)
{
    if (local_vertices == NULL || vertices_count < 2)
    {
        return;
    }

    Vector2d vertice_start = VectorSum_2d(local_vertices[0], offset);
    Vector2d vertice_start_cache = vertice_start;
    Color colour = ToRaylibColor(line_colour);

    for (int i = 1; i < vertices_count; i++)
    {
        Vector2d vertice_end = VectorSum_2d(local_vertices[i], offset);
        Vector2d line_pixel_origin = {0};
        Vector2d line_pixel_end = {0};
        TransformLineEndpoints(vertice_start, vertice_end, space_to_pixel_mtx, &line_pixel_origin, &line_pixel_end);

        DrawLine(line_pixel_origin.x,
                 line_pixel_origin.y,
                 line_pixel_end.x,
                 line_pixel_end.y,
                 colour);

        vertice_start = vertice_end;
    }

    Vector2d line_pixel_origin = {0};
    Vector2d line_pixel_end = {0};
    TransformLineEndpoints(vertice_start, vertice_start_cache, space_to_pixel_mtx, &line_pixel_origin, &line_pixel_end);
    DrawLine(line_pixel_origin.x,
             line_pixel_origin.y,
             line_pixel_end.x,
             line_pixel_end.y,
             colour);
}
