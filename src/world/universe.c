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
#include "math/affine_space_ops.h"
#include "common/common.h"
#include "camera/camera.h"

Universe G_Universe = {0};
extern ColourRgba camera_marker_colour;

static Basis2d ResolveFrameBasis_UserInput(Vector2d requested_u, Vector2d requested_v)
{
    const float eps = 0.0001f;
    Basis2d result = IDENTITY_BASIS_2D;

    float u_mag = VectorMagnitude_2d(requested_u);
    float v_mag = VectorMagnitude_2d(requested_v);
    if (u_mag < eps || v_mag < eps)
    {
        LOG_WARN("Invalid coord-space basis magnitude. Falling back to identity basis. u=(%.3f,%.3f), v=(%.3f,%.3f)\n",
                 requested_u.x, requested_u.y, requested_v.x, requested_v.y);
        return result;
    }

    // Preserve authored basis magnitudes so the basis editor affects scale as well as direction.
    result.u = requested_u;
    result.v = requested_v;

    float det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    if (fabsf(det) < eps)
    {
        // If vectors are nearly collinear, rebuild v perpendicular to u while preserving v magnitude.
        Vector2d u_unit = VectorNormalize_2d(result.u);
        result.v = (Vector2d){-u_unit.y * v_mag, u_unit.x * v_mag};
        det = (result.u.x * result.v.y) - (result.u.y * result.v.x);
    }

    if (det < 0.0f)
    {
        // Keep a consistent handedness for camera/grid transforms.
        result.v = VectorScale_2d(result.v, -1.0f);
    }

    return result;
}

// ---------------------------------------------------------------------------
void Universe_Init(Universe *u, Vector2d default_spawn, Vector2d default_new_world_resolution, float default_gravity)
{
    u->world_count = 0;
    u->selected_world_index = -1;
    u->next_entity_id = 1;

    for (int i = 0; i < UNIVERSE_MAX_WORLDS; i++)
    {
        u->camera_marker_ids[i] = INVALID_ENTITY_ID;
        u->world_bounds_valid[i] = false;
        u->world_bounds_min[i] = ZERO_VECTOR_2D;
        u->world_bounds_max[i] = ZERO_VECTOR_2D;
    }

    u->next_spawn = default_spawn;
    u->next_resolution = default_new_world_resolution;
    u->next_basis_u = (Vector2d){1.0f, 0.0f};
    u->next_basis_v = (Vector2d){0.0f, 1.0f};
    u->next_gravity = default_gravity;
    u->next_object_count = 0;

    // Default step: 15 % of the coord-space extent so successive spaces are offset but visible.
    u->spawn_step.x = (default_new_world_resolution.x > 0.0f) ? default_new_world_resolution.x * 0.15f : 5.0f;
    u->spawn_step.y = (default_new_world_resolution.y > 0.0f) ? default_new_world_resolution.y * 0.15f : 5.0f;
}

EntityId Universe_AllocateEntityId(Universe *u)
{
    if (!u || u->next_entity_id == INVALID_ENTITY_ID)
    {
        return INVALID_ENTITY_ID;
    }

    return u->next_entity_id++;
}

