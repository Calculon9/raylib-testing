/**********************************************************************************************
 *
 * ENTITY REGISTRY MODULE
 *
 * Provides centralised EntityId allocation, location tracking, and component attachment
 * storage using the generic SlotMap collection. Both entity locations and optional gameplay
 * components are stored in dense packed stores with sparse EntityId lookup tables.
 
 ENTITIES ARRAY (Sparse Handles)
  Entity 3 (ID: 3, Gen: 1) --------+
  Entity 8 (ID: 8, Gen: 1) -----+  |
                                |  |
 ROTOR COMPONENT STORE           |  |
  Dense Data Array:             v  v
    data[0] = Rotor for Entity 8  (Index 0)
    data[1] = Rotor for Entity 3  (Index 1)  <-- Packed contiguously!
 **********************************************************************************************/

#include "entities/entity_registry.h"
#include "common/common.h"
#include "world/world.h"
#include "world/universe.h"

// Stored record for an entity slot: world location tracking only.
// Component storage is separated into dedicated type-specific SlotMaps.
typedef struct EntitySlotRecord
{
    EntityId world_id;          // Owning world entity ID, or INVALID_ENTITY_ID if none.
    int array_index;            // -1 for world root, >= 0 for index in world->objects.
    bool has_location;          // Flag indicating whether location data is currently bound.
} EntitySlotRecord;

// Dense component storage with a sparse EntityId -> dense index lookup table.
typedef struct ComponentStore
{
    void *dense_items;
    EntityId *dense_entities;
    int *sparse_indices;
    uint32_t *sparse_generations;
    int count;
    int dense_capacity;
    int sparse_capacity;
    size_t elem_bytes;
} ComponentStore;

// Central registry holding generational ID records and packed component stores.
typedef struct EntityRegistry
{
    SlotMap id_slots;           // Allocates/validates EntityIds (stores EntitySlotRecord).
    ComponentStore rotor_components;
    ComponentStore gear_components;
    ComponentStore portal_components;
    ComponentStore relation_components;
    bool is_initialised;        // Initialisation state flag.
} EntityRegistry;

static EntityRegistry registry = {0};

// Calculate the next packed-store capacity without depending on collection internals.
static int ComponentStore_NextCapacity(int capacity)
{
    int next_capacity = capacity > 0 ? (int)((float)capacity * 1.4f) + 1 : 4;
    return next_capacity > capacity ? next_capacity : capacity + 1;
}

// Initialise a dense component store and its sparse lookup table.
static bool ComponentStore_Init(ComponentStore *store, size_t elem_bytes, int initial_capacity)
{
    if (!store || elem_bytes == 0 || initial_capacity <= 0)
    {
        return false;
    }

    *store = (ComponentStore){0};
    store->elem_bytes = elem_bytes;
    store->dense_capacity = initial_capacity;
    store->sparse_capacity = initial_capacity;
    store->dense_items = AllocateBytes((size_t)initial_capacity * elem_bytes);
    store->dense_entities = AllocateBytes((size_t)initial_capacity * sizeof(EntityId));
    store->sparse_indices = AllocateBytes((size_t)initial_capacity * sizeof(int));
    store->sparse_generations = AllocateBytes((size_t)initial_capacity * sizeof(uint32_t));
    if (!store->dense_items || !store->dense_entities ||
        !store->sparse_indices || !store->sparse_generations)
    {
        if (store->dense_items) Deallocate(&store->dense_items, (size_t)initial_capacity * elem_bytes);
        if (store->dense_entities) Deallocate((void **)&store->dense_entities, (size_t)initial_capacity * sizeof(EntityId));
        if (store->sparse_indices) Deallocate((void **)&store->sparse_indices, (size_t)initial_capacity * sizeof(int));
        if (store->sparse_generations) Deallocate((void **)&store->sparse_generations, (size_t)initial_capacity * sizeof(uint32_t));
        *store = (ComponentStore){0};
        return false;
    }

    for (int i = 0; i < initial_capacity; i++)
    {
        store->sparse_indices[i] = -1;
        store->sparse_generations[i] = 0;
    }
    return true;
}

