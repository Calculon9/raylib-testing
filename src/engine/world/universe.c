/**********************************************************************************************
*
UNIVERSE MODULE
*
**********************************************************************************************/
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "world/world_growth.h"
#include "editor/geometry_editor.h"
#include "system/systems.h"
#include "system/click_resolver.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"
#include "common/common.h"
#include "camera/camera.h"
#include "entities/entity_registry.h"
#include "entities/health.h"

void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx);

Universe G_Universe = {0};
extern ColourRgba camera_marker_colour;

static Basis2d ResolveFrameBasis_UserInput(Vector2d requested_u, Vector2d requested_v)
{
    Basis2d result = IDENTITY_BASIS_2D;
    if (!Basis2d_NormaliseAndValidate((Basis2d){requested_u, requested_v}, &result))
    {
        LOG_WARN("Invalid coord-space basis magnitude. Falling back to identity basis. u=(%.3f,%.3f), v=(%.3f,%.3f)\n",
                 requested_u.x, requested_u.y, requested_v.x, requested_v.y);
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
            Newtonoid_ConfigureMetadata(&object, ENTITY_ROLE_NEWTONOID,
                                         ENTITY_DEFAULT_COLLISION_ROLE_MASK,
                                         ENTITY_CAPABILITY_NONE,
                                         ENTITY_CONSTRAINT_NONE,
                                         ENTITY_STATUS_FLAG_ALIVE,
                                         (ColourRgba){155, 0, 0, 255},
                                         (ColourRgba){155, 0, 0, 255});
            bool overlaps_existing = false;
            Newtonoid2d *existing_objects = (Newtonoid2d *)world->objects.items;
            for (size_t existing_index = 0; existing_index < world->objects.count; existing_index++)
            {
                Newtonoid2d *existing = &existing_objects[existing_index];
                if ((existing->roles & ENTITY_ROLE_EFFECT) == 0 &&
                    CheckForCollision_AABB(object, *existing))
                {
                    overlaps_existing = true;
                    break;
                }
            }

            if (!overlaps_existing)
            {
                EntityId object_id = EntityRegistry_AllocateId();
                HealthComponentParams health_params = {.max_health = 3.0f};
                HealthComponent health = {0};
                if (object_id != INVALID_ENTITY_ID &&
                    HealthComponent_Initialise(&health, &health_params) &&
                    EntityRegistry_AttachHealth(object_id, &health))
                {
                    object.id = object_id;
                    EntityLifecycle_ApplyAttachedComponent(&object, ENTITY_COMPONENT_HEALTH, &health);
                    if (AddObjectToWorld(world, &object, world->grid_space.object.id) != INVALID_ENTITY_ID)
                    {
                        placed = true;
                    }
                    else
                    {
                        EntityRegistry_ReleaseId(object_id);
                    }
                }
                else if (object_id != INVALID_ENTITY_ID)
                {
                    EntityRegistry_ReleaseId(object_id);
                }
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
    Vector2d root_resolution = (u->resolution.x > 0.0f && u->resolution.y > 0.0f) ? u->resolution : (Vector2d){250.0f, 250.0f};
    GridSpace2d root_space = NewGridSpace2d(ZERO_VECTOR_2D, root_resolution, IDENTITY_BASIS_2D, COLOURLESS_RGBA, COLOURLESS_RGBA);
    root_space.space.grid_origin = VectorScale_2d(root_resolution, -0.5f);
    RebuildSpaceCells(&root_space.space);
    root_space.object.id = INVALID_ENTITY_ID;
    root_space.object.constraints = ENTITY_CONSTRAINT_NONE;
    CreateAndBindWorld(u, root_space, 0.0f, &u->camera.frame, &u->root_world);
    // Pure container: not drawn as a grid, not selectable/draggable, not physics-ticked.
    u->root_world.flags = WORLD_FLAG_ACTIVE;
}

// ---------------------------------------------------------------------------
int Universe_CreateWorld(Universe *u, ColourRgba fill_colour, ColourRgba line_colour, ColourRgba camera_marker_colour, Vector2d world_center_in_universe)
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
    space_g.object.constraints = ENTITY_CONSTRAINT_NONE;
    space_g.object.status_flags = ENTITY_STATUS_FLAG_ALIVE;
    space_g.object.collision_role_mask = ENTITY_DEFAULT_COLLISION_ROLE_MASK;
    space_g.object.roles = ENTITY_ROLE_WALL;

    int new_index = u->world_count;
    World2d *new_world = &u->worlds[new_index];
    if (!CreateAndBindWorld(u, space_g, u->next_gravity, u->camera.tunnel.source_frame, new_world))
    {
        LOG_ERROR("Cannot create world: world initialization failed.\n");
        return -1;
    }

    // Sync the world's frame origin to its anchored logical position in the universe
    new_world->grid_space.space.frame.origin_in_parent = world_center_in_universe;
    new_world->uni_coords_center = world_center_in_universe;

    World_RefreshBoundsFromFrame(new_world);

    // Advance the default spawn so the next world is offset by default.
    u->next_spawn = VectorSum_2d(u->next_spawn, u->spawn_step);

    // Place marker in world-local space; world placement is applied by world transforms.
    Vector2d cam_local_coords = {0.5f, 0.5f};
    Newtonoid2d cam = CreateNewtonoid2d_Symmetric(24, 0.45f, camera_marker_colour, 1.0f,
                                                  cam_local_coords, ZERO_VECTOR_2D, ZERO_VECTOR_2D);

    // Camera markers are visual-only entities and therefore do not participate in collisions.
    Newtonoid_ConfigureMetadata(&cam, ENTITY_ROLE_CAMERA | ENTITY_ROLE_EFFECT, ENTITY_ROLE_NONE,
                                ENTITY_CAPABILITY_NONE, ENTITY_CONSTRAINT_NONE,
                                ENTITY_STATUS_FLAG_ALIVE, camera_marker_colour,
                                camera_marker_colour);
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
// Delete a world, compact the world array, and repair tunnel bindings.
bool Universe_DeleteWorld(Universe *u, int index)
{
    if (!u || index < 0 || index >= u->world_count)
    {
        return false;
    }

    DestroyWorld(&u->worlds[index]);

    // Compact the world array by shifting all worlds after the deleted one down by one slot.
    int last_world_index = u->world_count - 1;
    for (int world_index = index; world_index < last_world_index; world_index++)
    {
        u->worlds[world_index] = u->worlds[world_index + 1];
    }
    MemorySet(&u->worlds[last_world_index], 0, sizeof(u->worlds[last_world_index]));
    u->world_count--;

    // Compaction copies tunnel pointers, so rebind every surviving world to its new array slot.
    for (int world_index = 0; world_index < u->world_count; world_index++)
    {
        BindWorldTunnel(&u->worlds[world_index], &u->camera);
    }

    LOG_INFO("Universe_DeleteWorld -> index=%d remaining=%d\n", index, u->world_count);
    return true;
}

bool Universe_SetWorldBasis(Universe *u, int index, Vector2d basis_u, Vector2d basis_v)
{
    if (!u || index < 0 || index >= u->world_count)
    {
        return false;
    }

    Basis2d normalised_basis;
    if (!Basis2d_NormaliseAndValidate((Basis2d){basis_u, basis_v}, &normalised_basis))
    {
        return false;
    }

    World2d *world = &u->worlds[index];
    world->grid_space.space.frame.basis = normalised_basis;
    RebuildSpaceCells(&world->grid_space.space);

    BindWorldTunnel(world, &u->camera);

    World_RefreshBoundsFromFrame(world);
    RefreshWorldSpatialMap(world);
    return true;
}

// Transform the lattice rectangle [x0,x1) x [y0,y1) of a world's grid into universe space as a quad
// (corner order is irrelevant to the separating-axis test, only that it traces the perimeter).
static void Universe_LatticeRectToQuad(const World2d *world, int x0, int y0, int x1, int y1, Vector2d out_quad[4])
{
    const Frame2d *frame = &world->grid_space.space.frame;
    out_quad[0] = Frame_TransformPoint_ToParent((Vector2d){(float)x0, (float)y0}, frame);
    out_quad[1] = Frame_TransformPoint_ToParent((Vector2d){(float)x1, (float)y0}, frame);
    out_quad[2] = Frame_TransformPoint_ToParent((Vector2d){(float)x1, (float)y1}, frame);
    out_quad[3] = Frame_TransformPoint_ToParent((Vector2d){(float)x0, (float)y1}, frame);
}

// R3 test. True when the lattice rectangle [x0,x1)x[y0,y1) of world `world_index`, mapped into universe
// space through its frame, strictly overlaps the FULL GRID PARALLELOGRAM of any OTHER nested world
// (that world's grid_origin .. grid_origin + columns/rows mapped through its frame, holes included).
// This is an exact quad test and is deliberately NOT the frame-AABB picking used by Universe_FindWorldAt.
// "Strictly" means overlap deeper than eps on every separating axis, so abutting worlds are allowed.
bool Universe_StripOverlapsOtherWorld(const Universe *u, int world_index, int x0, int y0, int x1, int y1, float eps)
{
    if (!u || world_index < 0 || world_index >= u->world_count)
    {
        return false;
    }

    Vector2d strip_quad[4];
    Universe_LatticeRectToQuad(&u->worlds[world_index], x0, y0, x1, y1, strip_quad);

    for (int i = 0; i < u->world_count; i++)
    {
        if (i == world_index)
        {
            continue;
        }
        const Space2d *other = &u->worlds[i].grid_space.space;
        int ox = (int)other->grid_origin.x;
        int oy = (int)other->grid_origin.y;
        Vector2d other_quad[4];
        Universe_LatticeRectToQuad(&u->worlds[i], ox, oy, ox + other->columns, oy + other->rows, other_quad);
        if (WorldGrowth_QuadsOverlapStrict(strip_quad, other_quad, eps))
        {
            return true;
        }
    }
    return false;
}

// Authoritative growth entry: grow nested world `world_index` so lattice cell (lattice_x, lattice_y) is inside
// its grid. Validates in order: R5 root/invalid world, R2 adjacency to a PRESENT cell, R4 the 512 cap, R3 no
// overlap with another world; then grows, refreshes frame bounds, tunnel and spatial map. Logs every refusal.
// Changes nothing on refusal or failure. Cells that are already inside return WORLD_GROWTH_NOT_NEEDED.
// DRAIN-TIME ONLY: never call outside the command drain (ProcessCommandQueue); growth reallocates the cell
// buffer, so any Cell * held elsewhere would dangle.
WorldGrowthStatus Universe_GrowWorldToInclude(Universe *u, int world_index, int lattice_x, int lattice_y)
{
    World2d *world = (u && world_index >= 0 && world_index < u->world_count) ? &u->worlds[world_index] : NULL;
    if (!world)
    {
        LOG_WARN("World growth refused: invalid or root world index %d (the root world is never grown).\n", world_index);
        return WORLD_GROWTH_INVALID_WORLD;
    }

    Space2d *space = &world->grid_space.space;
    int origin_x = (int)space->grid_origin.x;
    int origin_y = (int)space->grid_origin.y;

    GridGrowthPlan plan;
    GrowthResult planned = WorldGrowth_Plan(space->columns, space->rows, origin_x, origin_y,
                                            lattice_x, lattice_y, MAX_WORLD_GRID_DIMENSION, &plan);
    if (planned == GROWTH_NOT_NEEDED)
    {
        return WORLD_GROWTH_NOT_NEEDED;
    }

    // R2: the new cell must edge-touch a PRESENT cell. Re-checked here (not only at click time) because
    // state can change between the click and the drain.
    if (!WorldGrowth_TouchesPresentCell((const Cell *)space->cells.items, space->columns, space->rows,
                                        origin_x, origin_y, lattice_x, lattice_y))
    {
        LOG_INFO("World growth refused: cell (%d,%d) does not touch a present cell of world %d.\n",
                 lattice_x, lattice_y, world_index);
        return WORLD_GROWTH_REFUSED_NOT_ADJACENT;
    }

    // R4: cap.
    if (planned == GROWTH_REFUSED_CAP)
    {
        LOG_WARN("World growth refused: world %d grid would exceed %dx%d.\n",
                 world_index, MAX_WORLD_GRID_DIMENSION, MAX_WORLD_GRID_DIMENSION);
        return WORLD_GROWTH_REFUSED_CAP;
    }

    // R3: test only the newly added strip so already-overlapping worlds can still grow away from each other.
    int sx0, sy0, sx1, sy1;
    WorldGrowth_AddedStrip(&plan, &sx0, &sy0, &sx1, &sy1);
    if (Universe_StripOverlapsOtherWorld(u, world_index, sx0, sy0, sx1, sy1, WORLD_GROWTH_OVERLAP_EPS))
    {
        LOG_WARN("World growth refused: the new cells of world %d would overlap another world.\n", world_index);
        return WORLD_GROWTH_REFUSED_OVERLAP;
    }

    if (!Space_GrowCells(space, &plan))
    {
        return WORLD_GROWTH_FAILED_ALLOC; // Space_GrowCells already logged; the world is untouched.
    }

    // Rebuild everything derived from the old buffer/frame extent. origin_in_parent and the basis are
    // unchanged, so every existing local point still maps to the same universe point (no visual jump).
    BindWorldTunnel(world, &u->camera);
    World_RefreshBoundsFromFrame(world);
    RefreshWorldSpatialMap(world);

    LOG_INFO("World %d grown: %dx%d origin (%d,%d) -> %dx%d origin (%d,%d)\n", world_index,
             plan.old_columns, plan.old_rows, plan.old_origin_x, plan.old_origin_y,
             plan.new_columns, plan.new_rows, plan.new_origin_x, plan.new_origin_y);
    return WORLD_GROWTH_OK;
}

// Shrink nested world `world_index` to the tight bounding box of its PRESENT cells after the cell at lattice
// (deleted_lx, deleted_ly) was cleared. Cheap exit when that cell was not on an outer row/column. The box is then
// widened to keep the lattice cell holding the centre (anchor_position) of every entity owned by this world, so a
// trim never strands an entity outside the grid: those cells are absent (walls) and the existing recovery pass
// moves the entity out, after which a later delete can trim further. Entities are never moved here.
// Returns true when the grid was resized. A failed allocation leaves the world untouched (logged).
// DRAIN-TIME ONLY: never call outside the command drain (ProcessCommandQueue); a trim reallocates the cell
// buffer, so any Cell * held elsewhere would dangle.
bool Universe_TrimWorldToPresentCells(Universe *u, int world_index, int deleted_lx, int deleted_ly)
{
    World2d *world = (u && world_index >= 0 && world_index < u->world_count) ? &u->worlds[world_index] : NULL;
    if (!world)
    {
        return false; // The root world (and invalid indices) are never trimmed.
    }

    Space2d *space = &world->grid_space.space;
    int origin_x = (int)space->grid_origin.x;
    int origin_y = (int)space->grid_origin.y;
    if (!space->cells.items)
    {
        return false;
    }

    int x0, y0, x1, y1;
    if (!WorldGrowth_TightPresentRect((const Cell *)space->cells.items, space->columns, space->rows,
                                      origin_x, origin_y, deleted_lx, deleted_ly, &x0, &y0, &x1, &y1))
    {
        return false; // No present cell would remain; the handler refuses that delete before reaching here.
    }
    if (x0 == origin_x && y0 == origin_y && x1 == origin_x + space->columns && y1 == origin_y + space->rows)
    {
        return false; // Already tight.
    }

    // B4: widen the box to include the cell containing each world-owned entity's centre. Entities are
    // enumerated exactly as RefreshWorldSpatialMap does: the world's object array, filtered to those parented
    // to this world's grid object. Centres already outside the old grid are skipped (the trim cannot make
    // them any more outside).
    const Newtonoid2d *objects = (const Newtonoid2d *)world->objects.items;
    for (size_t i = 0; objects && i < world->objects.count; i++)
    {
        if (objects[i].parent_id != world->grid_space.object.id)
            continue;
        int ex, ey;
        if (WorldGrowth_LatticeFromLocal(space->grid_origin.x, space->grid_origin.y,
                                         objects[i].anchor_position.x, objects[i].anchor_position.y, &ex, &ey) &&
            WorldGrowth_IndexFromLattice(space->columns, space->rows, origin_x, origin_y, ex, ey) >= 0)
        {
            WorldGrowth_RectInclude(&x0, &y0, &x1, &y1, ex, ey);
        }
    }

    GridGrowthPlan plan;
    if (WorldGrowth_PlanToRect(space->columns, space->rows, origin_x, origin_y, x0, y0, x1, y1,
                               MAX_WORLD_GRID_DIMENSION, &plan) != GROWTH_OK)
    {
        return false; // Entities pinned the box to the full grid, or the plan was invalid: nothing to do.
    }

    if (!Space_ResizeCells(space, &plan))
    {
        return false; // Space_ResizeCells already logged; the world is untouched and simply stays larger.
    }

    // Same refresh sequence as growth: origin_in_parent and the basis are untouched, so the world does not move.
    BindWorldTunnel(world, &u->camera);
    World_RefreshBoundsFromFrame(world);
    RefreshWorldSpatialMap(world);

    LOG_INFO("World %d trimmed: %dx%d origin (%d,%d) -> %dx%d origin (%d,%d)\n", world_index,
             plan.old_columns, plan.old_rows, plan.old_origin_x, plan.old_origin_y,
             plan.new_columns, plan.new_rows, plan.new_origin_x, plan.new_origin_y);
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

Newtonoid2d *Universe_GetEntityByID(const Universe *u, EntityId entity_id, int *world_index_out)
{
    if (!u)
    {
        return NULL;
    }

    // Determine the world that owns the entity.
    EntityId world_id = EntityRegistry_GetEntityWorldId(entity_id);
    if (world_id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    // Retrieve the entity from the registry.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (!entity)
    {
        return NULL;
    }

    if (world_index_out)
    {
        *world_index_out = Universe_GetWorldIndexById(u, world_id);
    }

    return entity;
}

// Confirm both entity identity and the world that currently owns the entity.
bool Universe_IsEntityOwnedByWorld(const Universe *u, const World2d *world, const Newtonoid2d *entity)
{
    if (!u || !world || !entity)
    {
        return false;
    }

    return EntityRegistry_GetEntityWorldId(entity->id) == world->grid_space.object.id &&
           EntityRegistry_GetEntity(entity->id) == entity;
}

// ---------------------------------------------------------------------------
void Universe_Draw(Universe *u)
{
    // Root world's local space is universe space, so the universe->pixel matrix applies directly.
    Matrix3x3 universe_to_pixel_mtx = ResolveWorldToPixelMatrix(&u->root_world, &u->camera);
    DrawNewtonoids(&u->root_world.objects, universe_to_pixel_mtx);
    GeometryEditor_DrawHandles(&u->root_world, &u->camera);

    for (int i = 0; i < u->world_count; i++)
    {
        World2d *w = &u->worlds[i];
        DrawWorldRegion(w, &u->camera);
    }
}

// ---------------------------------------------------------------------------
int Universe_GetWorldCount(const Universe *u)
{
    return u->world_count;
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

// Resolve a stable world root ID to the current world-array address.
World2d *Universe_GetWorldById(Universe *u, EntityId world_id)
{
    if (!u || world_id == INVALID_ENTITY_ID)
    {
        return NULL;
    }

    if (u->root_world.grid_space.object.id == world_id)
    {
        return &u->root_world;
    }

    for (int world_index = 0; world_index < u->world_count; world_index++)
    {
        if (u->worlds[world_index].grid_space.object.id == world_id)
        {
            return &u->worlds[world_index];
        }
    }

    return NULL;
}

// Resolve a stable world root ID to the command-queue index expected by movement APIs.
int Universe_GetWorldIndexById(const Universe *u, EntityId world_id)
{
    if (!u || world_id == INVALID_ENTITY_ID)
    {
        return -1;
    }

    if (u->root_world.grid_space.object.id == world_id)
    {
        return UNIVERSE_ROOT_WORLD_INDEX;
    }

    for (int world_index = 0; world_index < u->world_count; world_index++)
    {
        if (u->worlds[world_index].grid_space.object.id == world_id)
        {
            return world_index;
        }
    }

    return -1;
}

// ---------------------------------------------------------------------------
Camera2d *Universe_GetCamera(Universe *u)
{
    return &u->camera;
}
