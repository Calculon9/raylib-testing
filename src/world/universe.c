/**********************************************************************************************
*
UNIVERSE MODULE
*
**********************************************************************************************/
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "system/systems.h"
#include "system/click_resolver.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"
#include "common/common.h"
#include "camera/camera.h"

void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx);

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

void BindWorldTunnel(World2d *world, Camera2d *camera)
{
    world->tunnel.source_frame = &world->grid_space.space.frame;
    world->tunnel.destination_frame = &camera->frame;
    world->tunnel.source_to_dest_mtx = MtxTransform_GetLocalToParent(*world->tunnel.source_frame);
    world->tunnel.dest_to_source_mtx = MatrixInvert_3x3(world->tunnel.source_to_dest_mtx);
}

// Shared by Universe_Init/Universe_CreateWorld: bind temp tunnel pointers so CreateWorld can copy
// GridSpace2d, then rebind the tunnel to the world's persistent storage once it's copied in.
static bool CreateAndBindWorld(Universe *u, GridSpace2d space_g, float gravity,
                               Frame2d *initial_destination_frame, World2d *out_world)
{
    out_world->tunnel.source_frame = &space_g.space.frame;
    out_world->tunnel.destination_frame = initial_destination_frame;

    if (!CreateWorld(space_g, gravity, u, out_world))
    {
        return false;
    }

    BindWorldTunnel(out_world, &u->camera);
    return true;
}

