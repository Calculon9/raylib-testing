/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include "common/common.h"
#include "colour/colour.h"
#include "editor/geometry_editor.h"
#include "camera/camera.h"
#include "system/draw_primitives.h"
#include "system/debug_overlay_system.h"
#include "system/systems.h"
#include "world/world.h"
#include "world/tile_catalogue.h" // TilePalette_TextureForType (texture accessor)
#include "world/world_internal.h"
#include "world/object_gizmos.h"
#include "world/universe.h"
#include "world/universe_renderer.h"
#include "world/target_resolver.h"
#include "entities/entity_registry.h" // EntityRegistry_GetSprite (pulls sprite.h / texture_registry.h)
#include "system/ui/ui_state.h"
#include "input/drag_interaction.h"
#include "system/frame_profiler.h" // PROFILER

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

// Region of a draw space, in that space's local units, that falls inside the game viewport.
typedef struct LocalViewBounds
{
    Vector2d min;
    Vector2d max;
    bool valid; // False when the region is unknown (non-invertible matrix); callers must not cull.
} LocalViewBounds;

// Resolve the part of a draw space the game viewport can see. The space-to-pixel matrix is
// inverted per space, so nested worlds (which have their own local basis and placement) get a
// box in their own units rather than the universe-space camera view box. A degenerate or
// non-finite result is reported as invalid so the caller draws everything instead of culling it all.
static LocalViewBounds ResolveLocalViewBounds(Matrix3x3 space_to_pixel_mtx)
{
    LocalViewBounds bounds = {0};
    CameraViewBox view = GetViewportBoundsInSpace(&game_viewport, space_to_pixel_mtx);

    // Reject non-finite or zero-area boxes (a singular matrix inverts to the zero matrix).
    if (!isfinite(view.origin.x) || !isfinite(view.origin.y) ||
        !isfinite(view.dimensions.x) || !isfinite(view.dimensions.y) ||
        view.dimensions.x <= 0.0f || view.dimensions.y <= 0.0f)
    {
        return bounds;
    }

    bounds.min = view.origin;
    bounds.max = (Vector2d){view.origin.x + view.dimensions.x, view.origin.y + view.dimensions.y};
    bounds.valid = true;
    return bounds;
}

// Conservative visibility test for one entity against a draw space's visible region. The test
// box is centred on the anchor (authoritative position) and padded generously so nothing drawn
// for the entity can be missed: the polygon, the hull/AABB/axes gizmos (which reach about
// 0.75 x bounds from the anchor) and any sprite (offset plus its full size). It may keep a few
// entities that are just off-screen, but it never drops one that is partly on-screen.
//
//   newtonoid : the entity to test.
//   sprite    : the entity's sprite component, or NULL when it has none.
//   view      : the visible region of the space the entity is drawn in.
static bool NewtonoidIsPotentiallyVisible(const Newtonoid2d *newtonoid, const SpriteComponent *sprite,
                                          const LocalViewBounds *view)
{
    if (!view->valid)
    {
        return true; // Unknown view region: draw rather than risk hiding the entity.
    }

    // Anchor-to-furthest-vertex distance is bounded by the sum of the AABB extents; the bounding
    // radius is added as a second safeguard against a stale AABB.
    float half_extent = newtonoid->bounds_size.x + newtonoid->bounds_size.y + newtonoid->radius;
    if (sprite != NULL && sprite->texture_id != INVALID_TEXTURE_ID)
    {
        half_extent += (float)VectorMagnitude_2d(sprite->offset) + (float)VectorMagnitude_2d(sprite->size);
    }

    Vector2d centre = newtonoid->anchor_position;
    return (centre.x + half_extent >= view->min.x) && (centre.x - half_extent <= view->max.x) &&
           (centre.y + half_extent >= view->min.y) && (centre.y - half_extent <= view->max.y);
}

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
    if (!world_vertices || vertices_count < 3 || !IsGizmoEnabled(GIZMO_HULL))
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
    if (!newtonoid || !IsGizmoEnabled(GIZMO_AABB))
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

// A distinct outline colour for selected members, kept separate from the gizmo palette so a
// selection highlight never reads as an AABB gizmo. Bright cyan stands out against the world.
static const ColourRgba selection_highlight_colour = {64, 224, 255, 255};