// Release all dense and sparse buffers owned by a component store.
static void ComponentStore_Clear(ComponentStore *store)
{
    if (!store)
    {
        return;
    }

    if (store->dense_items) Deallocate(&store->dense_items, (size_t)store->dense_capacity * store->elem_bytes);
    if (store->dense_entities) Deallocate((void **)&store->dense_entities, (size_t)store->dense_capacity * sizeof(EntityId));
    if (store->sparse_indices) Deallocate((void **)&store->sparse_indices, (size_t)store->sparse_capacity * sizeof(int));
    if (store->sparse_generations) Deallocate((void **)&store->sparse_generations, (size_t)store->sparse_capacity * sizeof(uint32_t));
    *store = (ComponentStore){0};
}

// Grow the sparse lookup table to cover an EntityId slot index.
static bool ComponentStore_GrowSparse(ComponentStore *store, uint32_t slot_index)
{
    if (slot_index < (uint32_t)store->sparse_capacity)
    {
        return true;
    }

    int new_capacity = store->sparse_capacity;
    while (slot_index >= (uint32_t)new_capacity)
    {
        new_capacity = ComponentStore_NextCapacity(new_capacity);
    }

    int *new_indices = AllocateBytes((size_t)new_capacity * sizeof(int));
    uint32_t *new_generations = AllocateBytes((size_t)new_capacity * sizeof(uint32_t));
    if (!new_indices || !new_generations)
    {
        if (new_indices) Deallocate((void **)&new_indices, (size_t)new_capacity * sizeof(int));
        if (new_generations) Deallocate((void **)&new_generations, (size_t)new_capacity * sizeof(uint32_t));
        return false;
    }

    MemoryCopy(new_indices, store->sparse_indices, (size_t)store->sparse_capacity * sizeof(int));
    MemoryCopy(new_generations, store->sparse_generations, (size_t)store->sparse_capacity * sizeof(uint32_t));
    for (int i = store->sparse_capacity; i < new_capacity; i++)
    {
        new_indices[i] = -1;
        new_generations[i] = 0;
    }
    Deallocate((void **)&store->sparse_indices, (size_t)store->sparse_capacity * sizeof(int));
    Deallocate((void **)&store->sparse_generations, (size_t)store->sparse_capacity * sizeof(uint32_t));
    store->sparse_indices = new_indices;
    store->sparse_generations = new_generations;
    store->sparse_capacity = new_capacity;
    return true;
}

// Grow dense component payload and owner arrays to contain one more item.
static bool ComponentStore_GrowDense(ComponentStore *store)
{
    if (store->count < store->dense_capacity)
    {
        return true;
    }

    int new_capacity = ComponentStore_NextCapacity(store->dense_capacity);
    void *new_items = AllocateBytes((size_t)new_capacity * store->elem_bytes);
    EntityId *new_entities = AllocateBytes((size_t)new_capacity * sizeof(EntityId));
    if (!new_items || !new_entities)
    {
        if (new_items) Deallocate(&new_items, (size_t)new_capacity * store->elem_bytes);
        if (new_entities) Deallocate((void **)&new_entities, (size_t)new_capacity * sizeof(EntityId));
        return false;
    }

    MemoryCopy(new_items, store->dense_items, (size_t)store->count * store->elem_bytes);
    MemoryCopy(new_entities, store->dense_entities, (size_t)store->count * sizeof(EntityId));
    Deallocate(&store->dense_items, (size_t)store->dense_capacity * store->elem_bytes);
    Deallocate((void **)&store->dense_entities, (size_t)store->dense_capacity * sizeof(EntityId));
    store->dense_items = new_items;
    store->dense_entities = new_entities;
    store->dense_capacity = new_capacity;
    return true;
}

