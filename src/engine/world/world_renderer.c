/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "common/common.h"
#include "colour/colour.h"
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
static void DrawNewtonoidAxes(const Newtonoid2d *newtonoid, Matrix3x3 space_to_pixel_mtx);
static void DrawNewtonoidHull(const Vector2d *world_vertices, int vertices_count,
                              Matrix3x3 space_to_pixel_mtx);
static void DrawNewtonoidAABB(const Newtonoid2d *newtonoid, Matrix3x3 space_to_pixel_mtx);
void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 space_to_pixel_mtx);

static float CalculatePolygonCross(Vector2d point_a, Vector2d point_b, Vector2d point_c)
{
    Vector2d edge_ab = VectorDiff_2d(point_b, point_a);
    Vector2d edge_ac = VectorDiff_2d(point_c, point_a);
    return (edge_ab.x * edge_ac.y) - (edge_ab.y * edge_ac.x);
}

static bool PointIsInsideTriangle(Vector2d point, Vector2d point_a, Vector2d point_b,
                                  Vector2d point_c, float winding_sign)
{
    const float epsilon = 0.000001f;
    float cross_ab = CalculatePolygonCross(point_a, point_b, point);
    float cross_bc = CalculatePolygonCross(point_b, point_c, point);
    float cross_ca = CalculatePolygonCross(point_c, point_a, point);

    return (cross_ab * winding_sign >= -epsilon) &&
           (cross_bc * winding_sign >= -epsilon) &&
           (cross_ca * winding_sign >= -epsilon);
}

static float CalculatePolygonSignedArea(Vector2d *vertices, int vertices_count)
{
    float signed_area = 0.0f;
    for (int vertex_index = 0; vertex_index < vertices_count; vertex_index++)
    {
        Vector2d current = vertices[vertex_index];
        Vector2d next = vertices[(vertex_index + 1) % vertices_count];
        signed_area += (current.x * next.y) - (next.x * current.y);
    }
    return signed_area * 0.5f;
}

static int CompareVector2dLexicographically(const void *left, const void *right)
{
    const Vector2d *a = (const Vector2d *)left;
    const Vector2d *b = (const Vector2d *)right;

    if (a->x < b->x)
    {
        return -1;
    }
    if (a->x > b->x)
    {
        return 1;
    }
    if (a->y < b->y)
    {
        return -1;
    }
    if (a->y > b->y)
    {
        return 1;
    }
    return 0;
}

static int BuildConvexHull(Vector2d *vertices, int vertices_count, Vector2d *out_hull)
{
    if (!vertices || !out_hull || vertices_count < 3)
    {
        return 0;
    }

    Vector2d sorted_vertices[MAX_SHAPE_VERTICES];
    if (vertices_count > MAX_SHAPE_VERTICES)
    {
        vertices_count = MAX_SHAPE_VERTICES;
    }

    for (int vertex_index = 0; vertex_index < vertices_count; vertex_index++)
    {
        sorted_vertices[vertex_index] = vertices[vertex_index];
    }

    qsort(sorted_vertices, (size_t)vertices_count, sizeof(Vector2d), CompareVector2dLexicographically);

    int hull_count = 0;
    for (int vertex_index = 0; vertex_index < vertices_count; vertex_index++)
    {
        while (hull_count >= 2 &&
               CalculatePolygonCross(out_hull[hull_count - 2], out_hull[hull_count - 1],
                                     sorted_vertices[vertex_index]) <= 0.0f)
        {
            hull_count--;
        }
        out_hull[hull_count++] = sorted_vertices[vertex_index];
    }

    int lower_hull_count = hull_count;
    for (int vertex_index = vertices_count - 2; vertex_index >= 0; vertex_index--)
    {
        while (hull_count > lower_hull_count &&
               CalculatePolygonCross(out_hull[hull_count - 2], out_hull[hull_count - 1],
                                     sorted_vertices[vertex_index]) <= 0.0f)
        {
            hull_count--;
        }
        out_hull[hull_count++] = sorted_vertices[vertex_index];
    }

    if (hull_count > 1)
    {
        hull_count--;
    }

    return hull_count;
}