// ---------------------------------------------------------------------------
int Universe_CreateWorld(Universe *u, ColourRgba fill_colour, ColourRgba line_colour, ColourRgba camera_marker_colour, Vector2d world_center_in_universe,
                         bool auto_select)
{
    if (u->world_count >= UNIVERSE_MAX_WORLDS)
    {
        LOG_WARN("Cannot create world: reached max world count (%d).\n", UNIVERSE_MAX_WORLDS);
        return -1;
    }

    Vector2d requested_res = (u->next_resolution.x > 0.0f && u->next_resolution.y > 0.0f) ? u->next_resolution : (Vector2d){7.0f, 5.0f}; // sensible fallback

    Basis2d world_basis = ResolveFrameBasis_UserInput(u->next_basis_u, u->next_basis_v);
    u->next_basis_u = world_basis.u;
    u->next_basis_v = world_basis.v;

    GridSpace2d space_g = NewGridSpace2d(world_center_in_universe, requested_res, world_basis, fill_colour, line_colour);
    space_g.object.id = INVALID_ENTITY_ID;
    space_g.object.status_flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
    space_g.object.collision_mask = FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE | FLAG_TYPE_WALL;
    space_g.object.entity_flags = FLAG_TYPE_WALL;

    int new_index = u->world_count;
    World2d *new_world = &u->worlds[new_index];
    // Temporary source pointer before CreateWorld copies GridSpace2d into world-owned storage.
    new_world->tunnel.source_frame = &space_g.space.frame;
    new_world->tunnel.destination_frame = u->camera.tunnel.source_frame;

    if (!CreateWorld(space_g, u->next_gravity, u, new_world))
    {
        LOG_ERROR("Cannot create world: world initialization failed.\n");
        return -1;
    }

    // Rebind tunnel frames to persistent storage owned by the world and universe camera.
    new_world->tunnel.source_frame = &new_world->grid_space.space.frame;
    new_world->tunnel.destination_frame = &u->camera.frame;

    // Sync the world's frame origin to its anchored logical position in the universe
    new_world->grid_space.space.frame.origin_in_parent = world_center_in_universe;
    new_world->uni_coords_center = world_center_in_universe;

    // Keep world tunnel in stable world-local <-> universe space (camera zoom must not affect it).
    new_world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*new_world->tunnel.source_frame);
    new_world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(new_world->tunnel.source_to_dest_mtx);

    Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&new_world->grid_space.space.frame);
    Universe_SetWorldBounds(u, new_index, world_bounds.col1, world_bounds.col2);

    // Advance the default spawn so the next world is offset by default.
    u->next_spawn = VectorSum_2d(u->next_spawn, u->spawn_step);

    if (auto_select)
    {
        u->selected_world_index = new_index;
    }

    // Place marker in world-local space; world placement is applied by world transforms.
    Vector2d cam_local_coords = {0.5f, 0.5f};
    Newtonoid2d cam = CreateNewtonoid2d_Symmetric(24, 0.45f, camera_marker_colour, 1.0f,
                                                  cam_local_coords, ZERO_VECTOR_2D, ZERO_VECTOR_2D);

    cam.entity_flags = FLAG_TYPE_CAMERA | FLAG_TYPE_EFFECT; // Effect makes it immune to physics (currently)
    cam.collision_mask = 0;
    cam.status_flags = FLAG_STATUS_ALIVE;
    cam.line_colour = camera_marker_colour;
    cam.fill_colour = camera_marker_colour;
    u->camera_marker_ids[new_index] = AddObjectToWorld(new_world, &cam, new_world->grid_space.object.id);

    // Populate the new world with randomly placed starter objects.
    int object_count = u->next_object_count;
    if (object_count < 0)
    {
        object_count = 0;
    }
    const int max_placement_attempts = 1000;
    for (int i = 0; i < object_count; i++)
    {
        int grid_width = new_world->grid_space.space.columns;
        int grid_height = new_world->grid_space.space.rows;
        if (grid_width <= 0 || grid_height <= 0)
        {
            break;
        }

        bool placed = false;
        for (int attempt = 0; attempt < max_placement_attempts && !placed; attempt++)
        {
            float half_width = 0.2f;
            float half_height = 0.2f;
            Vector2d object_coords = {
                GetRandomFloat(half_width, (float)grid_width - half_width),
                GetRandomFloat(half_height, (float)grid_height - half_height)};

            float angle = GetRandomFloat(0.0f, 2 * PI);
            Vector2d velocity = {2.0f * cosf(angle), 2.0f * sinf(angle)};
            Newtonoid2d object = CreateNewtonoid2d_Symmetric(
                6, 0.2f, (ColourRgba){155, 0, 0, 255}, 1.0f,
                object_coords, velocity, ZERO_VECTOR_2D);
            object.entity_flags = FLAG_TYPE_NEWTONOID;
            object.collision_mask = FLAG_TYPE_NEWTONOID | FLAG_TYPE_WALL;
            object.status_flags = FLAG_STATUS_ALIVE;
            object.line_colour = (ColourRgba){155, 0, 0, 255};
            object.fill_colour = (ColourRgba){155, 0, 0, 255};

            bool overlaps_existing = false;
            Newtonoid2d *existing_objects = (Newtonoid2d *)new_world->objects.items;
            for (size_t existing_index = 0; existing_index < new_world->objects.count; existing_index++)
            {
                Newtonoid2d *existing = &existing_objects[existing_index];
                if ((existing->entity_flags & FLAG_TYPE_EFFECT) == 0 &&
                    CheckForCollision_AABB(object, *existing))
                {
                    overlaps_existing = true;
                    break;
                }
            }

            if (!overlaps_existing)
            {
                AddObjectToWorld(new_world, &object, new_world->grid_space.object.id);
                placed = true;
            }
        }

        if (!placed)
        {
            LOG_WARN("Could not place starter object %d without AABB overlap after %d attempts.\n",
                     i, max_placement_attempts);
            break;
        }
    }

    u->world_count++;

    LOG_INFO("Universe_CreateWorld -> index=%d universe_pos=(%.1f,%.1f) res=(%.0fx%.0f)\n", new_index,
             new_world->grid_space.space.frame.origin_in_parent.x, new_world->grid_space.space.frame.origin_in_parent.y,
             requested_res.x, requested_res.y);

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