// Return the dense index for an EntityId, or -1 when the component is absent/stale.
static int ComponentStore_Find(const ComponentStore *store, EntityId entity_id)
{
    if (!store || entity_id == INVALID_ENTITY_ID)
    {
        return -1;
    }

    uint32_t slot_index = EntityId_GetSlotIndex(entity_id);
    uint32_t generation = EntityId_GetGeneration(entity_id);
    if (slot_index >= (uint32_t)store->sparse_capacity)
    {
        return -1;
    }

    int dense_index = store->sparse_indices[slot_index];
    if (dense_index < 0 || dense_index >= store->count ||
        store->sparse_generations[slot_index] != generation ||
        store->dense_entities[dense_index] != entity_id)
    {
        return -1;
    }
    return dense_index;
}

// Attach a component to the dense store in O(1).
static bool ComponentStore_Attach(ComponentStore *store, EntityId entity_id, const void *component)
{
    if (!store || !component || entity_id == INVALID_ENTITY_ID ||
        !ComponentStore_GrowSparse(store, EntityId_GetSlotIndex(entity_id)) ||
        ComponentStore_Find(store, entity_id) >= 0 || !ComponentStore_GrowDense(store))
    {
        return false;
    }

    int dense_index = store->count++;
    MemoryCopy((char *)store->dense_items + ((size_t)dense_index * store->elem_bytes), component, store->elem_bytes);
    store->dense_entities[dense_index] = entity_id;
    store->sparse_indices[EntityId_GetSlotIndex(entity_id)] = dense_index;
    store->sparse_generations[EntityId_GetSlotIndex(entity_id)] = EntityId_GetGeneration(entity_id);
    return true;
}

// Retrieve a component from the dense store in O(1).
static void *ComponentStore_Get(const ComponentStore *store, EntityId entity_id)
{
    int dense_index = ComponentStore_Find(store, entity_id);
    return dense_index >= 0
               ? (char *)store->dense_items + ((size_t)dense_index * store->elem_bytes)
               : NULL;
}

// Remove a component with swap-pop in O(1).
static bool ComponentStore_Remove(ComponentStore *store, EntityId entity_id)
{
    int dense_index = ComponentStore_Find(store, entity_id);
    if (dense_index < 0)
    {
        return false;
    }

    int last_index = store->count - 1;
    uint32_t slot_index = EntityId_GetSlotIndex(entity_id);
    if (dense_index != last_index)
    {
        EntityId moved_entity = store->dense_entities[last_index];
        MemoryCopy((char *)store->dense_items + ((size_t)dense_index * store->elem_bytes),
                   (char *)store->dense_items + ((size_t)last_index * store->elem_bytes),
                   store->elem_bytes);
        store->dense_entities[dense_index] = moved_entity;
        store->sparse_indices[EntityId_GetSlotIndex(moved_entity)] = dense_index;
    }

    MemorySet((char *)store->dense_items + ((size_t)last_index * store->elem_bytes), 0, store->elem_bytes);
    store->dense_entities[last_index] = INVALID_ENTITY_ID;
    store->sparse_indices[slot_index] = -1;
    store->sparse_generations[slot_index] = 0;
    store->count--;
    return true;
}

// Initialise the entity registry and backing slot storage.
void EntityRegistry_Init(void)
{
    if (registry.is_initialised)
    {
        EntityRegistry_Shutdown();
    }

    // Allocate entity ID slot storage with initial capacity of 8 slots.
    registry.id_slots = MakeSlotMap(8, sizeof(EntitySlotRecord));

    // Allocate compact component stores; storage grows with attached component count.
    bool component_stores_ready =
        ComponentStore_Init(&registry.rotor_components, sizeof(RotorComponent), 8) &&
        ComponentStore_Init(&registry.gear_components, sizeof(GearComponent), 8) &&
        ComponentStore_Init(&registry.portal_components, sizeof(PortalEntity), 8) &&
        ComponentStore_Init(&registry.relation_components, sizeof(RelationComponent), 8);

    if (!component_stores_ready)
    {
        LOG_ERROR("Entity registry component-store initialization failed.\n");
        ComponentStore_Clear(&registry.rotor_components);
        ComponentStore_Clear(&registry.gear_components);
        ComponentStore_Clear(&registry.portal_components);
        ComponentStore_Clear(&registry.relation_components);
        ClearSlotMap(&registry.id_slots);
        return;
    }

    registry.is_initialised = true;
}