// Draw the convex collision hull around an object's transformed world vertices.
static void DrawNewtonoidHull(const Vector2d *world_vertices, int vertices_count,
                              Matrix3x3 space_to_pixel_mtx)
{
    if (!world_vertices || vertices_count < 3 || !IsDebugEnabled(DEBUG_OBJECT_HULL))
    {
        return;
    }

    Vector2d hull_vertices[MAX_SHAPE_VERTICES];
    int hull_count = BuildConvexHull((Vector2d *)world_vertices, vertices_count, hull_vertices);
    if (hull_count < 2)
    {
        return;
    }

    for (int vertex_index = 0; vertex_index < hull_count; vertex_index++)
    {
        int next_index = (vertex_index + 1) % hull_count;
        DrawTransformedLineV(hull_vertices[vertex_index], hull_vertices[next_index],
                             space_to_pixel_mtx, game_default_palette.primary);
    }
}

// Draw the tight world-space axis-aligned bounds used by the broad phase.
static void DrawNewtonoidAABB(const Newtonoid2d *newtonoid, Matrix3x3 space_to_pixel_mtx)
{
    if (!newtonoid || !IsDebugEnabled(DEBUG_OBJECT_AABB))
    {
        return;
    }

    Vector2d min_corner = newtonoid->bounds_origin;
    Vector2d max_corner = VectorSum_2d(min_corner, newtonoid->bounds_size);
    Vector2d top_right = (Vector2d){max_corner.x, min_corner.y};
    Vector2d bottom_left = (Vector2d){min_corner.x, max_corner.y};

    DrawTransformedLineV(min_corner, top_right, space_to_pixel_mtx, game_default_palette.secondary);
    DrawTransformedLineV(top_right, max_corner, space_to_pixel_mtx, game_default_palette.secondary);
    DrawTransformedLineV(max_corner, bottom_left, space_to_pixel_mtx, game_default_palette.secondary);
    DrawTransformedLineV(bottom_left, min_corner, space_to_pixel_mtx, game_default_palette.secondary);
}