// Marquee box palette: a faint translucent fill and a solid thin outline, kept distinct from the
// selection highlight colour so the live drag box reads as a transient overlay, not a selection.
static const ColourRgba marquee_fill_colour = {64, 224, 255, 40};
static const ColourRgba marquee_line_colour = {64, 224, 255, 200};

// Draw an axis-aligned box outline (min corner + size) in a given space using the selection
// highlight colour. Shares the four-corner / four-line math with DrawNewtonoidAABB but is not
// gizmo-gated and takes an explicit colour.
static void DrawSelectionHighlightAABB(Vector2d min_corner, Vector2d size, Matrix3x3 space_to_pixel,
                                       ColourRgba colour)
{
    Vector2d max_corner = VectorSum_2d(min_corner, size);
    Vector2d top_right = (Vector2d){max_corner.x, min_corner.y};
    Vector2d bottom_left = (Vector2d){min_corner.x, max_corner.y};

    DrawTransformedLineV(min_corner, top_right, space_to_pixel, colour);
    DrawTransformedLineV(top_right, max_corner, space_to_pixel, colour);
    DrawTransformedLineV(max_corner, bottom_left, space_to_pixel, colour);
    DrawTransformedLineV(bottom_left, min_corner, space_to_pixel, colour);
}

// Outline every member of the central selection set. ENTITY members are outlined from their
// AABB in their owning world's space; WORLD members from their cached universe-space bounds.
// CELL members are skipped in Stage 1 (documented limitation) as the draw path does not already
// expose cell geometry; cells still participate in the set and the info-strip count. Unresolved
// members (deleted entity, invalid world, or invalid bounds) are skipped silently.
void WorldRenderer_DrawSelectionHighlights(Universe *universe)
{
    if (!universe)
    {
        return;
    }

    TargetRef *ref = NULL;
    LArray_ForEach(&G_UIState.selection_set, TargetRef*, ref)
    {
        switch (ref->kind)
        {
            case TARGET_KIND_ENTITY:
            {
                int world_index = 0;
                Newtonoid2d *entity = Universe_GetEntityByID(universe, ref->as.entity, &world_index);
                if (!entity)
                {
                    break;
                }
                World2d *owning_world = Universe_GetWorld(universe, world_index);
                if (!owning_world)
                {
                    break;
                }
                Matrix3x3 space_to_pixel = ResolveWorldToPixelMatrix(owning_world, &universe->camera);
                DrawSelectionHighlightAABB(entity->bounds_origin, entity->bounds_size,
                                           space_to_pixel, selection_highlight_colour);
                break;
            }
            case TARGET_KIND_WORLD:
            {
                World2d *world = Universe_GetWorld(universe, ref->as.world_index);
                if (!world || !world->bounds_valid)
                {
                    break;
                }
                // bounds.col1 is the universe-space min corner, col2 the max corner.
                Vector2d min_corner = world->bounds.col1;
                Vector2d size = VectorDiff_2d(world->bounds.col2, world->bounds.col1);
                Matrix3x3 universe_to_pixel = ResolveWorldToPixelMatrix(&universe->root_world,
                                                                        &universe->camera);
                DrawSelectionHighlightAABB(min_corner, size, universe_to_pixel,
                                           selection_highlight_colour);
                break;
            }
            case TARGET_KIND_CELL:
            case TARGET_KIND_NONE:
            default:
                // Cells skipped in Stage 1; NONE never stored.
                break;
        }
    }
}

