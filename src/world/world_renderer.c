/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "common/common.h"
#include "editor/geometry_editor.h"
#include "camera/camera.h"
#include "system/draw_primitives.h"
#include "system/debug_overlay_system.h"
#include "system/systems.h"
#include "world/world.h"
#include "world/world_internal.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx);
void DrawCollisions(LArray *collisions, Matrix3x3 space_to_pixel_mtx);
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset,
                        Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour,
                        ColourRgba fill_colour);
void DrawRotatedObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset,
                               Vector2d axis_x, Vector2d axis_y,
                               Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour,
                               ColourRgba fill_colour);
void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 space_to_pixel_mtx);

static void DrawNewtonoidAxes(const Newtonoid2d *newtonoid, Matrix3x3 space_to_pixel_mtx)
{
    if (!newtonoid || !IsDebugEnabled(DEBUG_OBJECT_AXES))
    {
        return;
    }

    // Scale each local basis arrow from its corresponding local AABB dimension.
    float x_axis_length = fmaxf(newtonoid->bounds_size.x * 0.75f, 0.5f);
    float y_axis_length = fmaxf(newtonoid->bounds_size.y * 0.75f, 0.5f);

    Vector2d center = newtonoid->anchor_position;
    Vector2d x_axis_end = VectorSum_2d(center, VectorScale_2d(newtonoid->local_axis_x, x_axis_length));
    Vector2d y_axis_end = VectorSum_2d(center, VectorScale_2d(newtonoid->local_axis_y, y_axis_length));

    DrawTransformedArrowV(center, x_axis_end, space_to_pixel_mtx, COLOUR_GAME_AXIS_X_RGBA);
    DrawTransformedArrowV(center, y_axis_end, space_to_pixel_mtx, COLOUR_GAME_AXIS_Y_RGBA);
}

void DrawWorldRegion(World2d *world, Camera2d *universe_camera)
{
    Matrix3x3 world_to_pixel_mtx = ResolveWorldToPixelMatrix(world, universe_camera);

    DrawGridSpace(&world->grid_space, world_to_pixel_mtx);
    DrawNewtonoids(&world->objects, world_to_pixel_mtx);
    DrawNewtonoids(&world->temp_objects, world_to_pixel_mtx);
    GeometryEditor_DrawHandles(world, universe_camera);
    // DrawCollisions(&world->collisions, world_to_pixel_mtx);
}