bool Universe_SetWorldBasis(Universe *u, int index, Vector2d basis_u, Vector2d basis_v)
{
    if (!u || index < 0 || index >= u->world_count)
    {
        return false;
    }

    float determinant = (basis_u.x * basis_v.y) - (basis_u.y * basis_v.x);
    if (VectorMagnitude_2d(basis_u) < 0.0001f ||
        VectorMagnitude_2d(basis_v) < 0.0001f ||
        fabsf(determinant) < 0.0001f)
    {
        return false;
    }

    World2d *world = &u->worlds[index];
    world->grid_space.space.frame.basis = (Basis2d){basis_u, basis_v};
    RebuildSpaceCells(&world->grid_space.space);

    world->tunnel.source_frame = &world->grid_space.space.frame;
    world->tunnel.destination_frame = &u->camera.frame;
    world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*world->tunnel.source_frame);
    world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(world->tunnel.source_to_dest_mtx);

    Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&world->grid_space.space.frame);
    Universe_SetWorldBounds(u, index, world_bounds.col1, world_bounds.col2);
    RefreshWorldSpatialMap(world);
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

int Universe_FindWorldContainingObject(const Universe *u, const Newtonoid2d *object)
{
    if (!u || !object)
    {
        return -1;
    }

    for (int world_index = 0; world_index < u->world_count; world_index++)
    {
        const World2d *world = &u->worlds[world_index];
        const LArray *object_arrays[] = {&world->objects, &world->temp_objects};
        for (size_t array_index = 0; array_index < 2; array_index++)
        {
            const LArray *object_array = object_arrays[array_index];
            const Newtonoid2d *objects = (const Newtonoid2d *)object_array->items;
            for (size_t object_index = 0; object_index < object_array->count; object_index++)
            {
                if (&objects[object_index] == object)
                {
                    return world_index;
                }
            }
        }
    }

    return -1;
}

Newtonoid2d *Universe_GetEntityByID(const Universe *u, EntityId entity_id, int *world_index_out)
{
    if (!u || entity_id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    for (int world_index = 0; world_index < u->world_count; world_index++)
    {
        World2d *world = (World2d *)&u->worlds[world_index];
        Newtonoid2d *entity = (Newtonoid2d *)GetEntityByID(world, entity_id);
        if (entity)
        {
            if (world_index_out)
            {
                *world_index_out = world_index;
            }
            return entity;
        }
    }

    return NULL;
}

// ---------------------------------------------------------------------------
void Universe_Draw(Universe *u)
{
    for (int i = 0; i < u->world_count; i++)
    {
        World2d *w = &u->worlds[i];
        DrawWorldRegion(w, &u->camera);
    }
}

// ---------------------------------------------------------------------------
bool Universe_ResolveClick(Universe *u, Vector2d universe_click, Vector2d *local_out)
{
    if (!u)
    {
        return false;
    }

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
            // Frame2d universe_space = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, u->resolution);
            // Frame2d world_space = w->grid_space.space.frame;
            Matrix3x3 universe_to_world_mtx = w->tunnel.dest_to_source_mtx; // Transform from universe space to world space
            *local_out = TransformCoordinates(universe_to_world_mtx, universe_click);
        }
        return true;
    }
    // No world hit - indicate no selection.
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