// Draw the live marquee selection box while an empty-space box drag is in progress. The rectangle
// is taken straight from the game drag context's screen-pixel start/current points (no coordinate
// conversion), normalised to a top-left origin with positive size so it draws correctly when the
// user drags in any direction. A no-op unless a DRAG_TARGET_MARQUEE capture is currently active.
void WorldRenderer_DrawMarquee(void)
{
    DragInteractionState *ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    if (!ctx || !ctx->has_capture || ctx->target_kind != DRAG_TARGET_MARQUEE)
    {
        return;
    }

    Vector2d a = ctx->pointer_state.initial_pos;
    Vector2d b = ctx->pointer_state.current_pos;

    // Normalise the two corners into a top-left origin plus a positive width/height.
    float x = fminf(a.x, b.x);
    float y = fminf(a.y, b.y);
    float w = fabsf(a.x - b.x);
    float h = fabsf(a.y - b.y);

    DrawRectangleV((Vector2){x, y}, (Vector2){w, h}, ToRaylibColor(marquee_fill_colour));
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1.0f, ToRaylibColor(marquee_line_colour));
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
    if (!newtonoid || !IsGizmoEnabled(GIZMO_AXES))
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
    PROFILE_BEGIN(PROF_WORLDREGION); // PROFILER
    Matrix3x3 world_to_pixel_mtx = ResolveWorldToPixelMatrix(world, universe_camera);

    DrawGridSpace(&world->grid_space, world_to_pixel_mtx);
    DrawNewtonoids(&world->objects, world_to_pixel_mtx);
    GeometryEditor_DrawHandles(world, universe_camera);
    PROFILE_END(PROF_WORLDREGION); // PROFILER
    // DrawCollisions(&world->collisions, world_to_pixel_mtx);
}

// Grid on-screen size thresholds, in pixels per cell (the smaller of the two cell axes).
// Below LINE_MIN the whole grid-line pass is skipped (lines would be noise); raise to hide sooner.
// Below LABEL_MIN the per-cell debug labels are skipped (they would not fit inside the cell).
#define WORLD_GRID_LINE_MIN_CELL_PX 4.0f
#define WORLD_GRID_LABEL_MIN_CELL_PX 40.0f
// Bold world-border line width in pixels (grid lines are 1 px). The border is NOT gated by
// LINE_MIN: it must stay visible when zoomed far out, which is when it is most useful.
#define WORLD_GRID_BORDER_WIDTH_PX 3.0f

// GRID-RUN-HELPERS-BEGIN (pure helpers; the scratch unit test extracts everything up to the END marker)

// Presence query: is the cell at (col, row) really there? Must read out-of-range as absent.
typedef bool (*GridPresenceQuery)(const void *ctx, int col, int row);

// How a cell is drawn by the fill pass.
typedef enum GridFillKind
{
    GRID_FILL_ABSENT = 0, // No cell: nothing to fill.
    GRID_FILL_TEXTURED,   // Draws its own texture quad; never merged into a colour run.
    GRID_FILL_COLOUR      // Plain colour fill; mergeable with neighbours of the same tile_type.
} GridFillKind;

// Fill query: classify the cell at (col, row); for GRID_FILL_COLOUR also reports its tile_type.
typedef GridFillKind (*GridFillQuery)(const void *ctx, int col, int row, int *tile_type);

// Length of the run of colour-filled cells that starts at (col, row), extends along the row and
// stops at col_end (exclusive), the first absent/textured cell, or the first tile_type change.
// The cell at (col, row) must itself be GRID_FILL_COLOUR with tile_type `tile_type`.
static int GridFillRunLength(GridFillQuery query, const void *ctx, int row, int col, int col_end,
                             int tile_type)
{
    int length = 1; // The starting cell is already known to qualify.
    while (col + length < col_end)
    {
        int next_tile = 0;
        if (query(ctx, col + length, row, &next_tile) != GRID_FILL_COLOUR || next_tile != tile_type)
        {
            break;
        }
        length++;
    }
    return length;
}

// Does the grid edge at position `pos` along grid line `line` exist? An edge exists when either
// cell it separates is present. Horizontal: the edge at row boundary y=line spanning column `pos`
// separates cells (pos, line-1) and (pos, line). Vertical: the edge at column boundary x=line
// spanning row `pos` separates cells (line-1, pos) and (line, pos).
static bool GridEdgeExists(GridPresenceQuery present, const void *ctx, bool horizontal, int line,
                           int pos)
{
    if (horizontal)
    {
        return present(ctx, pos, line - 1) || present(ctx, pos, line);
    }
    return present(ctx, line - 1, pos) || present(ctx, line, pos);
}

