/**********************************************************************************************
 *
 * ENTITY REGISTRY MODULE
 *
 * Provides centralised EntityId allocation, location tracking, and component attachment
 * storage using the generic SlotMap collection. Both entity locations and optional gameplay
 * components are embedded directly within each generational slot record, providing O(1)
 * lookups without secondary lists or linear scans.
 *
 **********************************************************************************************/
#include "entities/entity_registry.h"
#include "collections/slot_map.h"
#include "common/common.h"
#include "world/world.h"
#include "world/universe.h"

// Stored record for an entity slot: world location and optional component payload.
typedef struct EntitySlotRecord
{
    EntityId world_id;          // Owning world entity ID, or INVALID_ENTITY_ID if none.
    int array_index;            // -1 for world root, >= 0 for index in world->objects.
    bool has_location;          // Flag indicating whether location data is currently bound.
    EntityComponent component;  // Optional component (type == ENTITY_COMPONENT_NONE if unattached).
} EntitySlotRecord;

// Central registry holding generational slot records.
typedef struct EntityRegistry
{
    SlotMap slots;              // Generational slot map storing EntitySlotRecord payloads.
    bool is_initialised;        // Initialisation state flag.
} EntityRegistry;

static EntityRegistry registry = {0};

// Initialise the entity registry and backing slot storage.
void EntityRegistry_Init(void)
{
    if (registry.is_initialised)
    {
        EntityRegistry_Shutdown();
    }

    // Allocate slot storage with initial capacity of 8 slot records.
    registry.slots = MakeSlotMap(8, sizeof(EntitySlotRecord));
    registry.is_initialised = true;
}

// Release all resources held by the registry and reset internal state.
void EntityRegistry_Shutdown(void)
{
    if (!registry.is_initialised)
    {
        return;
    }

    ClearSlotMap(&registry.slots);
    registry.is_initialised = false;
}

// Allocate a slot-based ID whose generation detects stale references.
EntityId EntityRegistry_AllocateId(void)
{
    if (!registry.is_initialised)
    {
        EntityRegistry_Init();
    }

    // Insert an empty record with no location and no attached component.
    EntitySlotRecord empty_record = {
        .world_id = INVALID_ENTITY_ID,
        .array_index = -1,
        .has_location = false,
        .component = {.type = ENTITY_COMPONENT_NONE}};
    SlotHandle handle = SlotMap_Insert(&registry.slots, &empty_record);
    return (EntityId)handle;
}

// Release an ID, clear its attached state, and advance generation to invalidate live handles.
void EntityRegistry_ReleaseId(EntityId entity_id)
{
    if (!registry.is_initialised || entity_id == INVALID_ENTITY_ID)
    {
        return;
    }

    // Removing the slot from the slot map recycles it and advances its generation automatically.
    SlotMap_Remove(&registry.slots, (SlotHandle)entity_id);
}

// Return whether an ID currently refers to an active, valid slot.
bool EntityRegistry_IsIdActive(EntityId entity_id)
{
    if (!registry.is_initialised || entity_id == INVALID_ENTITY_ID)
    {
        return false;
    }

    return SlotMap_Contains(&registry.slots, (SlotHandle)entity_id);
}

// Inspect the next slot-based ID that would be allocated without advancing state.
EntityId EntityRegistry_GetNextId(void)
{
    if (!registry.is_initialised)
    {
        EntityRegistry_Init();
    }

    return (EntityId)SlotMap_GetNextHandle(&registry.slots);
}

// Record or update the world storage location for an active entity.
bool EntityRegistry_SetLocation(EntityId entity_id, EntityId world_id, int array_index)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
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
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
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
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
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

// Attach an optional component to an EntityId in O(1) time.
bool EntityRegistry_RegisterComponent(EntityId entity_id, const EntityComponent *component)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
    if (!record || !component || component->type == ENTITY_COMPONENT_NONE)
    {
        return false;
    }

    // Prevent duplicate component attachment if a component is already active on this entity.
    if (record->component.type != ENTITY_COMPONENT_NONE)
    {
        return false;
    }

    record->component = *component;
    return true;
}

// Retrieve an optional component attached to an EntityId by component type in O(1) time.
EntityComponent *EntityRegistry_GetComponent(EntityId entity_id, EntityComponentType type)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
    if (!record || type == ENTITY_COMPONENT_NONE || record->component.type != type)
    {
        return NULL;
    }

    return &record->component;
}

// Remove an attached optional component from an EntityId in O(1) time.
void EntityRegistry_RemoveComponent(EntityId entity_id)
{
    EntitySlotRecord *record = (EntitySlotRecord *)SlotMap_Get(&registry.slots, (SlotHandle)entity_id);
    if (!record)
    {
        return;
    }

    record->component = (EntityComponent){.type = ENTITY_COMPONENT_NONE};
}