static void PopulateStarterObjects(World2d *world, int requested_count)
{
    int object_count = requested_count;
    if (object_count < 0)
    {
        object_count = 0;
    }
    const int max_placement_attempts = 1000;
    for (int i = 0; i < object_count; i++)
    {
        int grid_width = world->grid_space.space.columns;
        int grid_height = world->grid_space.space.rows;
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
            Newtonoid2d *existing_objects = (Newtonoid2d *)world->objects.items;
            for (size_t existing_index = 0; existing_index < world->objects.count; existing_index++)
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
                AddObjectToWorld(world, &object, world->grid_space.object.id);
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
}

// ---------------------------------------------------------------------------
void Universe_Init(Universe *u, Vector2d default_spawn, Vector2d default_new_world_resolution, float default_gravity)
{
    u->world_count = 0;
    u->selected_world_index = -1;
    u->next_entity_id = 1;

    for (int i = 0; i < UNIVERSE_MAX_WORLDS; i++)
    {
        u->worlds[i].camera_marker_id = INVALID_ENTITY_ID;
        u->worlds[i].bounds_valid = false;
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

    // Root world's local space IS universe space: identity basis, frame origin at zero.
    // Its grid is centered on that origin so the camera-centered view (positive and negative coords) is indexable.
    Vector2d root_resolution = (u->resolution.x > 0.0f && u->resolution.y > 0.0f) ? u->resolution : (Vector2d){60.0f, 60.0f};
    GridSpace2d root_space = NewGridSpace2d(ZERO_VECTOR_2D, root_resolution, IDENTITY_BASIS_2D, COLOURLESS_RGBA, COLOURLESS_RGBA);
    root_space.space.grid_origin = VectorScale_2d(root_resolution, -0.5f);
    RebuildSpaceCells(&root_space.space);
    root_space.object.id = INVALID_ENTITY_ID;
    CreateAndBindWorld(u, root_space, 0.0f, &u->camera.frame, &u->root_world);
    // Pure container: not drawn as a grid, not selectable/draggable, not physics-ticked.
    u->root_world.flags = WORLD_FLAG_ACTIVE;
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
    if (!CreateAndBindWorld(u, space_g, u->next_gravity, u->camera.tunnel.source_frame, new_world))
    {
        LOG_ERROR("Cannot create world: world initialization failed.\n");
        return -1;
    }

    // Note: CreateWorld() already sets the standard active/visible/selectable/physics/spawns/draggable
    // flag set internally, so no reassignment is needed here.

    // Sync the world's frame origin to its anchored logical position in the universe
    new_world->grid_space.space.frame.origin_in_parent = world_center_in_universe;
    new_world->uni_coords_center = world_center_in_universe;

    World_RefreshBoundsFromFrame(new_world);

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
    new_world->camera_marker_id = AddObjectToWorld(new_world, &cam, new_world->grid_space.object.id);

    // Populate the new world with randomly placed starter objects.
    PopulateStarterObjects(new_world, u->next_object_count);

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

    BindWorldTunnel(world, &u->camera);

    World_RefreshBoundsFromFrame(world);
    RefreshWorldSpatialMap(world);
    return true;
}

// ---------------------------------------------------------------------------
void World_SetBounds(World2d *world, Vector2d min_bound, Vector2d max_bound)
{
    if (!world)
        return;

    world->bounds = (Matrix2x2){min_bound, max_bound};
    world->bounds_valid = true;
}

// Recomputes bounds from the world's current frame; shared by create/basis-edit/drag paths.
void World_RefreshBoundsFromFrame(World2d *world)
{
    if (!world)
        return;

    Matrix2x2 world_bounds = Frame_CalcAABB_InParent(&world->grid_space.space.frame);
    World_SetBounds(world, world_bounds.col1, world_bounds.col2);
}

// ---------------------------------------------------------------------------
static bool Universe_WorldBoundsContainsLocal(const ClickableSpace *space, Vector2d local_point)
{
    const World2d *world = (const World2d *)space->ctx;
    if (!world || !world->bounds_valid)
    {
        return false;
    }

    // world->bounds is stored in universe (parent) space, but the resolver hands us a
    // world-local point. Transform it back to parent space before testing the AABB so
    // the hit-test matches the original Universe_FindWorldAt behavior exactly.
    Vector2d parent_point = Frame_TransformPoint_ToParent(local_point, &space->frame);
    Vector2d min_bound = world->bounds.col1;
    Vector2d max_bound = world->bounds.col2;
    return parent_point.x >= min_bound.x && parent_point.y >= min_bound.y &&
           parent_point.x < max_bound.x && parent_point.y < max_bound.y;
}

static bool Universe_RootContainsLocal(const ClickableSpace *space, Vector2d local_point)
{
    (void)space;
    (void)local_point;
    return true;
}

int Universe_FindWorldAt(const Universe *u, Vector2d universe_point)
{
    if (u == NULL || u->world_count <= 0)
    {
        return -1;
    }

    // Build a one-shot sibling list from the universe's world array. The last world in the
    // array is visually top-most, so the resolver naturally prefers it over earlier worlds.
    ClickableSpace world_spaces[UNIVERSE_MAX_WORLDS] = {0};
    ClickableSpace *first = NULL;
    ClickableSpace *prev = NULL;

    for (int i = 0; i < u->world_count; i++)
    {
        ClickableSpace *space = &world_spaces[i];
        space->frame = u->worlds[i].grid_space.space.frame;
        space->contains_local = Universe_WorldBoundsContainsLocal;
        space->ctx = (void *)&u->worlds[i];
        space->user_data = i;

        if (prev == NULL)
        {
            first = space;
        }
        else
        {
            prev->next_sibling = space;
        }
        prev = space;
    }

    // Use a root node that covers the entire universe so we can resolve the sibling list.
    ClickableSpace root = {0};
    root.frame = CreateFrame2d(IDENTITY_BASIS_2D, ZERO_VECTOR_2D, u->resolution);
    root.contains_local = Universe_RootContainsLocal;
    root.first_child = first;

    const ClickableSpace *hit = Clickable_ResolvePoint(&root, universe_point, NULL);
    if (hit != NULL && hit != &root)
    {
        return hit->user_data;
    }

    return -1;
}

// Maps a 0..world_count logical index to a world, with world_count itself meaning root.
static World2d *Universe_GetWorldByLogicalIndex(const Universe *u, int logical_index, int *world_index_out)
{
    if (logical_index < u->world_count)
    {
        if (world_index_out)
        {
            *world_index_out = logical_index;
        }
        return (World2d *)&u->worlds[logical_index];
    }

    if (world_index_out)
    {
        *world_index_out = UNIVERSE_ROOT_WORLD_INDEX;
    }
    return (World2d *)&u->root_world;
}

Newtonoid2d *Universe_GetEntityByID(const Universe *u, EntityId entity_id, int *world_index_out)
{
    if (!u || entity_id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    for (int i = 0; i <= u->world_count; i++)
    {
        int world_index;
        World2d *world = Universe_GetWorldByLogicalIndex(u, i, &world_index);
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
    // Root world's local space is universe space, so the universe->pixel matrix applies directly.
    Matrix3x3 universe_to_pixel_mtx = ResolveWorldToPixelMatrix(&u->root_world, &u->camera);
    DrawNewtonoids(&u->root_world.objects, universe_to_pixel_mtx);
    DrawNewtonoids(&u->root_world.temp_objects, universe_to_pixel_mtx);

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
    if (!u)
        return NULL;
    if (index == UNIVERSE_ROOT_WORLD_INDEX)
        return &u->root_world;
    if (index < 0 || index >= u->world_count)
        return NULL;
    return &u->worlds[index];
}

// ---------------------------------------------------------------------------
Camera2d *Universe_GetCamera(Universe *u)
{
    return &u->camera;
}