// Find the next run of consecutive existing edges along one grid line, scanning positions
// [from, to). On success returns true with the run as [*run_start, *run_end) (end exclusive); a
// missing edge (hole) ends the run, so the next call from *run_end starts a new segment.
static bool GridNextEdgeRun(GridPresenceQuery present, const void *ctx, bool horizontal, int line,
                            int from, int to, int *run_start, int *run_end)
{
    int pos = from;
    while (pos < to && !GridEdgeExists(present, ctx, horizontal, line, pos))
    {
        pos++; // Skip missing edges until one exists.
    }
    if (pos >= to)
    {
        return false;
    }
    *run_start = pos;
    while (pos < to && GridEdgeExists(present, ctx, horizontal, line, pos))
    {
        pos++; // Extend while edges keep existing.
    }
    *run_end = pos;
    return true;
}

// Does the grid edge at position `pos` along grid line `line` lie on the WORLD BORDER? It does when
// exactly ONE of the two cells it separates is present, i.e. the outline of the present cells:
// the outside of the grid, the sides of carved holes and any ragged edge. Uses the same edge
// geometry as GridEdgeExists (horizontal: cells (pos, line-1) / (pos, line); vertical: cells
// (line-1, pos) / (line, pos)); out-of-range cells read as absent through the presence query.
static bool GridBorderEdgeExists(GridPresenceQuery present, const void *ctx, bool horizontal,
                                 int line, int pos)
{
    bool before = horizontal ? present(ctx, pos, line - 1) : present(ctx, line - 1, pos);
    bool after = horizontal ? present(ctx, pos, line) : present(ctx, line, pos);
    return before != after;
}

// As GridNextEdgeRun, but over BORDER edges only: finds the next run of consecutive border edges
// along one grid line, scanning positions [from, to). Returns true with the run as
// [*run_start, *run_end) (end exclusive); a non-border edge ends the run.
static bool GridNextBorderRun(GridPresenceQuery present, const void *ctx, bool horizontal, int line,
                              int from, int to, int *run_start, int *run_end)
{
    int pos = from;
    while (pos < to && !GridBorderEdgeExists(present, ctx, horizontal, line, pos))
    {
        pos++; // Skip non-border edges until one exists.
    }
    if (pos >= to)
    {
        return false;
    }
    *run_start = pos;
    while (pos < to && GridBorderEdgeExists(present, ctx, horizontal, line, pos))
    {
        pos++; // Extend while border edges keep existing.
    }
    *run_end = pos;
    return true;
}

// GRID-RUN-HELPERS-END

// Context for the production presence/fill queries below: one grid space and its dimensions.
typedef struct GridQueryContext
{
    const GridSpace2d *grid_space;
    int columns;
    int rows;
} GridQueryContext;

// Production presence query: bounds-checked Cell_IsPresent on the (col, row) cell.
static bool GridCellPresentQuery(const void *ctx, int col, int row)
{
    const GridQueryContext *grid = (const GridQueryContext *)ctx;
    if (col < 0 || row < 0 || col >= grid->columns || row >= grid->rows)
    {
        return false; // Out of range reads as absent.
    }
    return Cell_IsPresent(&grid->grid_space->space, (row * grid->columns) + col);
}

// Production fill query: absent / textured / colour (with its tile_type) for the (col, row) cell.
static GridFillKind GridCellFillQuery(const void *ctx, int col, int row, int *tile_type)
{
    const GridQueryContext *grid = (const GridQueryContext *)ctx;
    int k = (row * grid->columns) + col; // Linear cell index within the full grid.
    if (!GridCellPresentQuery(ctx, col, row))
    {
        return GRID_FILL_ABSENT;
    }
    const Cell *cell = (const Cell *)((const char *)grid->grid_space->space.cells.items +
                                      ((size_t)k * grid->grid_space->space.cells.elem_bytes));
    *tile_type = cell->tile_type;
    return (TilePalette_TextureForType(cell->tile_type) != INVALID_TEXTURE_ID) ? GRID_FILL_TEXTURED
                                                                              : GRID_FILL_COLOUR;
}

