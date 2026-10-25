/**********************************************************************************************
*
TARGET RESOLVER MODULE
*
A small, reusable engine-level facility that answers "what is at this point?" and "describe
this thing" for the three pickable kinds in the simulation: an entity, a world, or a cell.
It unifies the existing entity picker (ResolveClosestEntityAt) and world picker
(Universe_FindWorldAt) behind one tagged value (TargetRef) so both UI and non-UI callers can
share a single notion of a resolved target. Payloads are id/index based (not raw pointers)
so a stored TargetRef stays stale-safe across frames.
*
**********************************************************************************************/
#ifndef TARGET_RESOLVER_H
#define TARGET_RESOLVER_H

#include <stddef.h>
#include <stdbool.h>
#include "entities/entity_id.h"
#include "math/cvectors.h"

// Forward declaration only: the resolver API takes `const Universe *` by pointer, so the full
// universe definition is not required here. Including world/universe.h directly would create a
// header cycle (universe.h <-> world.h) for the lightweight consumers that now pull TargetRef
// (e.g. the UI-state selection set); callers that dereference a Universe include universe.h
// (or world.h) themselves.
struct Universe;
typedef struct Universe Universe;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// The kind of thing a TargetRef points at. TARGET_KIND_NONE means "nothing resolved".
typedef enum TargetKind
{
    TARGET_KIND_NONE = 0,
    TARGET_KIND_ENTITY,
    TARGET_KIND_WORLD,
    TARGET_KIND_CELL,
} TargetKind;

// A resolved, stale-safe reference to whatever was picked at a point. The active union member
// is selected by `kind`; id/index payloads are resolved to live pointers only on read.
// CELL references carry their owning world index plus the cell's linear index so the cell can
// be located again without holding a Cell pointer.
typedef struct TargetRef
{
    TargetKind kind;
    union
    {
        EntityId entity;    // TARGET_KIND_ENTITY: resolve via Universe_GetEntityByID.
        int world_index;    // TARGET_KIND_WORLD: nested index (0..n) or UNIVERSE_ROOT_WORLD_INDEX.
        struct
        {
            int world_index; // Owning world index (nested index or UNIVERSE_ROOT_WORLD_INDEX).
            int cell_index;   // Linear cell index within that world's grid space.
        } cell;             // TARGET_KIND_CELL: minimal cell identification.
    } as;
} TargetRef;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Resolve a universe-space point to whatever is under it. Entity-at-point outranks
// world-at-point; a cell is surfaced only when the pick yields one and no entity won.
// Reuses ResolveClosestEntityAt (entity/cell pick) and Universe_FindWorldAt (world pick).
// Returns a TARGET_KIND_NONE ref when nothing is found or `u` is NULL.
TargetRef Target_ResolveAtPoint(const Universe *u, Vector2d universe_point);

// Resolve a universe-space point to the owning world and the dense cell index under it, IGNORING
// presence (so an absent/carved cell still resolves — needed by the cell-paint ADD tool to fill an
// empty slot). Returns true and fills *out_world_index (nested index or UNIVERSE_ROOT_WORLD_INDEX)
// and *out_cell_index (>= 0) when the point lands on a cell slot; returns false otherwise. Reuses
// the same world-pick + frame transform as Target_ResolveAtPoint so the logic lives in one place.
bool Target_ResolveCellIndexAtPoint(const Universe *u, Vector2d universe_point,
                                    int *out_world_index, int *out_cell_index);

// Synthesise human-readable display text for a ref into the caller buffer (always NUL
// terminated when out_bytes > 0). Entity labels are composed as
// "Entity #<id> (<roles>, <components>)" from the entity's role flags and attached
// components; world labels distinguish the root world from a nested index; cell labels
// carry the owning world and cell index. A TARGET_KIND_NONE ref yields a neutral dash.
void Target_Describe(TargetRef ref, char *out, size_t out_bytes);

#endif // TARGET_RESOLVER_H
