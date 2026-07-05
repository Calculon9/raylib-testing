/**********************************************************************************************
*
UNIVERSE MODULE
*
**********************************************************************************************/
#include "world/universe.h"
#include "world/world.h"
#include "system/systems.h"
#include "math/cvectors.h"
#include "common/common.h"
#include "camera/camera.h"

Universe G_Universe = {0};

// ---------------------------------------------------------------------------
void Universe_Init(Universe *u,
                   Vector2d default_spawn,
                   Vector2d default_world_resolution,
                   Vector2d universe_resolution,
                   float    default_gravity)
{
    u->world_count = 0;
    u->selected_world_index = -1;

    for (int i = 0; i < UNIVERSE_MAX_WORLDS; i++)
        u->camera_marker_ids[i] = 0;

    u->camera_offset = ZERO_VECTOR_2D;
    u->resolution = universe_resolution;

    // Initialize universe camera: identity basis, origin at (0,0), centered view
    Basis2d identity_basis = (Basis2d){{1, 0}, {0, 1}};
    u->camera = CreateCamera2d(identity_basis, identity_basis,
                               ZERO_VECTOR_2D, ZERO_VECTOR_2D,
                               1.0f, 0.0f);
    u->camera.camera_coords = (Vector2d){universe_resolution.x * 0.5f, universe_resolution.y * 0.5f};
    UpdateCameraTransforms(&u->camera);

    u->next_spawn = default_spawn;
    u->next_resolution = default_world_resolution;
    u->next_gravity = default_gravity;

    // Default step: 15 % of the world dimension so successive worlds are offset but visible.
    u->spawn_step.x = (default_world_resolution.x > 0.0f) ? default_world_resolution.x * 0.15f : 5.0f;
    u->spawn_step.y = (default_world_resolution.y > 0.0f) ? default_world_resolution.y * 0.15f : 5.0f;
}

// ---------------------------------------------------------------------------
int Universe_CreateWorld(Universe *u, Basis2d world_basis, ColourRgba fill_colour, ColourRgba line_colour, Camera2d *world_camera, ColourRgba camera_marker_colour, WorldState *world_state)
{
    if (u->world_count >= UNIVERSE_MAX_WORLDS)
    {
        LOG_WARN("Cannot create world: reached max world count (%d).\n", UNIVERSE_MAX_WORLDS);
        return -1;
    }

    Vector2d creation_res = (u->next_resolution.x > 0.0f && u->next_resolution.y > 0.0f)
                                ? u->next_resolution
                                : (Vector2d){16.0f, 9.0f}; // sensible fallback

    CoordSpace2d_Grid space_g = NewCoordSpace2d_Grid(ZERO_VECTOR_2D, creation_res, world_basis, fill_colour, line_colour);
    space_g.object.id = 0;
    space_g.object.flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
    space_g.object.collision_mask = FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE | FLAG_TYPE_WALL;
    space_g.object.entity_layer = FLAG_TYPE_WALL;

    int new_index = u->world_count;
    World2d *new_world = &u->worlds[new_index];

    CreateWorld(space_g, u->next_gravity, new_world);
    new_world->universe_position = u->next_spawn;

    // Place a camera-position marker inside the world.
    Newtonoid2d marker = CreateNewtonoid2d_Symmetric(4, 0.45f, camera_marker_colour, 1.0f,
                                                     world_camera->camera_coords,
                                                     ZERO_VECTOR_2D, ZERO_VECTOR_2D);
    marker.entity_layer = FLAG_TYPE_EFFECT;
    marker.collision_mask = 0;
    marker.flags = FLAG_STATUS_ALIVE;
    marker.line_colour = camera_marker_colour;
    marker.fill_colour = camera_marker_colour;
    u->camera_marker_ids[new_index] = AddObjectToWorld(new_world, &marker,
                                                       new_world->coord_space_grid.object.id);

    u->world_count++;

    // Advance the default spawn so the next world is offset by default.
    u->next_spawn = VectorSum_2d(u->next_spawn, u->spawn_step);

    // DO NOT auto-select the world; let caller decide.
    // World creation is separate from world selection, allowing unselected states.

    // Wire WorldState to the new world.
    if (world_state)
    {
        world_state->world = new_world;
        world_state->entity_world_index_registry = &new_world->entity_world_index_registry;
        world_state->collisions = &new_world->collisions;
        world_state->selected_object = NULL;
        world_state->selected_cell = NULL;
    }

    LOG_INFO("Universe_CreateWorld -> index=%d universe_pos=(%.1f,%.1f) res=(%.0fx%.0f) gravity=%.2f\n",
             new_index,
             new_world->universe_position.x, new_world->universe_position.y,
             creation_res.x, creation_res.y,
             u->next_gravity);

    return new_index;
}

// ---------------------------------------------------------------------------
bool Universe_SelectWorld(Universe *u, int index, Vector2d world_origin)
{
    if (index < 0 || index >= u->world_count)
        return false;

    u->selected_world_index = index;

    // Pan the universe camera so the selected world aligns to world_origin.
    u->camera_offset = VectorSum_2d(u->worlds[index].universe_position,
                                    VectorScale_2d(world_origin, -1.0f));
    
    // Also update the universe camera position to center on the selected world.
    u->camera.camera_coords = u->worlds[index].universe_position;
    UpdateCameraTransforms(&u->camera);
    
    return true;
}

// ---------------------------------------------------------------------------
void Universe_Draw(Universe *u, Camera2d *world_camera)
{
    // Choose camera: if a world is selected, use its camera; otherwise use universe camera.
    Camera2d *active_camera = (u->selected_world_index >= 0) ? world_camera : &u->camera;

    for (int i = 0; i < u->world_count; i++)
    {
        World2d *w = &u->worlds[i];
        Vector2d effective_pos = VectorSum_2d(w->universe_position,
                                              VectorScale_2d(u->camera_offset, -1.0f));
        DrawWorldRegion(w, active_camera, effective_pos);
    }
}

// ---------------------------------------------------------------------------
bool Universe_ResolveClick(Universe *u, Vector2d universe_click, Vector2d world_origin, Vector2d *local_out)
{
    for (int wi = 0; wi < u->world_count; wi++)
    {
        World2d *w = &u->worlds[wi];
        Vector2d local = VectorSum_2d(universe_click,
                                      VectorScale_2d(w->universe_position, -1.0f));
        Vector2d res = w->coord_space_grid.coord_space.resolution_ixj;
        if (local.x >= 0 && local.y >= 0 && local.x < res.x && local.y < res.y)
        {
            bool changed = (wi != u->selected_world_index);
            if (changed)
                Universe_SelectWorld(u, wi, world_origin);
            if (local_out)
                *local_out = local;
            return changed;
        }
    }
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

void Universe_ZoomCamera(Universe *u, float factor)
{
    ZoomCamera(&u->camera, factor);
}

void Universe_PanCamera(Universe *u, Vector2d delta)
{
    u->camera.camera_coords = VectorSum_2d(u->camera.camera_coords, delta);
    UpdateCameraTransforms(&u->camera);
}

void Universe_RotateCamera(Universe *u, float angle_delta)
{
    RotateCamera(&u->camera, angle_delta);
}