// Draw the bold world border: every border edge (see GridBorderEdgeExists) over the visible block,
// merged into runs and drawn through the viewport-clipped helper so it never bleeds into panels.
// Each run is lengthened by `cap_local` at both ends so two thick segments meeting at a convex
// corner overlap and leave no notch (butt caps would otherwise leave a missing corner square).
//
//   grid_query    : presence context for the grid being drawn.
//   mtx           : world-local to pixel transform.
//   colour        : border colour (the grid's own line colour, drawn thicker).
//   col/row_min/max : inclusive visible cell block.
//   gox, goy      : grid origin added to every local coordinate.
//   cap_local     : run end extension in local units (half the line width in cells).
static void DrawGridBorder(const GridQueryContext *grid_query, Matrix3x3 mtx, ColourRgba colour,
                           int col_min, int col_max, int row_min, int row_max,
                           float gox, float goy, float cap_local)
{
    int run_start = 0;
    int run_end = 0;

    // Horizontal border edges: one grid line per row boundary, positions running along columns.
    for (int line = row_min; line <= row_max + 1; line++)
    {
        int from = col_min;
        while (GridNextBorderRun(GridCellPresentQuery, grid_query, true, line, from,
                                 col_max + 1, &run_start, &run_end))
        {
            DrawTransformedLineClippedEx((Vector2d){(float)run_start + gox - cap_local, (float)line + goy},
                                         (Vector2d){(float)run_end + gox + cap_local, (float)line + goy},
                                         mtx, WORLD_GRID_BORDER_WIDTH_PX, colour,
                                         game_viewport.pixel_origin, game_viewport.pixel_end);
            from = run_end;
        }
    }

    // Vertical border edges: one grid line per column boundary, positions running along rows.
    for (int line = col_min; line <= col_max + 1; line++)
    {
        int from = row_min;
        while (GridNextBorderRun(GridCellPresentQuery, grid_query, false, line, from,
                                 row_max + 1, &run_start, &run_end))
        {
            DrawTransformedLineClippedEx((Vector2d){(float)line + gox, (float)run_start + goy - cap_local},
                                         (Vector2d){(float)line + gox, (float)run_end + goy + cap_local},
                                         mtx, WORLD_GRID_BORDER_WIDTH_PX, colour,
                                         game_viewport.pixel_origin, game_viewport.pixel_end);
            from = run_end;
        }
    }
}

