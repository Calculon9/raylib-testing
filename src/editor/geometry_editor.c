#include "editor/geometry_editor.h"

#include "input/drag_interaction.h"
#include "raylib.h"
#include "system/systems.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "world/universe.h"
#include "world/world_internal.h"

typedef struct
{
    Newtonoid2d *object;
    int vertex_index;
} VertexHandleTarget;

static VertexHandleTarget vertex_handle_target = {0};

static bool GeometryEditor_IsEnabled(const World2d *world, const Newtonoid2d *selected_object)
{
    // selected_object is already existence-validated by UIState_GetSelectedObject, so a
    // NULL check here is equivalent to (and far cheaper than) an universe-wide entity scan.
    return world && world->mode == PAUSED &&
           selected_object != NULL &&
           G_UIState.active_panel_view == LPANEL_DRAW_VIEW;
}

static void RefreshObjectGeometry(World2d *world, Newtonoid2d *object,
                                  Matrix2x2 previous_box)
{
    RebuildNewtonoidGeometry(object);

    RemapEntityInASpace(&world->grid_space.space, object, previous_box,
                        &world->entity_space_map);
}

static bool GeometryEditor_TryBeginDrag(World2d *world, Vector2d pixel_coords, Newtonoid2d *object)
{
    if (!GeometryEditor_IsEnabled(world, object) || !object ||
        object->surface.surface_vectors.count == 0)
    {
        LOG_INFO("Geometry editor hit-test skipped: world=%p object=%p mode=%d view=%d\n",
                 (void *)world, (void *)object, world ? world->mode : -1,
                 G_UIState.active_panel_view);
        return false;
    }

    Matrix3x3 world_to_pixel = ResolveWorldToPixelMatrix(world, &G_Universe.camera);
    Vector2d *vertices = object->surface.surface_vectors.items;
    const float handle_radius = 10.0f;

    for (size_t index = 0; index < object->surface.surface_vectors.count; index++)
    {
        Vector2d world_vertex = VectorSum_2d(vertices[index], object->anchor_position);
        Vector2d pixel_vertex = TransformCoordinates(world_to_pixel, world_vertex);
        LOG_INFO("Geometry editor vertex %zu: mouse=(%.1f,%.1f) handle=(%.1f,%.1f)\n",
                 index, pixel_coords.x, pixel_coords.y, pixel_vertex.x, pixel_vertex.y);
        if (VectorWithinDistance_2d(pixel_vertex, pixel_coords, handle_radius))
        {
            vertex_handle_target.object = object;
            vertex_handle_target.vertex_index = (int)index;
            DragInteraction_BeginCapture(
                DragInteraction_GetContext(DRAG_CONTEXT_GAME),
                DRAG_TARGET_VERTEX_HANDLE, &vertex_handle_target, ZERO_VECTOR_2D);
            LOG_INFO("Geometry editor captured vertex handle: index=%d target_kind=%d\n",
                     vertex_handle_target.vertex_index,
                     DragInteraction_GetContext(DRAG_CONTEXT_GAME)->target_kind);
            return true;
        }
    }

    return false;
}

static bool GeometryEditor_UpdateDrag(World2d *world, Vector2d pixel_coords, Newtonoid2d *selected_object)
{
    DragInteractionState *drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    if (!GeometryEditor_IsEnabled(world, selected_object))
    {
        DragInteraction_ClearCapture(drag_ctx);
        return false;
    }

    if (!drag_ctx->has_capture ||
        drag_ctx->target_kind != DRAG_TARGET_VERTEX_HANDLE ||
        drag_ctx->target != &vertex_handle_target)
    {
        return false;
    }

    Newtonoid2d *object = vertex_handle_target.object;
    if (!object || object != selected_object ||
        vertex_handle_target.vertex_index < 0 ||
        (size_t)vertex_handle_target.vertex_index >= object->surface.surface_vectors.count)
    {
        DragInteraction_ClearCapture(drag_ctx);
        return false;
    }

    Vector2d previous_vertices[4] = {0};
    CalcSnappedAABB_Vertices(object->surface.surface_vectors.items,
                             object->surface.surface_vectors.count,
                             object->anchor_position,
                             world->grid_space.space.frame.basis,
                             previous_vertices);
    Matrix2x2 previous_box = CalcAABBCoords_Tight(previous_vertices, 4, ZERO_VECTOR_2D);

    Vector2d previous_geometry_center = object->local_geometry_center;
    Vector2d local_coords = ResolvePixelToWorldFrame(world, pixel_coords);
    Vector2d *vertex = &((Vector2d *)object->surface.surface_vectors.items)[vertex_handle_target.vertex_index];
    *vertex = VectorDiff_2d(local_coords, object->anchor_position);

    // Keep the local geometry centered on the anchor while preserving world-space vertex positions.
    RebuildNewtonoidGeometry(object);
    Vector2d center_delta = VectorSum_2d(
        object->local_geometry_center,
        (Vector2d){-previous_geometry_center.x, -previous_geometry_center.y});
    object->anchor_position = VectorSum_2d(object->anchor_position, center_delta);
    Vector2d *local_vertices = (Vector2d *)object->surface.surface_vectors.items;
    for (size_t index = 0; index < object->surface.surface_vectors.count; index++)
    {
        local_vertices[index] = VectorSum_2d(
            local_vertices[index],
            (Vector2d){-center_delta.x, -center_delta.y});
    }
    RefreshObjectGeometry(world, object, previous_box);
    object->velocity = ZERO_VECTOR_2D;
    object->acceleration = ZERO_VECTOR_2D;
    object->momentum = ZERO_VECTOR_2D;
    return true;
}

