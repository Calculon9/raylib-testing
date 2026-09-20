#ifndef ENTITY_COMPONENTS_H
#define ENTITY_COMPONENTS_H

#include <stdbool.h>
#include "entities/portal.h"
#include "entities/rotor.h"
#include "entities/gear.h"
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
    ENTITY_COMPONENT_RELATION = 4
} EntityComponentType;

// Generic type with tagged union for entity creation results. Used by EntityFactory to return components
// and by command_queue to route components to appropriate registry stores.
typedef struct EntityComponent
{
    EntityComponentType type;
    union
    {
        RotorComponent rotor;
        GearComponent gear;
        PortalEntity portal;
        RelationComponent relation;
    } data;
} EntityComponent;

#endif