// Draw a grid space. Each PRESENT cell resolves its tile_type through the data-driven tile palette
// (tile_catalogue.c): if the tile_type carries a texture, the cell draws one textured quad into its
// world cell via DrawTextureInWorld; otherwise it falls back to the FILL colour from
// TilePalette_ColourForType(cell->tile_type, grid_space->colour_fill) — a tile_type of 0 resolves
// to the grid's own colour_fill (pixel-identical to before this change), while non-zero types read
// the palette. The grid LINES (grid_space->colour_line via the viewport-clipped
// DrawTransformedLineClippedV path) and the debug labels are unchanged, and absent cells are still
// skipped — only the per-cell fill source changed.
void DrawGridSpace(GridSpace2d *grid_space, Matrix3x3 world_to_pixel_mtx)
{
    if (grid_space->space.cells.capacity < 1)
    {
        return;
    }

    // Grid geometry is authored in world-local coordinates; the world basis and
    // placement are applied once by world_to_pixel_mtx.

    int columns = grid_space->space.columns;
    int rows = grid_space->space.rows;

    // Cell (column j, row i) occupies local space [j + go.x, j + go.x + 1] x [i + go.y, i + go.y + 1].
    // grid_origin is a whole-number local coordinate that shifts when the world grows on the left/top,
    // so EVERY local coordinate fed to the transform below adds it. (Nested worlds used to have a zero
    // origin, which is why bare (j, i) used to work.)
    const float gox = grid_space->space.grid_origin.x;
    const float goy = grid_space->space.grid_origin.y;

    // View culling: restrict every pass below to the block of cells the game viewport can reach.
    // Cell (j, i) spans [j, j+1] x [i, i+1] in this world's local units, so the visible block is
    // the floor of the local view box clamped to the grid. The floats are clamped BEFORE the int
    // conversion so a hugely zoomed-out view cannot overflow. An unknown view draws the full grid.
    int col_min = 0;
    int col_max = columns - 1;
    int row_min = 0;
    int row_max = rows - 1;
    LocalViewBounds view = ResolveLocalViewBounds(world_to_pixel_mtx);
    if (view.valid)
    {
        // The view box is in local space; subtract grid_origin to get index space before clamping.
        col_min = (int)floorf(fminf(fmaxf(view.min.x - gox, 0.0f), (float)columns));
        col_max = (int)floorf(fminf(fmaxf(view.max.x - gox, -1.0f), (float)columns));
        row_min = (int)floorf(fminf(fmaxf(view.min.y - goy, 0.0f), (float)rows));
        row_max = (int)floorf(fminf(fmaxf(view.max.y - goy, -1.0f), (float)rows));
        if (col_max > columns - 1) col_max = columns - 1;
        if (row_max > rows - 1) row_max = rows - 1;
    }
    if (col_min > col_max || row_min > row_max)
    {
        return; // The whole grid lies outside the viewport: nothing to fill, outline or label.
    }

    // Flattened iteration over the visible block (row-major), so each pass keeps a single loop.
    int visible_columns = col_max - col_min + 1;
    int visible_count = visible_columns * (row_max - row_min + 1);

    // Pixels per cell: the shorter of the two basis-column lengths of the world-to-pixel matrix
    // (so a squashed axis hides detail sooner). Computed once and shared by the line and label gates.
    float cell_px = fminf(sqrtf((world_to_pixel_mtx.col1.x * world_to_pixel_mtx.col1.x) +
                                (world_to_pixel_mtx.col1.y * world_to_pixel_mtx.col1.y)),
                          sqrtf((world_to_pixel_mtx.col2.x * world_to_pixel_mtx.col2.x) +
                                (world_to_pixel_mtx.col2.y * world_to_pixel_mtx.col2.y)));
    PROFILE_SET_CELL_PX(cell_px); // PROFILER

    // Presence/fill queries read cells straight from the full grid (out-of-range reads as absent).
    GridQueryContext grid_query = {grid_space, columns, rows};
    // Base/fallback fill colour for the grid. It is now passed into TilePalette_ColourForType as
    // the default (used for tile_type 0 and any out-of-range index) rather than drawn directly; the
    // raylib Color is computed per present cell from the resolved per-cell colour.
    ColourRgba colour_fill = grid_space->colour_fill;

    PROFILE_BEGIN(PROF_GRIDFILL); // PROFILER
    // FILL: walk each visible row and merge runs of consecutive PRESENT, untextured cells of the
    // same tile_type into ONE parallelogram (two triangles). Any affine world/camera transform maps
    // an axis-aligned local rectangle to a parallelogram, so merging is valid under rotation and
    // shear. Textured cells still draw one quad each via DrawTextureInWorld and break runs; absent
    // cells are skipped, so carved shapes stay non-rectangular.
    for (int i = row_min; i <= row_max; i++)
    {
        int j = col_min;
        while (j <= col_max)
        {
            int tile_type = 0;
            GridFillKind kind = GridCellFillQuery(&grid_query, j, i, &tile_type);
            if (kind == GRID_FILL_ABSENT)
            {
                j++;
                continue;
            }

            // Textured path: one textured quad into the cell's unit quad [j, j+1] x [i, i+1] (centre
            // (j + 0.5, i + 0.5), size 1x1), same world_to_pixel_mtx as the colour quads, rotation 0
            // and opaque white tint. No UV tiling or warp_basis distortion (explicitly deferred).
            if (kind == GRID_FILL_TEXTURED)
            {
                DrawTextureInWorld(TilePalette_TextureForType(tile_type),
                                   (Vector2d){(float)j + gox + 0.5f, (float)i + goy + 0.5f},
                                   (Vector2d){1.0f, 1.0f}, world_to_pixel_mtx,
                                   (ColourRgba){255, 255, 255, 255}, 0.0f);
                j++;
                continue;
            }

            // Colour path: extend the run, then resolve the colour once for the whole run.
            // tile_type 0 resolves to the grid's own colour_fill, so existing worlds are unchanged.
            int run_length = GridFillRunLength(GridCellFillQuery, &grid_query, i, j, col_max + 1,
                                               tile_type);
            Color fill = ToRaylibColor(TilePalette_ColourForType(tile_type, colour_fill));

            // The run's four local corners, transformed to pixels through the world basis/placement.
            Vector2d cp0 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){(float)j + gox, (float)i + goy});
            Vector2d cp1 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){(float)(j + run_length) + gox, (float)i + goy});
            Vector2d cp2 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){(float)(j + run_length) + gox, (float)(i + 1) + goy});
            Vector2d cp3 = TransformCoordinates(world_to_pixel_mtx, (Vector2d){(float)j + gox, (float)(i + 1) + goy});

            // Vertex order/winding is unchanged from the per-cell version (raylib culls reversed quads).
            DrawTriangle((Vector2){cp2.x, cp2.y}, (Vector2){cp1.x, cp1.y}, (Vector2){cp0.x, cp0.y}, fill);
            DrawTriangle((Vector2){cp3.x, cp3.y}, (Vector2){cp2.x, cp2.y}, (Vector2){cp0.x, cp0.y}, fill);
            j += run_length;
        }
    }
    PROFILE_END(PROF_GRIDFILL); // PROFILER

    // The world grid toggle cycles FULL (lines + border) -> BORDER (border only) -> OFF. OFF draws
    // neither; the cell fills above are unaffected.
    if (world_grid_mode == WORLD_GRID_MODE_OFF)
    {
        return;
    }

    PROFILE_BEGIN(PROF_GRIDLINES); // PROFILER

    // LINES (FULL mode only): skipped entirely when cells are too small on screen for a 1 px line
    // to read. Otherwise every grid edge is drawn exactly ONCE (shared interior edges are no longer
    // doubled). An edge exists when either cell it separates is present, so a carved shape keeps
    // its outline and a hole splits runs. Consecutive existing edges merge into one segment,
    // drawn through the viewport-clipped helper so lines never bleed into panels.
    if (world_grid_mode == WORLD_GRID_MODE_FULL && cell_px >= WORLD_GRID_LINE_MIN_CELL_PX)
    {
        ColourRgba colour = grid_space->colour_line;
        int run_start = 0;
        int run_end = 0;

        // Horizontal lines: one per row boundary y = line, with positions running along columns.
        for (int line = row_min; line <= row_max + 1; line++)
        {
            int from = col_min;
            while (GridNextEdgeRun(GridCellPresentQuery, &grid_query, true, line, from,
                                   col_max + 1, &run_start, &run_end))
            {
                DrawTransformedLineClippedV((Vector2d){(float)run_start + gox, (float)line + goy},
                                            (Vector2d){(float)run_end + gox, (float)line + goy},
                                            world_to_pixel_mtx, colour,
                                            game_viewport.pixel_origin, game_viewport.pixel_end);
                from = run_end;
            }
        }

        // Vertical lines: one per column boundary x = line, with positions running along rows.
        for (int line = col_min; line <= col_max + 1; line++)
        {
            int from = row_min;
            while (GridNextEdgeRun(GridCellPresentQuery, &grid_query, false, line, from,
                                   row_max + 1, &run_start, &run_end))
            {
                DrawTransformedLineClippedV((Vector2d){(float)line + gox, (float)run_start + goy},
                                            (Vector2d){(float)line + gox, (float)run_end + goy},
                                            world_to_pixel_mtx, colour,
                                            game_viewport.pixel_origin, game_viewport.pixel_end);
                from = run_end;
            }
        }
    }

    // BORDER (FULL and BORDER modes): the bold outline of the present cells, drawn after the thin
    // lines so it sits on top of them. Not gated by cell size, so the world outline stays visible
    // when zoomed out. Run ends overshoot by half the line width (capped at half a cell) so
    // corners join cleanly; cell_px is the shorter cell axis, so this is slightly generous on the
    // longer one, which only ever hides inside the neighbouring thick segment.
    float cap_local = (cell_px > 0.0f) ? fminf(0.5f * WORLD_GRID_BORDER_WIDTH_PX / cell_px, 0.5f) : 0.0f;
    DrawGridBorder(&grid_query, world_to_pixel_mtx, grid_space->colour_line,
                   col_min, col_max, row_min, row_max, gox, goy, cap_local);
    PROFILE_END(PROF_GRIDLINES); // PROFILER

    // Debug labels only make sense alongside the full grid.
    if (world_grid_mode != WORLD_GRID_MODE_FULL)
    {
        return;
    }

    Color text_colour = ToRaylibColor(COLOUR_GAME_PARCHMENT_RGBA);

    if (!world_grid_debug_labels_enabled)
    {
        return;
    }

    PROFILE_BEGIN(PROF_GRIDLABELS); // PROFILER
    if (cell_px < WORLD_GRID_LABEL_MIN_CELL_PX)
    {
        PROFILE_END(PROF_GRIDLABELS); // PROFILER
        return;
    }

    for (int n = 0; n < visible_count; n++)
    {
        int i = row_min + (n / visible_columns);
        int j = col_min + (n % visible_columns);
        int k = (i * columns) + j; // Linear cell index within the full grid.
        if (!Cell_IsPresent(&grid_space->space, k))
        {
            continue; // Absent (carved) cells carry no label.
        }
        // Label anchor = the cell's top-left corner (j + go.x, i + go.y) in local space. A derived per-cell origin (Space_CellLocalOrigin)
        // is NOT used: it already contains the basis (grid_origin + j*u + i*v) which world_to_pixel_mtx
        // would apply a second time under a non-orthonormal basis. The second line shows the absolute
        // lattice coordinate so growth is easy to verify.
        Vector2d cell_coords = {(float)j + gox, (float)i + goy};
        Vector2d cell_pixel_coords = TransformCoordinates(world_to_pixel_mtx, cell_coords);
        const char *displayText = TextFormat("%d(%d,%d)\nL(%d,%d)\n(%.0f,%.0f)\n", k, i, j,
                                             j + (int)gox, i + (int)goy, cell_pixel_coords.x, cell_pixel_coords.y);

        DrawTextEx(font, displayText, (Vector2){cell_pixel_coords.x + 2, cell_pixel_coords.y + 2}, 16, 1, text_colour);
    }
    PROFILE_END(PROF_GRIDLABELS); // PROFILER
}