// Release all resources held by the registry and reset internal state.
void EntityRegistry_Shutdown(void)
{
    if (!registry.is_initialised)
    {
        return;
    }

    ClearSlotMap(&registry.id_slots);
    ComponentStore_Clear(&registry.rotor_components);
    ComponentStore_Clear(&registry.gear_components);
    ComponentStore_Clear(&registry.portal_components);
    ComponentStore_Clear(&registry.relation_components);
    registry.is_initialised = false;
}

// Allocate a slot-based ID whose generation detects stale references.
EntityId EntityRegistry_AllocateId(void)
{
    if (!registry.is_initialised)
    {
        EntityRegistry_Init();
    }

    // Insert an empty location record with no world attachment.
    EntitySlotRecord empty_record = {
        .world_id = INVALID_ENTITY_ID,
        .array_index = -1,
        .has_location = false
    };
    SlotHandle handle = SlotMap_Insert(&registry.id_slots, &empty_record);
    return (EntityId)handle;
}

// Release an ID, clear its attached state, and advance generation to invalidate live handles.
void EntityRegistry_ReleaseId(EntityId entity_id)
{
    if (!registry.is_initialised || entity_id == INVALID_ENTITY_ID)
    {
        return;
    }

    // Also remove any attached components before invalidating the ID.
    for (int type = 1; type <= ENTITY_COMPONENT_RELATION; type++)
    {
        EntityRegistry_RemoveComponent(entity_id, (EntityComponentType)type);
    }

    // Removing the slot from the slot map recycles it and advances its generation automatically.
    SlotMap_Remove(&registry.id_slots, (SlotHandle)entity_id);
}

// Return whether an ID currently refers to an active, valid slot.
bool EntityRegistry_IsIdActive(EntityId entity_id)
{
    if (!registry.is_initialised || entity_id == INVALID_ENTITY_ID)
    {
        return false;
    }

    return SlotMap_Contains(&registry.id_slots, (SlotHandle)entity_id);
}

// Inspect the next slot-based ID that would be allocated without advancing state.
EntityId EntityRegistry_GetNextId(void)
{
    if (!registry.is_initialised)
    {
        EntityRegistry_Init();
    }

    return (EntityId)SlotMap_GetNextHandle(&registry.id_slots);
}

// Record or update the world storage location for an active entity.
bool EntityRegistry_SetLocation(EntityId entity_id, EntityId world_id, int array_index)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.id_slots, (SlotHandle)entity_id);
    if (!record || world_id == INVALID_ENTITY_ID || array_index < -1)
    {
        return false;
    }

    record->world_id = world_id;
    record->array_index = array_index;
    record->has_location = true;
    return true;
}

// Clear an entity location while preserving the ID and attached components (e.g. across a world transfer).
bool EntityRegistry_ClearLocation(EntityId entity_id)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.id_slots, (SlotHandle)entity_id);
    if (!record)
    {
        return false;
    }

    record->world_id = INVALID_ENTITY_ID;
    record->array_index = -1;
    record->has_location = false;
    return true;
}

// Read the storage location associated with an active ID.
bool EntityRegistry_GetLocation(EntityId entity_id, EntityId *out_world_id, int *out_array_index)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.id_slots, (SlotHandle)entity_id);
    if (!record || !record->has_location)
    {
        return false;
    }

    if (out_world_id)
    {
        *out_world_id = record->world_id;
    }
    if (out_array_index)
    {
        *out_array_index = record->array_index;
    }
    return true;
}

// Resolve the world ID currently owning an active entity ID.
EntityId EntityRegistry_GetEntityWorldId(EntityId entity_id)
{
    EntityId world_id = INVALID_ENTITY_ID;
    return EntityRegistry_GetLocation(entity_id, &world_id, NULL) ? world_id : INVALID_ENTITY_ID;
}