InputRouteResult UpdateGeometryEditor(World2d *world, const InputFrame *input)
{
    if (!input)
    {
        return INPUT_ROUTE_IGNORED;
    }

    // Resolve the selection once per frame; every helper below reuses this instead of re-querying.
    Newtonoid2d *selected_object = UIState_GetSelectedObject();
    DragInteractionState *drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
    Vector2d pixel_coords = input->pointer_position;
    bool log_input = (frame_counter.total_frames % 300u) == 0;
    if (log_input)
    {
        LOG_INFO("Editor input begin: down=%d enabled=%d kind=%d target=%p capture=%d hold=%d mouse=(%.1f,%.1f)\n",
                 input->left_down,
                 GeometryEditor_IsEnabled(world, selected_object),
                 drag_ctx->target_kind,
                 drag_ctx->target,
                 drag_ctx->has_capture,
                 drag_ctx->pointer_state.left_button_hold_ticks,
                 pixel_coords.x,
                 pixel_coords.y);
    }

    if (!GeometryEditor_IsEnabled(world, selected_object))
    {
        if (drag_ctx->has_capture && drag_ctx->target_kind == DRAG_TARGET_VERTEX_HANDLE)
        {
            DragInteraction_ClearCapture(drag_ctx);
            return INPUT_ROUTE_HANDLED;
        }
        if (log_input)
        {
            LOG_INFO("Editor input end: consumed=0 reason=DISABLED kind=%d target=%p capture=%d hold=%d\n",
                     drag_ctx->target_kind,
                     drag_ctx->target,
                     drag_ctx->has_capture,
                     drag_ctx->pointer_state.left_button_hold_ticks);
        }
        return INPUT_ROUTE_IGNORED;
    }

    if (drag_ctx->has_capture && drag_ctx->target_kind == DRAG_TARGET_VERTEX_HANDLE)
    {
        if (!input->left_released && !GeometryEditor_UpdateDrag(world, pixel_coords, selected_object))
        {
            return INPUT_ROUTE_HANDLED;
        }
        if (log_input)
        {
            LOG_INFO("Editor input end: consumed=1 reason=VERTEX_DRAG kind=%d target=%p capture=%d hold=%d\n",
                     drag_ctx->target_kind,
                     drag_ctx->target,
                     drag_ctx->has_capture,
                     drag_ctx->pointer_state.left_button_hold_ticks);
        }
        return input->left_released ? INPUT_ROUTE_HANDLED : INPUT_ROUTE_CAPTURED;
    }

    if (input->left_pressed || (input->left_down && !drag_ctx->has_capture &&
                                drag_ctx->pointer_state.left_button_hold_ticks == 1))
    {
        bool captured = GeometryEditor_TryBeginDrag(world, pixel_coords, selected_object);
        if (log_input)
        {
            LOG_INFO("Editor input end: consumed=%d reason=PRESS kind=%d target=%p capture=%d hold=%d\n",
                     captured,
                     drag_ctx->target_kind,
                     drag_ctx->target,
                     drag_ctx->has_capture,
                     drag_ctx->pointer_state.left_button_hold_ticks);
        }
        return captured ? INPUT_ROUTE_CAPTURED : INPUT_ROUTE_IGNORED;
    }

    if (log_input)
    {
        LOG_INFO("Editor input end: consumed=0 reason=IDLE kind=%d target=%p capture=%d hold=%d\n",
                 drag_ctx->target_kind,
                 drag_ctx->target,
                 drag_ctx->has_capture,
                 drag_ctx->pointer_state.left_button_hold_ticks);
    }
    return INPUT_ROUTE_IGNORED;
}

void GeometryEditor_DrawHandles(const World2d *world, Camera2d *universe_camera)
{
    Newtonoid2d *object = UIState_GetSelectedObject();
    if (!GeometryEditor_IsEnabled(world, object) || object == NULL ||
        object->surface.surface_vectors.count == 0)
    {
        return;
    }

    Matrix3x3 world_to_pixel = ResolveWorldToPixelMatrix(world, universe_camera);
    Vector2d *vertices = object->surface.surface_vectors.items;
    for (size_t index = 0; index < object->surface.surface_vectors.count; index++)
    {
        Vector2d world_vertex = VectorSum_2d(vertices[index], object->anchor_position);
        Vector2d pixel_vertex = TransformCoordinates(world_to_pixel, world_vertex);
        Color colour = (index == (size_t)vertex_handle_target.vertex_index &&
                        vertex_handle_target.object == object)
                           ? YELLOW
                           : WHITE;
        DrawCircleV((Vector2){pixel_vertex.x, pixel_vertex.y}, 5.0f, colour);
        DrawCircleLines((int)pixel_vertex.x, (int)pixel_vertex.y, 5.0f, BLACK);
    }
}