void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx)
{
    if (!LArray_IsValid(newtonoids))
    {
        return;
    }

    // Resolve the visible region once per call (one matrix inversion), then skip every entity
    // that lies wholly outside it instead of transforming and submitting its geometry.
    LocalViewBounds view = ResolveLocalViewBounds(space_to_pixel_mtx);

    Newtonoid2d *newtonoid;
    LArray_ForEach(newtonoids, Newtonoid2d*, newtonoid)
    {
        Vector2d obj_center_coords = newtonoid->anchor_position;
        LArray surf_vectors = newtonoid->surface.surface_vectors;

        // Sprite-bearing entities draw a texture INSTEAD OF the polygon fill/outline; every other
        // entity keeps the exact polygon call (byte-identical output). Sprites currently draw in
        // entity-iteration order — no z-index sort pass yet (a deliberate Layer-1 deferral).
        SpriteComponent *sprite = EntityRegistry_GetSprite(newtonoid->id);

        // View culling: nothing of this entity (polygon, sprite or gizmos) can reach the viewport.
        if (!NewtonoidIsPotentiallyVisible(newtonoid, sprite, &view))
        {
            continue;
        }

        if (sprite != NULL && sprite->texture_id != INVALID_TEXTURE_ID)
        {
            // Sprite centre = anchor + offset rotated into the entity's basis (local_axis_x/y) —
            // the SAME basis the polygon uses, so the sprite rides the entity's rotation identically.
            Vector2d world_centre = VectorSum_2d(
                newtonoid->anchor_position,
                VectorSum_2d(VectorScale_2d(newtonoid->local_axis_x, sprite->offset.x),
                             VectorScale_2d(newtonoid->local_axis_y, sprite->offset.y)));
            // Pass the entity's own rotation (radians) so the texture spins WITH the entity, in the
            // same direction its vertices rotate (both compose entity rotation then the camera matrix).
            DrawTextureInWorld(sprite->texture_id, world_centre, sprite->size,
                               space_to_pixel_mtx, sprite->tint, newtonoid->rotation);
        }
        else
        {
            DrawRotatedObjectVertices(surf_vectors.items, surf_vectors.count, obj_center_coords,
                                      newtonoid->local_axis_x, newtonoid->local_axis_y,
                                      space_to_pixel_mtx, newtonoid->line_colour,
                                      newtonoid->fill_colour);
        }
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