// Resolve an active ID to its current Newtonoid storage pointer.
Newtonoid2d *EntityRegistry_GetEntity(EntityId entity_id)
{
    EntityId world_id = INVALID_ENTITY_ID;
    int array_index = -1;
    if (!EntityRegistry_GetLocation(entity_id, &world_id, &array_index))
    {
        return NULL;
    }

    World2d *world = Universe_GetWorldById(&G_Universe, world_id);
    if (!world)
    {
        return NULL;
    }

    // An array index of -1 designates the root world bounding body.
    if (array_index < 0)
    {
        return &world->grid_space.object;
    }

    return (Newtonoid2d *)LArray_Get(&world->objects, array_index);
}

// Resolve the world currently owning an active ID.
World2d *EntityRegistry_GetEntityWorld(EntityId entity_id)
{
    EntityId world_id = EntityRegistry_GetEntityWorldId(entity_id);
    return Universe_GetWorldById(&G_Universe, world_id);
}

// ============================================================================
// Entity Description
// ============================================================================

// Capture the current component state of an entity into a description struct.
EntityDescription EntityRegistry_Describe(const Newtonoid2d *object)
{
    EntityDescription desc = {0};
    if (!object || object->id == INVALID_ENTITY_ID)
    {
        return desc;
    }

    desc.id = object->id;
    for (int type = 1; type <= ENTITY_COMPONENT_RELATION; type++)
    {
        desc.components[type] = EntityRegistry_GetComponent(object->id, (EntityComponentType)type);
    }

    return desc;
}

// ============================================================================
// Generic Component Dispatchers
// ============================================================================

// Attach component data of the specified EntityComponentType to an EntityId.
bool EntityRegistry_AttachComponent(EntityId entity_id, EntityComponentType type, const void *component_data)
{
    if (!component_data)
    {
        return false;
    }

    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        return EntityRegistry_AttachPortal(entity_id, (const PortalEntity *)component_data);
    case ENTITY_COMPONENT_ROTOR:
        return EntityRegistry_AttachRotor(entity_id, (const RotorComponent *)component_data);
    case ENTITY_COMPONENT_GEAR:
        return EntityRegistry_AttachGear(entity_id, (const GearComponent *)component_data);
    case ENTITY_COMPONENT_RELATION:
        return EntityRegistry_AttachRelation(entity_id, (const RelationComponent *)component_data);
    case ENTITY_COMPONENT_NONE:
    default:
        return false;
    }
}

// Retrieve a pointer to the component data attached to an EntityId, or NULL if not attached.
void *EntityRegistry_GetComponent(EntityId entity_id, EntityComponentType type)
{
    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        return (void *)EntityRegistry_GetPortal(entity_id);
    case ENTITY_COMPONENT_ROTOR:
        return (void *)EntityRegistry_GetRotor(entity_id);
    case ENTITY_COMPONENT_GEAR:
        return (void *)EntityRegistry_GetGear(entity_id);
    case ENTITY_COMPONENT_RELATION:
        return (void *)EntityRegistry_GetRelation(entity_id);
    case ENTITY_COMPONENT_NONE:
    default:
        return NULL;
    }
}

// Remove the component of the specified EntityComponentType from an EntityId.
bool EntityRegistry_RemoveComponent(EntityId entity_id, EntityComponentType type)
{
    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        return EntityRegistry_RemovePortal(entity_id);
    case ENTITY_COMPONENT_ROTOR:
        return EntityRegistry_RemoveRotor(entity_id);
    case ENTITY_COMPONENT_GEAR:
        return EntityRegistry_RemoveGear(entity_id);
    case ENTITY_COMPONENT_RELATION:
        return EntityRegistry_RemoveRelation(entity_id);
    case ENTITY_COMPONENT_NONE:
    default:
        return false;
    }
}

// Check whether an EntityId currently has a component of the specified EntityComponentType attached.
bool EntityRegistry_HasComponent(EntityId entity_id, EntityComponentType type)
{
    return EntityRegistry_GetComponent(entity_id, type) != NULL;
}

// ============================================================================
// Component Lifecycle Hooks
// ============================================================================

