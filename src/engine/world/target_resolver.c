/**********************************************************************************************
*
TARGET RESOLVER MODULE (implementation)
*
Resolves a universe-space point to a tagged TargetRef and synthesises display text for it.
The point picking reuses the engine's existing pickers so there is no parallel hit-test:
  - entities (and the cell they sit in) via ResolveClosestEntityAt (world_internal.h)
  - nested worlds via Universe_FindWorldAt (universe.h)
Root-vs-empty is normalised here: Universe_FindWorldAt returns -1 for "no nested world"
(i.e. the point sits in the universe's own root world / empty space); UNIVERSE_ROOT_WORLD_INDEX
(-2) is the explicit sentinel for that root world. A resolved root/empty point is represented
as TARGET_KIND_WORLD with world_index == UNIVERSE_ROOT_WORLD_INDEX.
*
**********************************************************************************************/
#include "world/target_resolver.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "physics/newtonoid.h"
#include "entities/entity_flags.h"
#include "entities/entity_components.h"
#include "entities/entity_registry.h"
#include "math/affine_space_ops.h"
#include "world/world.h"
#include "world/world_internal.h" // ResolveClosestEntityAt

//----------------------------------------------------------------------------------
// Internal helpers
//----------------------------------------------------------------------------------

// Append a comma-separated token to a running buffer, inserting ", " before the second and
// later tokens. `written` tracks whether anything has been appended yet (for the separator).
// Bounded by out_bytes so the composed label never overruns the caller buffer.
static void Target_AppendToken(char *out, size_t out_bytes, size_t *used, bool *written,
                               const char *token)
{
    if (!out || !token || *used >= out_bytes)
    {
        return;
    }

    if (*written)
    {
        int n = snprintf(out + *used, out_bytes - *used, ", %s", token);
        if (n > 0)
        {
            *used += (size_t)n;
        }
    }
    else
    {
        int n = snprintf(out + *used, out_bytes - *used, "%s", token);
        if (n > 0)
        {
            *used += (size_t)n;
        }
        *written = true;
    }
}

// Compose the role portion of an entity label ("Wall", "Newtonoid", ...) from its role flags.
// Appends each set role as a token; leaves the buffer untouched when no roles are set.
static void Target_AppendRoles(char *out, size_t out_bytes, size_t *used, bool *written,
                               EntityRoleFlags roles)
{
    if (roles & ENTITY_ROLE_WALL)       Target_AppendToken(out, out_bytes, used, written, "Wall");
    if (roles & ENTITY_ROLE_NEWTONOID)  Target_AppendToken(out, out_bytes, used, written, "Newtonoid");
    if (roles & ENTITY_ROLE_PROJECTILE) Target_AppendToken(out, out_bytes, used, written, "Projectile");
    if (roles & ENTITY_ROLE_EFFECT)     Target_AppendToken(out, out_bytes, used, written, "Effect");
    if (roles & ENTITY_ROLE_CAMERA)     Target_AppendToken(out, out_bytes, used, written, "Camera");
}

// Compose the component portion of an entity label ("Rotor", "Gear", ...) from the registry.
// Only the components the strip cares about (ROTOR/GEAR/PORTAL/HEALTH) are surfaced.
static void Target_AppendComponents(char *out, size_t out_bytes, size_t *used, bool *written,
                                    EntityId id)
{
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_ROTOR))  Target_AppendToken(out, out_bytes, used, written, "Rotor");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_GEAR))   Target_AppendToken(out, out_bytes, used, written, "Gear");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_PORTAL)) Target_AppendToken(out, out_bytes, used, written, "Portal");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_HEALTH)) Target_AppendToken(out, out_bytes, used, written, "Health");
}

// Build the "Entity #<id> (<roles>, <components>)" label for an entity ref. The Newtonoid2d
// carries no name string, so the descriptive portion is synthesised purely from role flags
// and attached components. An entity with no roles/components yields "Entity #<id>".
static void Target_DescribeEntity(TargetRef ref, char *out, size_t out_bytes)
{
    EntityId id = ref.as.entity;
    Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe, id, NULL);

    // Header "Entity #<id>" using the id's slot index for a stable, human-friendly number.
    int header = snprintf(out, out_bytes, "Entity #%u", EntityId_GetSlotIndex(id));
    if (header < 0)
    {
        if (out_bytes > 0) out[0] = '\0';
        return;
    }

    size_t used = (size_t)header;
    if (used >= out_bytes)
    {
        return; // Buffer already full; snprintf NUL-terminated.
    }

    // Open the detail parenthesis and append roles then components.
    char detail[128];
    detail[0] = '\0';
    size_t detail_used = 0;
    bool detail_written = false;

    if (entity)
    {
        Target_AppendRoles(detail, sizeof(detail), &detail_used, &detail_written, entity->roles);
    }
    Target_AppendComponents(detail, sizeof(detail), &detail_used, &detail_written, id);

    // Only append the parenthesised detail when something was synthesised.
    if (detail_written)
    {
        snprintf(out + used, out_bytes - used, " (%s)", detail);
    }
}

