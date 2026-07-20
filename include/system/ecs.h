#ifndef ECS_H
#define ECS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Lightweight Entity-Component-System for organizing runtime entities and their behaviors.
// Supports entity creation/destruction, component attachment/detachment, and system iteration.

typedef uint32_t Entity;      // Opaque entity handle
typedef uint32_t ComponentID; // Component type identifier

// Component IDs (user-defined; reserve 0 as "invalid")
#define COMPONENT_INVALID 0
#define COMPONENT_PHYSICS 1
#define COMPONENT_RENDER 2
#define COMPONENT_LIFETIME 3
#define COMPONENT_DAMAGE 4

typedef struct ECS ECS;

// Forward-declare component data types (actual structs defined elsewhere or by user)
typedef struct PhysicsComponent PhysicsComponent;
typedef struct RenderComponent RenderComponent;
typedef struct LifetimeComponent LifetimeComponent;
typedef struct DamageComponent DamageComponent;

// Callback for iterating entities with specific components
// ctx is user-provided context, e is the entity ID, components is an array of void* pointers to that entity's components
typedef void (*SystemIteratorFn)(void *ctx, Entity e, void **components);

// Create a new ECS context
ECS *ECS_Create(void);

// Destroy ECS context and all entities/components
void ECS_Destroy(ECS *ecs);

// Create a new entity; returns a unique entity ID
Entity ECS_CreateEntity(ECS *ecs);

// Destroy an entity and all its components
void ECS_DestroyEntity(ECS *ecs, Entity e);

// Check if entity is alive
bool ECS_IsEntityAlive(ECS *ecs, Entity e);

// Attach a component to an entity; component_data is copied into internal storage
void ECS_AddComponent(ECS *ecs, Entity e, ComponentID comp_id,
                      const void *component_data, size_t component_size);

// Remove a component from an entity
void ECS_RemoveComponent(ECS *ecs, Entity e, ComponentID comp_id);

// Query a component from an entity
void *ECS_GetComponent(ECS *ecs, Entity e, ComponentID comp_id);

// Iterate over all entities that have a specific set of components (mask bitmask of ComponentID).
// For each entity, invoke fn with user context and an array of component pointers.
// component_ids is an array of ComponentID to query (e.g., {COMPONENT_PHYSICS, COMPONENT_RENDER})
// component_count is the size of component_ids array
void ECS_IterateEntities(ECS *ecs, const ComponentID *component_ids, int component_count,
                         SystemIteratorFn fn, void *user_ctx);

// Convenience: get entity count
int ECS_GetEntityCount(ECS *ecs);

#endif // ECS_H

