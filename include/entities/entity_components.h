#ifndef ENTITY_COMPONENTS_H
#define ENTITY_COMPONENTS_H

#include <stdbool.h>
#include "entities/portal.h"
#include "entities/rotor.h"
#include "entities/gear.h"
#include "entities/health.h"
#include "entities/entity_id.h"

// ============================================================================
// Relation Types
// ============================================================================

// Relation type enumerator for entity-to-entity relationships.
// A relation connects two entities and enables paired behaviour.
typedef enum RelationType
{
    RELATION_NONE = 0,
    RELATION_PORTAL_LINKED = 1,   // Portal ↔ Portal bidirectional teleport link
    RELATION_GEAR_MESHED = 2,     // Gear → Gear meshing constraint
    RELATION_PARENT_CHILD = 3,    // Entity → Parent attachment/hierarchy
    RELATION_OWNER = 4             // Entity → Owner possession
} RelationType;

// Return a human-readable display string for a RelationType.
static inline const char *RelationType_ToString(RelationType type)
{
    switch (type)
    {
    case RELATION_PORTAL_LINKED:
        return "PORTAL_LINKED";
    case RELATION_GEAR_MESHED:
        return "GEAR_MESHED";
    case RELATION_PARENT_CHILD:
        return "PARENT_CHILD";
    case RELATION_OWNER:
        return "OWNER";
    default:
        return "NONE";
    }
}

// Relation component: Represents a connection or relationship between two entities.
// Enables paired behaviour that depends on entity-to-entity state.
typedef struct RelationComponent
{
    RelationType type;             // Type of relationship
    EntityId target_entity;        // The other entity in the relation
    bool is_active;                // Whether the relation is currently active/enabled
} RelationComponent;

// World component: a presence tag marking an EntityId as a WORLD node rather than a plain
// entity. Worlds and plain entities share the EntityId space and both resolve to a
// Newtonoid2d via EntityRegistry_GetEntity (a world returns its GridSpace2d.object body), so
// they cannot be told apart by that call alone. This tag is the unambiguous discriminator for
// ancestry/UI classification. It stores NO world state: the owning World2d is resolved live
// via Universe_GetWorldById(id), keeping a single source of truth.
typedef struct WorldComponent
{
    // Reserved storage keeps the tag representable in the packed component store. The tag's
    // PRESENCE is the signal; no world data is duplicated here.
    unsigned char reserved;
} WorldComponent;

// ============================================================================
// Component Types
// ============================================================================

// Component type enumerator for EntityRegistry lookups.
// Each entity may hold at most one component of any type.
typedef enum EntityComponentType
{
    ENTITY_COMPONENT_NONE = 0,
    ENTITY_COMPONENT_PORTAL = 1,
    ENTITY_COMPONENT_ROTOR = 2,
    ENTITY_COMPONENT_GEAR = 3,
    ENTITY_COMPONENT_RELATION = 4,
    ENTITY_COMPONENT_HEALTH = 5,
    ENTITY_COMPONENT_WORLD = 6    // Presence tag: this EntityId is a world node (see WorldComponent).
} EntityComponentType;

// Highest valid component type, for iteration bounds (e.g. EntityRegistry_Describe).
#define ENTITY_COMPONENT_MAX ENTITY_COMPONENT_WORLD

// Runtime component payload used by command_system to route attachments to registry stores.
typedef struct EntityComponent
{
    EntityComponentType type;
    union
    {
        RotorComponent rotor;
        GearComponent gear;
        PortalComponent portal;
        RelationComponent relation;
        HealthComponent health;
        WorldComponent world;
    } data;
} EntityComponent;

#endif