void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 world_to_pixel_mtx)
{
    if (grid_space->space.cells.capacity < 1)
    {
        return;
    }

    // Grid geometry is authored in world-local coordinates; the world basis and
    // placement are applied once by world_to_pixel_mtx.
    Vector2d origin = ZERO_VECTOR_2D;

    Vector2d corner_local_0 = origin;
    Vector2d corner_local_1 = (Vector2d){(float)grid_space->space.columns, 0.0f};
    Vector2d corner_local_2 = (Vector2d){(float)grid_space->space.columns, (float)grid_space->space.rows};
    Vector2d corner_local_3 = (Vector2d){0.0f, (float)grid_space->space.rows};

    Vector2d corner_pixel_0 = TransformCoordinates(world_to_pixel_mtx, corner_local_0);
    Vector2d corner_pixel_1 = TransformCoordinates(world_to_pixel_mtx, corner_local_1);
    Vector2d corner_pixel_2 = TransformCoordinates(world_to_pixel_mtx, corner_local_2);
    Vector2d corner_pixel_3 = TransformCoordinates(world_to_pixel_mtx, corner_local_3);

    ColourRgba colour_fill = grid_space->colour_fill;
    ColourRgba colour_line = grid_space->colour_line;
    Color fill = ToRaylibColor(colour_fill);
    DrawTriangle((Vector2){corner_pixel_2.x, corner_pixel_2.y},
                 (Vector2){corner_pixel_1.x, corner_pixel_1.y},
                 (Vector2){corner_pixel_0.x, corner_pixel_0.y},
                 fill);
    DrawTriangle((Vector2){corner_pixel_3.x, corner_pixel_3.y},
                 (Vector2){corner_pixel_2.x, corner_pixel_2.y},
                 (Vector2){corner_pixel_0.x, corner_pixel_0.y},
                 fill);

    if (!IsDebugEnabled(DEBUG_WORLD_GRID))
    {
        return;
    }

    int columns = grid_space->space.columns;
    int rows = grid_space->space.rows;

    ColourRgba colour = grid_space->colour_line;
    for (int j = 0; j <= rows; j++)
    {
        Vector2d row_offset = (Vector2d){0.0f, (float)j};
        Vector2d width_extent = (Vector2d){(float)columns, 0.0f};

        Vector2d line_origin = VectorSum_2d(origin, row_offset);
        Vector2d line_end = VectorSum_2d(line_origin, width_extent);
        DrawTransformedLineV(line_origin, line_end, world_to_pixel_mtx, colour);
    }

    for (int i = 0; i <= columns; i++)
    {
        Vector2d column_offset = (Vector2d){(float)i, 0.0f};
        Vector2d height_extent = (Vector2d){0.0f, (float)rows};

        Vector2d line_origin = VectorSum_2d(origin, column_offset);
        Vector2d line_end = VectorSum_2d(line_origin, height_extent);
        DrawTransformedLineV(line_origin, line_end, world_to_pixel_mtx, colour);
    }

    int totalUnits = columns * rows;
    DArray cells = grid_space->space.cells;
    Color text_colour = ToRaylibColor(COLOUR_GAME_PARCHMENT_RGBA);

    if (!world_grid_debug_labels_enabled)
    {
        return;
    }

    Vector2d p00 = TransformCoordinates(world_to_pixel_mtx, origin);
    Vector2d p10 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){1.0f, 0.0f});
    Vector2d p01 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){0.0f, 1.0f});
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
        Vector2d obj_center_coords = newtonoid->anchor_position;
        LArray surf_vectors = newtonoid->surface.surface_vectors;
        DrawRotatedObjectVertices(surf_vectors.items, surf_vectors.count, obj_center_coords,
                                  newtonoid->local_axis_x, newtonoid->local_axis_y,
                                  space_to_pixel_mtx, newtonoid->line_colour,
                                  newtonoid->fill_colour);
        DrawNewtonoidAxes(newtonoid, space_to_pixel_mtx);
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
        Vector2d anchor_position = CalcGeometricCentre_FromBox(*collision_box);
        CalcBoxVertices(dimensions, anchor_position, collision_vertices);
        DrawObjectVertices(collision_vertices, 4, ZERO_VECTOR_2D, space_to_pixel_mtx,
                           COLOUR_GAME_INK_RGBA, COLOUR_GAME_OLIVE_RGBA);
    }
}
void DrawObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset,
                        Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour,
                        ColourRgba fill_colour)
{
    if (local_vertices == NULL || vertices_count < 2)
    {
        return;
    }

    Vector2d vertice_start = VectorSum_2d(local_vertices[0], offset);
    Vector2d vertice_start_cache = vertice_start;
    Color colour = ToRaylibColor(line_colour);
    Color fill = ToRaylibColor(fill_colour);

    Vector2 fill_origin = {
        TransformCoordinates(space_to_pixel_mtx, vertice_start).x,
        TransformCoordinates(space_to_pixel_mtx, vertice_start).y};
    for (int i = 1; i < vertices_count - 1; i++)
    {
        Vector2d vertex_a = VectorSum_2d(local_vertices[i], offset);
        Vector2d vertex_b = VectorSum_2d(local_vertices[i + 1], offset);
        Vector2d pixel_a = TransformCoordinates(space_to_pixel_mtx, vertex_a);
        Vector2d pixel_b = TransformCoordinates(space_to_pixel_mtx, vertex_b);
        DrawTriangle(fill_origin, (Vector2){pixel_b.x, pixel_b.y},
                     (Vector2){pixel_a.x, pixel_a.y}, fill);
    }

    for (int i = 1; i < vertices_count; i++)
    {
        Vector2d vertice_end = VectorSum_2d(local_vertices[i], offset);
        DrawTransformedLineV(vertice_start, vertice_end, space_to_pixel_mtx, line_colour);

        vertice_start = vertice_end;
    }

    DrawTransformedLineV(vertice_start, vertice_start_cache, space_to_pixel_mtx, line_colour);
}

void DrawRotatedObjectVertices(Vector2d *local_vertices, int vertices_count, Vector2d offset,
                               Vector2d axis_x, Vector2d axis_y,
                               Matrix3x3 space_to_pixel_mtx, ColourRgba line_colour,
                               ColourRgba fill_colour)
{
    if (local_vertices == NULL || vertices_count < 2)
    {
        return;
    }

    // Rebuild the vertex list in world space using the object's current basis.
    Vector2d world_vertices[MAX_SHAPE_VERTICES];
    if (vertices_count > MAX_SHAPE_VERTICES)
    {
        vertices_count = MAX_SHAPE_VERTICES;
    }

    for (int i = 0; i < vertices_count; i++)
    {
        // Map the local vertex through the object's basis into world space.
        Vector2d local_vertex = local_vertices[i];
        world_vertices[i].x = (local_vertex.x * axis_x.x) + (local_vertex.y * axis_y.x) + offset.x;
        world_vertices[i].y = (local_vertex.x * axis_x.y) + (local_vertex.y * axis_y.y) + offset.y;
    }

    DrawObjectVertices(world_vertices, vertices_count, ZERO_VECTOR_2D,
                       space_to_pixel_mtx, line_colour, fill_colour);
}