//----------------------------------------------------------------------------------
// Public API
//----------------------------------------------------------------------------------

// Resolve a universe-space point to whatever is under it (entity > cell > world > none).
TargetRef Target_ResolveAtPoint(const Universe *u, Vector2d universe_point)
{
    TargetRef ref = {0};
    ref.kind = TARGET_KIND_NONE;

    if (!u)
    {
        return ref;
    }

    // Cast away const for the picker APIs, which take non-const World2d* but do not mutate
    // during a pick. The universe is treated as read-only by this resolver.
    Universe *mutable_u = (Universe *)u;

    // Decide the world under the point first. A nested world (index >= 0) is picked in its own
    // local frame; otherwise the point falls in the universe root world (empty space).
    int world_index = Universe_FindWorldAt(mutable_u, universe_point);

    World2d *pick_world = NULL;
    Vector2d local_point = universe_point;
    int resolved_world_index = UNIVERSE_ROOT_WORLD_INDEX;

    if (world_index >= 0 && world_index < mutable_u->world_count)
    {
        // Nested world: transform the universe (parent-space) point into the world's local
        // frame so the entity/cell pick is performed in the correct coordinate space.
        pick_world = &mutable_u->worlds[world_index];
        local_point = Frame_TransformPoint_FromParent(universe_point, &pick_world->grid_space.space.frame);
        resolved_world_index = world_index;
    }
    else
    {
        // No nested world: the root world holds objects in universe-space coordinates, so the
        // universe point is already the root-world-local point (mirrors Universe_HasRootEntityAt).
        pick_world = &mutable_u->root_world;
        local_point = universe_point;
        resolved_world_index = UNIVERSE_ROOT_WORLD_INDEX;
    }

    // Entity-at-point wins. Capture the cell as a fallback for when no entity is hit.
    int cell_index = -1;
    Newtonoid2d *entity = ResolveClosestEntityAt(pick_world, local_point, NULL, &cell_index, NULL, 0, NULL);
    if (entity)
    {
        ref.kind = TARGET_KIND_ENTITY;
        ref.as.entity = entity->id;
        return ref;
    }

    // No entity: surface the cell if the pick landed in one.
    if (cell_index >= 0)
    {
        ref.kind = TARGET_KIND_CELL;
        ref.as.cell.world_index = resolved_world_index;
        ref.as.cell.cell_index = cell_index;
        return ref;
    }

    // Otherwise the point resolves to a world (nested or root/empty).
    ref.kind = TARGET_KIND_WORLD;
    ref.as.world_index = resolved_world_index;
    return ref;
}

// Synthesise display text for a ref. Always NUL-terminates the buffer when out_bytes > 0.
void Target_Describe(TargetRef ref, char *out, size_t out_bytes)
{
    if (!out || out_bytes == 0)
    {
        return;
    }

    switch (ref.kind)
    {
    case TARGET_KIND_ENTITY:
        Target_DescribeEntity(ref, out, out_bytes);
        return;

    case TARGET_KIND_WORLD:
        // Distinguish the universe root world from a nested world index.
        if (ref.as.world_index == UNIVERSE_ROOT_WORLD_INDEX)
        {
            snprintf(out, out_bytes, "World (root)");
        }
        else
        {
            snprintf(out, out_bytes, "World #%d", ref.as.world_index);
        }
        return;

    case TARGET_KIND_CELL:
        // Cells are identified by their owning world plus linear cell index.
        if (ref.as.cell.world_index == UNIVERSE_ROOT_WORLD_INDEX)
        {
            snprintf(out, out_bytes, "Cell #%d (World root)", ref.as.cell.cell_index);
        }
        else
        {
            snprintf(out, out_bytes, "Cell #%d (World #%d)",
                     ref.as.cell.cell_index, ref.as.cell.world_index);
        }
        return;

    case TARGET_KIND_NONE:
    default:
        // Neutral placeholder so the strip shows a stable dash rather than stale text.
        snprintf(out, out_bytes, "-");
        return;
    }
}
