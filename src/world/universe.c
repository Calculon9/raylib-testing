/**********************************************************************************************
*
UNIVERSE MODULE
*
**********************************************************************************************/
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "system/systems.h"
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"

Universe G_Universe = {0};
Vector2d game_viewport_u = {0};
Vector2d game_viewport_v = {0};

static Basis2d ResolveCoordSpaceBasis(Vector2d requested_u, Vector2d requested_v)
{
    const float eps = 0.0001f;
    Basis2d result = IDENTITY_BASIS_2D;

    float u_mag = VectorMagnitude_2d(requested_u);
    float v_mag = VectorMagnitude_2d(requested_v);
    if (u_mag < eps || v_mag < eps)
    {
        LOG_WARN("Invalid coord-space basis magnitude. Falling back to identity basis. u=(%.3f,%.3f), v=(%.3f,%.3f)\n",
                 requested_u.x,
                 requested_u.y,
                 requested_v.x,
                 requested_v.y);
        return result;
    }

    result.u = VectorScale_2d(requested_u, 1.0f / u_mag);
    result.v = VectorScale_2d(requested_v, 1.0f / v_mag);

    float det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    if (fabsf(det) < eps)
    {
        // If vectors are nearly collinear, rebuild v perpendicular to u.
        result.v = (Vector2d){-result.u.y, result.u.x};
        det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    }

    if (det < 0.0f)
    {
        // Keep a consistent handedness for camera/grid transforms.
        result.v = VectorScale_2d(result.v, -1.0f);
    }

    return result;
}

static Camera2d BuildWorldToUniverseCameraTemplate(Vector2d space_resolution, Basis2d universe_basis, Basis2d world_basis, Vector2d world_center_in_universe)
{
    Vector2d source_focus_coords = VectorScale_2d(space_resolution, 0.5f);

    Camera2d coord_space_camera_template = CreateCamera2d(universe_basis, world_basis, world_center_in_universe, source_focus_coords);
    coord_space_camera_template.source_focus_coords = source_focus_coords;
    coord_space_camera_template.target_source_focus_coords = source_focus_coords;
    coord_space_camera_template.zoom = 1.0f;
    coord_space_camera_template.target_zoom = 1.0f;
    coord_space_camera_template.rotation = 0.0f;
    coord_space_camera_template.target_rotation = 0.0f;
    UpdateCameraFull(&coord_space_camera_template);

    return coord_space_camera_template;
}

// ---------------------------------------------------------------------------
void Universe_Init(Universe *u, Vector2d default_spawn, Vector2d default_new_world_resolution, Vector2d universe_resolution, float default_gravity)
{
    u->world_count = 0;
    u->selected_world_index = -1;

    for (int i = 0; i < UNIVERSE_MAX_WORLDS; i++)
    {
        u->camera_marker_ids[i] = 0;
        u->world_bounds_valid[i] = false;
        u->world_bounds_min[i] = ZERO_VECTOR_2D;
        u->world_bounds_max[i] = ZERO_VECTOR_2D;
    }

    u->camera_offset = ZERO_VECTOR_2D;
    u->resolution = universe_resolution;

    u->next_spawn = default_spawn;
    u->next_resolution = default_new_world_resolution;
    u->next_basis_u = (Vector2d){1.0f, 0.0f};
    u->next_basis_v = (Vector2d){0.0f, 1.0f};
    u->next_gravity = default_gravity;

    // Default step: 15 % of the coord-space extent so successive spaces are offset but visible.
    u->spawn_step.x = (default_new_world_resolution.x > 0.0f) ? default_new_world_resolution.x * 0.15f : 5.0f;
    u->spawn_step.y = (default_new_world_resolution.y > 0.0f) ? default_new_world_resolution.y * 0.15f : 5.0f;
}

// ---------------------------------------------------------------------------
int Universe_CreateWorld(Universe *u,
                         ColourRgba fill_colour,
                         ColourRgba line_colour,
                         ColourRgba camera_marker_colour,
                         Vector2d world_center_in_universe,
                         WorldState *world_state,
                         bool auto_select)
{
    if (u->world_count >= UNIVERSE_MAX_WORLDS)
    {
        LOG_WARN("Cannot create world: reached max world count (%d).\n", UNIVERSE_MAX_WORLDS);
        return -1;
    }

    Vector2d creation_res = (u->next_resolution.x > 0.0f && u->next_resolution.y > 0.0f)
                                ? u->next_resolution
                                : (Vector2d){16.0f, 9.0f}; // sensible fallback

    Basis2d world_basis = ResolveCoordSpaceBasis(u->next_basis_u, u->next_basis_v);
    u->next_basis_u = world_basis.u;
    u->next_basis_v = world_basis.v;

    CoordSpace2d_Grid space_g = NewCoordSpace2d_Grid(ZERO_VECTOR_2D, creation_res, world_basis, fill_colour, line_colour);
    space_g.object.id = 0;
    space_g.object.flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
    space_g.object.collision_mask = FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE | FLAG_TYPE_WALL;
    space_g.object.entity_layer = FLAG_TYPE_WALL;

    int new_index = u->world_count;
    World2d *new_world = &u->worlds[new_index];

    Camera2d world_camera = BuildWorldToUniverseCameraTemplate(creation_res, u->camera.source_basis, world_basis, world_center_in_universe);

    CreateWorld(space_g, world_camera, u->next_gravity, new_world);
    new_world->uni_coords_center = world_center_in_universe;

    // Place a camera-position marker at the world-space focus point.
    Newtonoid2d marker = CreateNewtonoid2d_Symmetric(4, 0.45f, camera_marker_colour, 1.0f,
                                                     new_world->camera.source_focus_coords,
                                                     ZERO_VECTOR_2D, ZERO_VECTOR_2D);

    marker.entity_layer = FLAG_TYPE_EFFECT;
    marker.collision_mask = 0;
    marker.flags = FLAG_STATUS_ALIVE;
    marker.line_colour = camera_marker_colour;
    marker.fill_colour = camera_marker_colour;
    u->camera_marker_ids[new_index] = AddObjectToWorld(new_world, &marker, new_world->coord_space_grid.object.id);

    u->world_count++;

    Matrix2x2 world_bounds = CalcCoordSpaceBoundsFromCenter(&new_world->coord_space_grid.coord_space, new_world->uni_coords_center);
    Vector2d world_bounds_min = world_bounds.col1;
    Vector2d world_bounds_max = world_bounds.col2;
    Universe_SetWorldBounds(u, new_index, world_bounds_min, world_bounds_max);

    // Advance the default spawn so the next world is offset by default.
    u->next_spawn = VectorSum_2d(u->next_spawn, u->spawn_step);

    if (auto_select)
    {
        u->selected_world_index = new_index;
    }

    // Wire WorldState to the new world.
    if (world_state)
    {
        world_state->world = new_world;
        world_state->entity_world_index_registry = &new_world->entity_world_index_registry;
        world_state->collisions = &new_world->collisions;
        world_state->selected_object = NULL;
        world_state->selected_cell = NULL;
    }

    LOG_INFO("Universe_CreateWorld -> index=%d universe_pos=(%.1f,%.1f) res=(%.0fx%.0f)\n",
             new_index,
             new_world->uni_coords_center.x, new_world->uni_coords_center.y,
             creation_res.x, creation_res.y);

    return new_index;
}