// Synchronise an entity's physical state (flags, velocity, sensor rules) when a component is attached.
void EntityLifecycle_ApplyAttachedComponent(Newtonoid2d *entity, EntityComponentType type, const void *component_data)
{
    (void)component_data;
    if (!entity)
    {
        return;
    }

    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        // Configure portal sensor properties and position lock.
        entity->attribute_flags |= (ENTITY_ATTR_FLAG_SENSOR |
                                    ENTITY_ATTR_FLAG_POSITION_LOCKED |
                                    ENTITY_ATTR_FLAG_NO_CONTACT_RESPONSE);
        entity->attribute_flags &= ~ENTITY_ATTR_FLAG_DAMAGEABLE;
        entity->collision_mask = ENTITY_FLAG_NONE;
        break;

    case ENTITY_COMPONENT_ROTOR:
        // Configure rotor rotation and position lock.
        entity->attribute_flags |= ENTITY_ATTR_FLAG_POSITION_LOCKED;
        if (entity->angular_velocity == 0.0f)
        {
            entity->angular_velocity = 2.0f;
        }
        break;

    case ENTITY_COMPONENT_GEAR:
        // Configure gear rotation and position lock.
        entity->attribute_flags |= ENTITY_ATTR_FLAG_POSITION_LOCKED;
        if (entity->angular_velocity == 0.0f)
        {
            entity->angular_velocity = 2.0f;
        }
        break;

    case ENTITY_COMPONENT_RELATION:
    case ENTITY_COMPONENT_NONE:
    default:
        break;
    }
}

// Synchronise an entity's physical state (reverting flags, velocity, sensor rules) when a component is detached.
void EntityLifecycle_ApplyDetachedComponent(Newtonoid2d *entity, EntityComponentType type)
{
    if (!entity)
    {
        return;
    }

    switch (type)
    {
    case ENTITY_COMPONENT_PORTAL:
        // Revert sensor and no-contact flags, restoring damageable capability.
        entity->attribute_flags &= ~(ENTITY_ATTR_FLAG_SENSOR | ENTITY_ATTR_FLAG_NO_CONTACT_RESPONSE);
        entity->attribute_flags |= ENTITY_ATTR_FLAG_DAMAGEABLE;

        // Restore standard collision mask if it was cleared by the portal.
        if (entity->collision_mask == ENTITY_FLAG_NONE)
        {
            entity->collision_mask = ENTITY_FLAG_NEWTONOID | ENTITY_FLAG_PROJECTILE | ENTITY_FLAG_WALL;
        }

        // Release position lock only if neither rotor nor gear is still attached.
        if (!EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_ROTOR) &&
            !EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_GEAR))
        {
            entity->attribute_flags &= ~ENTITY_ATTR_FLAG_POSITION_LOCKED;
        }
        break;

    case ENTITY_COMPONENT_ROTOR:
        // Stop active rotation and release position lock if unconstrained.
        entity->angular_velocity = 0.0f;
        if (!EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_PORTAL) &&
            !EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_GEAR))
        {
            entity->attribute_flags &= ~ENTITY_ATTR_FLAG_POSITION_LOCKED;
        }
        break;

    case ENTITY_COMPONENT_GEAR:
        // Stop active rotation and release position lock if unconstrained.
        entity->angular_velocity = 0.0f;
        if (!EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_PORTAL) &&
            !EntityRegistry_HasComponent(entity->id, ENTITY_COMPONENT_ROTOR))
        {
            entity->attribute_flags &= ~ENTITY_ATTR_FLAG_POSITION_LOCKED;
        }
        break;

    case ENTITY_COMPONENT_RELATION:
    case ENTITY_COMPONENT_NONE:
    default:
        break;
    }
}

// ============================================================================
// Component Accessors - Rotor Component
// ============================================================================

// Attach a RotorComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachRotor(EntityId entity_id, const RotorComponent *rotor)
{
    if (!registry.is_initialised || !rotor || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Prevent duplicate component attachment.
    if (!ComponentStore_Attach(&registry.rotor_components, entity_id, rotor))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_ROTOR, rotor);
    }

    return true;
}

// Retrieve the RotorComponent attached to an EntityId, or NULL if not attached.
RotorComponent *EntityRegistry_GetRotor(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return NULL;
    }

    return (RotorComponent *)ComponentStore_Get(&registry.rotor_components, entity_id);
}

