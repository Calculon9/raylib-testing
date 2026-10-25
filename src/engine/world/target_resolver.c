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

// Append a "/"-separated token to a running buffer, inserting "/" before the second and later
// tokens. `written` tracks whether anything has been appended yet (for the separator). Bounded
// by out_bytes so the composed list never overruns the caller buffer.
static void Target_AppendToken(char *out, size_t out_bytes, size_t *used, bool *written,
                               const char *token)
{
    if (!out || !token || *used >= out_bytes)
    {
        return;
    }

    if (*written)
    {
        int n = snprintf(out + *used, out_bytes - *used, "/%s", token);
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

// Compose the role portion of an entity label ("WALL", "NEWTONOID", ...) from its role flags
// into out as a "/"-joined list. Writes "-" when no role bits are set (locked label form).
static void Target_ComposeRoles(char *out, size_t out_bytes, EntityRoleFlags roles)
{
    size_t used = 0;
    bool written = false;
    if (roles & ENTITY_ROLE_WALL)       Target_AppendToken(out, out_bytes, &used, &written, "WALL");
    if (roles & ENTITY_ROLE_NEWTONOID)  Target_AppendToken(out, out_bytes, &used, &written, "NEWTONOID");
    if (roles & ENTITY_ROLE_PROJECTILE) Target_AppendToken(out, out_bytes, &used, &written, "PROJECTILE");
    if (roles & ENTITY_ROLE_EFFECT)     Target_AppendToken(out, out_bytes, &used, &written, "EFFECT");
    if (roles & ENTITY_ROLE_CAMERA)     Target_AppendToken(out, out_bytes, &used, &written, "CAMERA");

    if (!written)
    {
        snprintf(out, out_bytes, "-");
    }
}

// Compose the component portion of an entity label ("ROTOR", "GEAR", ...) from the registry
// into out as a "/"-joined list. Only ROTOR/GEAR/PORTAL/HEALTH are surfaced; writes "-" when
// none are present (locked label form).
static void Target_ComposeComponents(char *out, size_t out_bytes, EntityId id)
{
    size_t used = 0;
    bool written = false;
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_ROTOR))  Target_AppendToken(out, out_bytes, &used, &written, "ROTOR");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_GEAR))   Target_AppendToken(out, out_bytes, &used, &written, "GEAR");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_PORTAL)) Target_AppendToken(out, out_bytes, &used, &written, "PORTAL");
    if (EntityRegistry_HasComponent(id, ENTITY_COMPONENT_HEALTH)) Target_AppendToken(out, out_bytes, &used, &written, "HEALTH");

    if (!written)
    {
        snprintf(out, out_bytes, "-");
    }
}

// Build the "Entity #<slot> (<roles>, <components>)" label for an entity ref. The Newtonoid2d
// carries no name string, so the descriptive portion is synthesised purely from role flags and
// attached components; empty lists show "-" (so a bare entity reads "Entity #3 (-, -)"). A
// stale id that no longer resolves to a live object is labelled "Entity #<slot> (gone)".
static void Target_DescribeEntity(TargetRef ref, char *out, size_t out_bytes)
{
    EntityId id = ref.as.entity;
    Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe, id, NULL);

    // Header "Entity #<slot>" using the id's slot index for a stable, human-friendly number.
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

    if (!entity)
    {
        // The id no longer resolves to a live object (e.g. a hover ref held across a despawn).
        // Mark it "(gone)" so the strip degrades gracefully rather than showing a detail-less id.
        snprintf(out + used, out_bytes - used, " (gone)");
        return;
    }

    // Compose the "/"-joined role and component lists ("-" when empty) and append the
    // parenthesised detail in the locked "(<roles>, <components>)" form.
    char roles[96];
    char components[96];
    Target_ComposeRoles(roles, sizeof(roles), entity->roles);
    Target_ComposeComponents(components, sizeof(components), id);

    snprintf(out + used, out_bytes - used, " (%s, %s)", roles, components);
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

    // No entity: surface the cell if the pick landed in a PRESENT one. An absent (carved-out) cell
    // falls through to the WORLD ref below, so clicking empty carved space selects the world behind
    // it. Entity picking ran above and short-circuits on a hit, so an entity standing over an absent
    // cell is still returned as TARGET_KIND_ENTITY and stays fully selectable.
    if (cell_index >= 0 && Cell_IsPresent(&pick_world->grid_space.space, cell_index))
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

// Resolve a universe-space point to the owning world and the dense cell index under it, IGNORING
// presence. Mirrors the first half of Target_ResolveAtPoint (world pick + frame transform), then
// GetIndexFromCoords — no presence/entity test — so a carved (absent) cell is still addressable.
bool Target_ResolveCellIndexAtPoint(const Universe *u, Vector2d universe_point,
                                    int *out_world_index, int *out_cell_index)
{
    if (!u)
    {
        return false;
    }

    Universe *mutable_u = (Universe *)u;

    int world_index = Universe_FindWorldAt(mutable_u, universe_point);

    World2d *pick_world = NULL;
    Vector2d local_point = universe_point;
    int resolved_world_index = UNIVERSE_ROOT_WORLD_INDEX;

    if (world_index >= 0 && world_index < mutable_u->world_count)
    {
        // Nested world: transform the universe point into the world's local frame.
        pick_world = &mutable_u->worlds[world_index];
        local_point = Frame_TransformPoint_FromParent(universe_point, &pick_world->grid_space.space.frame);
        resolved_world_index = world_index;
    }
    else
    {
        // Root world holds objects in universe-space coordinates; the point is already local.
        pick_world = &mutable_u->root_world;
        local_point = universe_point;
        resolved_world_index = UNIVERSE_ROOT_WORLD_INDEX;
    }

    int cell_index = GetIndexFromCoords(&pick_world->grid_space.space, local_point);
    if (cell_index < 0)
    {
        return false; // Point missed the rectangle: no cell slot under it.
    }

    if (out_world_index)
    {
        *out_world_index = resolved_world_index;
    }
    if (out_cell_index)
    {
        *out_cell_index = cell_index;
    }
    return true;
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
            snprintf(out, out_bytes, "World: Root");
        }
        else
        {
            snprintf(out, out_bytes, "World #%d", ref.as.world_index);
        }
        return;

    case TARGET_KIND_CELL:
        // Cells are identified by their owning world plus linear cell index. The owning world is
        // labelled "Root" for the root sentinel, else by its numeric index.
        if (ref.as.cell.world_index == UNIVERSE_ROOT_WORLD_INDEX)
        {
            snprintf(out, out_bytes, "Cell #%d (World Root)", ref.as.cell.cell_index);
        }
        else
        {
            snprintf(out, out_bytes, "Cell #%d (World %d)",
                     ref.as.cell.cell_index, ref.as.cell.world_index);
        }
        return;

    case TARGET_KIND_NONE:
    default:
        // Neutral placeholder so the strip shows a stable label rather than stale text.
        snprintf(out, out_bytes, "None");
        return;
    }
}