// ---------------------------------------------------------------------------
bool Universe_SelectWorld(Universe *u, int index)
{
    if (index < 0 || index >= u->world_count)
        return false;

    u->selected_world_index = index;

    return true;
}

// ---------------------------------------------------------------------------
void Universe_SetWorldBounds(Universe *u, int index, Vector2d min_bound, Vector2d max_bound)
{
    if (u == NULL || index < 0 || index >= UNIVERSE_MAX_WORLDS)
        return;

    u->world_bounds_min[index] = min_bound;
    u->world_bounds_max[index] = max_bound;
    u->world_bounds_valid[index] = true;
}

// ---------------------------------------------------------------------------
int Universe_FindWorldAt(const Universe *u, Vector2d universe_point)
{
    if (u == NULL)
        return -1;

    for (int i = 0; i < u->world_count; i++)
    {
        if (!u->world_bounds_valid[i])
            continue;

        Vector2d min_bound = u->world_bounds_min[i];
        Vector2d max_bound = u->world_bounds_max[i];
        if (universe_point.x >= min_bound.x && universe_point.y >= min_bound.y &&
            universe_point.x < max_bound.x && universe_point.y < max_bound.y)
        {
            return i;
        }
    }

    return -1;
}

// ---------------------------------------------------------------------------
void Universe_Draw(Universe *u)
{
    for (int i = 0; i < u->world_count; i++)
    {
        World2d *w = &u->worlds[i];
        // Camera2d *draw_camera = &u->camera;
        // Vector2d draw_origin = WorldTopLeftInUniverse(w);
        // // Create composite camera for drawing the world in universe space.
        // Matrix3x3 world_to_universe_mtx = MatrixMultiply_3x3_3x3(w->camera.source_to_dest_mtx, u->camera.source_to_dest_mtx);
        // if (u->selected_world_index == i)
        // {
        //     draw_camera = &w->camera;
        //     draw_origin = ZERO_VECTOR_2D;
        // }

        // Universe view must be camera-stable: selection changes state, not rendering transform.
        DrawWorldRegion(w, &w->camera, &u->camera);
    }
}

// ---------------------------------------------------------------------------
bool Universe_ResolveClick(Universe *u, Vector2d universe_click, Vector2d *local_out)
{
    // Find which world (if any) contains the click point.
    int clicked_world = Universe_FindWorldAt(u, universe_click);
    if (clicked_world >= 0)
    {
        // Select the world immediately. Selection should occur regardless of the
        // local coordinate check, because the world bounds already guarantee the
        // click is inside the world.
        if (clicked_world != u->selected_world_index)
            Universe_SelectWorld(u, clicked_world);

        // Compute local coordinates relative to the world top-left for callers that need it.
        if (local_out)
        {
            World2d *w = &u->worlds[clicked_world];
            CoordSystem2d universe_space = CreateCoordSystem2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D);
            CoordSystem2d world_space = CreateCoordSystem2d(w->coord_space_grid.coord_space.system.basis,
                                                           CalcCoordSpaceOriginFromCenter(&w->coord_space_grid.coord_space, w->uni_coords_center));
            Matrix3x3 universe_to_world_mtx = CoordSystemTransform_2d(universe_space, world_space);
            *local_out = TransformCoordinates(universe_to_world_mtx, universe_click);
        }
        return true;
    }
    // No world hit – indicate no selection.
    return false;
}

// ---------------------------------------------------------------------------
int Universe_GetWorldCount(const Universe *u)
{
    return u->world_count;
}

int Universe_GetSelectedIndex(const Universe *u)
{
    return u->selected_world_index;
}

World2d *Universe_GetSelectedWorld(Universe *u)
{
    if (u->selected_world_index < 0 || u->selected_world_index >= u->world_count)
        return NULL;
    return &u->worlds[u->selected_world_index];
}

World2d *Universe_GetWorld(Universe *u, int index)
{
    if (index < 0 || index >= u->world_count)
        return NULL;
    return &u->worlds[index];
}

// ---------------------------------------------------------------------------
Camera2d *Universe_GetCamera(Universe *u)
{
    return &u->camera;
}