// Remove the RotorComponent from an EntityId. Returns false if no rotor was attached.
bool EntityRegistry_RemoveRotor(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyDetachedComponent(entity, ENTITY_COMPONENT_ROTOR);
    }

    return ComponentStore_Remove(&registry.rotor_components, entity_id);
}

// ============================================================================
// Component Accessors - Gear Component
// ============================================================================

// Attach a GearComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachGear(EntityId entity_id, const GearComponent *gear)
{
    if (!registry.is_initialised || !gear || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Prevent duplicate component attachment.
    if (!ComponentStore_Attach(&registry.gear_components, entity_id, gear))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_GEAR, gear);
    }

    return true;
}

// Retrieve the GearComponent attached to an EntityId, or NULL if not attached.
GearComponent *EntityRegistry_GetGear(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return NULL;
    }

    return (GearComponent *)ComponentStore_Get(&registry.gear_components, entity_id);
}

// Remove the GearComponent from an EntityId. Returns false if no gear was attached.
bool EntityRegistry_RemoveGear(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyDetachedComponent(entity, ENTITY_COMPONENT_GEAR);
    }

    return ComponentStore_Remove(&registry.gear_components, entity_id);
}

// ============================================================================
// Component Accessors - Portal Component
// ============================================================================

// Attach a PortalEntity to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachPortal(EntityId entity_id, const PortalEntity *portal)
{
    if (!registry.is_initialised || !portal || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Prevent duplicate component attachment.
    if (!ComponentStore_Attach(&registry.portal_components, entity_id, portal))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_PORTAL, portal);
    }

    return true;
}

// Retrieve the PortalEntity attached to an EntityId, or NULL if not attached.
PortalEntity *EntityRegistry_GetPortal(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return NULL;
    }

    return (PortalEntity *)ComponentStore_Get(&registry.portal_components, entity_id);
}

// Remove the PortalEntity from an EntityId. Returns false if no portal was attached.
bool EntityRegistry_RemovePortal(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Synchronise physical state if entity is currently stored in a world.
    Newtonoid2d *entity = EntityRegistry_GetEntity(entity_id);
    if (entity)
    {
        EntityLifecycle_ApplyDetachedComponent(entity, ENTITY_COMPONENT_PORTAL);
    }

    // Clean up any bidirectional portal links involving this entity.
    RelationComponent *relation = EntityRegistry_GetRelation(entity_id);
    if (relation && relation->type == RELATION_PORTAL_LINKED)
    {
        EntityId target_id = relation->target_entity;
        EntityRegistry_RemoveRelation(entity_id);
        RelationComponent *target_relation = EntityRegistry_GetRelation(target_id);
        if (target_relation && target_relation->type == RELATION_PORTAL_LINKED &&
            target_relation->target_entity == entity_id)
        {
            EntityRegistry_RemoveRelation(target_id);
        }
    }

    return ComponentStore_Remove(&registry.portal_components, entity_id);
}

// ============================================================================
// Component Accessors - Relation Component
// ============================================================================

// Attach a RelationComponent to an EntityId. Returns false if entity already has a component.
bool EntityRegistry_AttachRelation(EntityId entity_id, const RelationComponent *relation)
{
    if (!registry.is_initialised || !relation || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    // Prevent duplicate component attachment.
    if (!ComponentStore_Attach(&registry.relation_components, entity_id, relation))
    {
        return false;
    }
    return true;
}

// Retrieve the RelationComponent attached to an EntityId, or NULL if not attached.
RelationComponent *EntityRegistry_GetRelation(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return NULL;
    }

    return (RelationComponent *)ComponentStore_Get(&registry.relation_components, entity_id);
}

// Remove the RelationComponent from an EntityId. Returns false if no relation was attached.
bool EntityRegistry_RemoveRelation(EntityId entity_id)
{
    if (!registry.is_initialised || !EntityRegistry_IsIdActive(entity_id))
    {
        return false;
    }

    return ComponentStore_Remove(&registry.relation_components, entity_id);
}