// Fill a simple polygon with ear-clipped triangles so concave outlines remain
// inside their authored boundary instead of relying on a crossing fan.
static void DrawPolygonFill(Vector2d *vertices, int vertices_count, Vector2d offset,
                            Matrix3x3 space_to_pixel_mtx, Color fill)
{
    const float epsilon = 0.000001f;
    if (!vertices || vertices_count < 3)
    {
        return;
    }

    if (vertices_count > MAX_SHAPE_VERTICES)
    {
        vertices_count = MAX_SHAPE_VERTICES;
    }

    // The signed area identifies the polygon winding so convex-ear tests work
    // for both clockwise and counter-clockwise vertex orderings.
    float signed_area = CalculatePolygonSignedArea(vertices, vertices_count);
    if (fabsf(signed_area) <= epsilon)
    {
        return;
    }

    // Keep the caller's vertex order unchanged while removing ears from this
    // local list of active polygon vertices.
    int remaining_indices[MAX_SHAPE_VERTICES];
    for (int vertex_index = 0; vertex_index < vertices_count; vertex_index++)
    {
        remaining_indices[vertex_index] = vertex_index;
    }

    float winding_sign = signed_area > 0.0f ? 1.0f : -1.0f;
    int remaining_count = vertices_count;
    int iterations_remaining = vertices_count * vertices_count;
    while (remaining_count > 3 && iterations_remaining-- > 0)
    {
        bool ear_found = false;
        for (int current_position = 0; current_position < remaining_count; current_position++)
        {
            int previous_position = (current_position + remaining_count - 1) % remaining_count;
            int next_position = (current_position + 1) % remaining_count;
            int previous_index = remaining_indices[previous_position];
            int current_index = remaining_indices[current_position];
            int next_index = remaining_indices[next_position];

            Vector2d point_previous = vertices[previous_index];
            Vector2d point_current = vertices[current_index];
            Vector2d point_next = vertices[next_index];

            // An ear must turn with the polygon winding; reflex corners cannot be removed without extending the fill outside the outline.
            float corner_cross = CalculatePolygonCross(point_previous, point_current, point_next);
            if (corner_cross * winding_sign <= epsilon)
            {
                continue;
            }

            // Reject candidate ears containing another active polygon vertex - prevents overlapping triangles inside concave regions.
            bool contains_vertex = false;
            for (int test_position = 0; test_position < remaining_count; test_position++)
            {
                int test_index = remaining_indices[test_position];
                if (test_index == previous_index || test_index == current_index || test_index == next_index)
                {
                    continue;
                }

                if (PointIsInsideTriangle(vertices[test_index], point_previous,
                                          point_current, point_next, winding_sign))
                {
                    contains_vertex = true;
                    break;
                }
            }

            if (contains_vertex)
            {
                continue;
            }

            // Transform only the accepted triangle into pixel space before submitting it for rendering.
            Vector2d world_previous = VectorSum_2d(point_previous, offset);
            Vector2d world_current = VectorSum_2d(point_current, offset);
            Vector2d world_next = VectorSum_2d(point_next, offset);
            Vector2d pixel_previous = TransformCoordinates(space_to_pixel_mtx, world_previous);
            Vector2d pixel_current = TransformCoordinates(space_to_pixel_mtx, world_current);
            Vector2d pixel_next = TransformCoordinates(space_to_pixel_mtx, world_next);
            DrawTriangle((Vector2){pixel_previous.x, pixel_previous.y},
                         (Vector2){pixel_next.x, pixel_next.y},
                         (Vector2){pixel_current.x, pixel_current.y}, fill);

            for (int shift_position = current_position; shift_position < remaining_count - 1; shift_position++)
            {
                remaining_indices[shift_position] = remaining_indices[shift_position + 1];
            }
            remaining_count--;
            ear_found = true;
            break;
        }

        if (!ear_found)
        {
            // A malformed or self-intersecting outline has no valid ear; stop
            // rather than drawing triangles that could escape its boundary.
            return;
        }
    }

    if (remaining_count == 3)
    {
        // The final three active vertices form the last triangle in the fill.
        Vector2d point_a = VectorSum_2d(vertices[remaining_indices[0]], offset);
        Vector2d point_b = VectorSum_2d(vertices[remaining_indices[1]], offset);
        Vector2d point_c = VectorSum_2d(vertices[remaining_indices[2]], offset);
        Vector2d pixel_a = TransformCoordinates(space_to_pixel_mtx, point_a);
        Vector2d pixel_b = TransformCoordinates(space_to_pixel_mtx, point_b);
        Vector2d pixel_c = TransformCoordinates(space_to_pixel_mtx, point_c);
        DrawTriangle((Vector2){pixel_a.x, pixel_a.y},
                 (Vector2){pixel_c.x, pixel_c.y},
                 (Vector2){pixel_b.x, pixel_b.y}, fill);
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

    DrawPolygonFill(local_vertices, vertices_count, offset,
                    space_to_pixel_mtx, fill);

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
    float cell_px_w = VectorMagnitude_2d(VectorDiff_2d(p10, p00));
    float cell_px_h = VectorMagnitude_2d(VectorDiff_2d(p01, p00));

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
        Vector2d world_vertices[MAX_SHAPE_VERTICES];
        int vertices_count = (int)surf_vectors.count;
        if (vertices_count > MAX_SHAPE_VERTICES)
        {
            vertices_count = MAX_SHAPE_VERTICES;
        }

        Newtonoid_TransformVertices(newtonoid, world_vertices, vertices_count);
        DrawNewtonoidHull(world_vertices, vertices_count, space_to_pixel_mtx);
        DrawNewtonoidAABB(newtonoid, space_to_pixel_mtx);
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
