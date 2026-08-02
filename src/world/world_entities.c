/**********************************************************************************************
 *
 * WORLD ENTITY MANAGEMENT
 *
 **********************************************************************************************/

#include "world/world_internal.h"

static LArray *GetWorldObjectArrayForArchetype(World2d *world, ArchetypeID array_type)
{
    if (!world)
    {
        return NULL;
    }

    return (array_type == ARCHETYPE_CLOCKED) ? &world->temp_objects : &world->objects;
}

static void FreeEntitySurfaceVectors(Newtonoid2d *entity)
{
    if (!entity)
    {
        return;
    }

    LArray *vectors = &entity->surface.surface_vectors;
    if (vectors->items && vectors->capacity > 0 && vectors->elem_bytes > 0)
    {
        size_t bytes = (size_t)vectors->capacity * vectors->elem_bytes;
        Deallocate(&vectors->items, bytes);
    }

    MemorySet(vectors, 0, sizeof(*vectors));
}

static void EnqueueWorldCommand(LArray *scheduled_events, WorldCmdType type, int object_id, int payload_value,
                                int initial_frame_delay, int interval_frames, int run_limit)
{
    if (!scheduled_events)
    {
        return;
    }

    WorldCommand cmd = {
        .type = type,
        .target_id = object_id,
        .payload_value = payload_value,
        .interval_frames = interval_frames <= 0 ? 1 : interval_frames,
        .run_limit = run_limit,
        .frame_count = 0,
        .initial_frame_delay = initial_frame_delay,
        .active = true};
    LArray_Push(scheduled_events, &cmd);
}

void SetObjectFlag(World2d *world, int object_id, int flag_to_update)
{
    Newtonoid2d *object = GetEntityByID(world, object_id);
    if (!object)
        return;

    object->status_flags = (object->status_flags | flag_to_update);
}

void ClearObjectFlag(World2d *world, int object_id, int flag_to_update)
{
    Newtonoid2d *object = GetEntityByID(world, object_id);
    if (!object)
        return;

    int inverse_flag = ~flag_to_update;
    object->status_flags = object->status_flags & inverse_flag;
}

void UpdateEntityWorldRegistry(FlatMapInt *entity_world_index_registry, int entity_id, int type_flag, int entity_arr_index)
{
    int max_type = (1 << PACKED_INT_HIGH_BITS) - 1;
    int max_index = (1 << PACKED_INT_LOW_BITS) - 1;

    assert(type_flag <= max_type && "Engine Error: type flag exceeds maximum capacity for high bits");
    assert(entity_arr_index <= max_index && "Engine Error: Array index exceeds maximum capacity for low bits.");

    int packed = PACK_INTS(entity_arr_index, type_flag);
    FlatMapInt_InsertOrUpdate(entity_world_index_registry, entity_id, packed);
}

int RegisterEntity(World2d *world, Newtonoid2d *entity)
{
    ArchetypeID array_type;

    if (!(entity->status_flags & FLAG_LIFETIME_CLOCKED))
    {
        array_type = ARCHETYPE_INHABITANT;
    }
    else
    {
        array_type = ARCHETYPE_CLOCKED;
    }

    LArray *world_objects = GetWorldObjectArrayForArchetype(world, array_type);
    if (!world_objects)
    {
        return 0;
    }

    entity->id = world->next_object_id++;
    LArray_Push(world_objects, entity);
    int assigned_index = world_objects->count - 1;
    UpdateEntityWorldRegistry(&world->entity_world_index_registry, entity->id, array_type, assigned_index);
    LOG_INFO("Registered entity %d at index %d in array type %d", entity->id, assigned_index, array_type);
    return entity->id;
}

void DeregisterEntity(World2d *world, int entity_id)
{
    if (!world)
    {
        return;
    }

    int packed_value = 0;
    if (!FlatMapInt_GetValue(&world->entity_world_index_registry, entity_id, &packed_value))
    {
        return;
    }

    int type = UNPACK_INT_HIGH(packed_value);
    int deleted_idx = UNPACK_INT_LOW(packed_value);
    LArray *world_objects = GetWorldObjectArrayForArchetype(world, (ArchetypeID)type);

    if (!LArray_IsValid(world_objects))
    {
        LOG_WARN("DeregisterEntity: world array empty for entity %d (type=%d). Clearing stale registry entry.\n", entity_id, type);
        FlatMapInt_DeactivateSlot(&world->entity_world_index_registry, entity_id);
        return;
    }

    Newtonoid2d *entity_to_delete = (Newtonoid2d *)LArray_Get(world_objects, deleted_idx);
    FreeEntitySurfaceVectors(entity_to_delete);

    if (deleted_idx < 0 || deleted_idx >= world_objects->count)
    {
        LOG_WARN("DeregisterEntity: stale index %d for entity %d (count=%d). Clearing stale registry entry.\n", deleted_idx, entity_id, world_objects->count);
        FlatMapInt_DeactivateSlot(&world->entity_world_index_registry, entity_id);
        return;
    }

    size_t last_idx = world_objects->count - 1;
    if (deleted_idx != last_idx)
    {
        Newtonoid2d *last_entity = (Newtonoid2d *)LArray_Get(world_objects, last_idx);
        if (last_entity)
        {
            UpdateEntityWorldRegistry(&world->entity_world_index_registry, last_entity->id, type, deleted_idx);
        }
        else
        {
            LOG_WARN("DeregisterEntity: last entity lookup failed (last_idx=%zu, count=%d).\n", last_idx, world_objects->count);
        }
    }

    LArray_SwapPopAt(world_objects, deleted_idx);
    FlatMapInt_DeactivateSlot(&world->entity_world_index_registry, entity_id);
    LOG_INFO("Entity %d safely deregistered and removed from slot %d\n", entity_id, deleted_idx);
}

void StickEntity(World2d *world, Newtonoid2d *child, Newtonoid2d *parent)
{
    if (!child || !parent)
        return;

    child->parent_id = parent->id;
    child->local_offset.x = child->coords_center.x - parent->coords_center.x;
    child->local_offset.y = child->coords_center.y - parent->coords_center.y;
    child->velocity.x = 0;
    child->velocity.y = 0;
}

void *GetEntityByID(World2d *world, int entity_id)
{
    int packed_value = 0;
    if (!FlatMapInt_GetValue(&world->entity_world_index_registry, entity_id, &packed_value))
    {
        return NULL;
    }

    int type_flag = UNPACK_INT_HIGH(packed_value);
    int index = UNPACK_INT_LOW(packed_value);

    LArray *world_objects = GetWorldObjectArrayForArchetype(world, (ArchetypeID)type_flag);
    return world_objects ? LArray_Get(world_objects, index) : NULL;
}

void ScheduleEntityFlagSet(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_SET_OBJECT_FLAG,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void ScheduleEntityFlagClear(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_CLEAR_OBJECT_FLAG,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void ScheduleEntityDeletion(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit)
{
    EnqueueWorldCommand(scheduled_events,
                        CMD_DELETE_OBJECT,
                        object_id,
                        flag_to_set,
                        initial_frame_delay,
                        interval_frames,
                        run_limit);
}

void RunScheduledWorldCmds(LArray *scheduled_cmds, World2d *world)
{
    WorldCommand *cmds = (WorldCommand *)scheduled_cmds->items;
    size_t i = 0;

    while (i < scheduled_cmds->count)
    {
        if (cmds[i].initial_frame_delay > 0)
        {
            cmds[i].initial_frame_delay--;
            i++;
            continue;
        }

        if (cmds[i].frame_count > 0)
        {
            cmds[i].frame_count--;
            i++;
            continue;
        }

        switch (cmds[i].type)
        {
        case CMD_SET_OBJECT_FLAG:
            SetObjectFlag(world, cmds[i].target_id, cmds[i].payload_value);
            break;
        case CMD_CLEAR_OBJECT_FLAG:
            ClearObjectFlag(world, cmds[i].target_id, cmds[i].payload_value);
            break;
        case CMD_DELETE_OBJECT:
            DeregisterEntity(world, cmds[i].target_id);
            break;
        default:
            LOG_ERROR("Unknown command type %d\n", cmds[i].type);
            break;
        }

        cmds[i].run_count++;
        if (cmds[i].run_count >= cmds[i].run_limit)
        {
            size_t last_idx = scheduled_cmds->count - 1;
            if (i != last_idx)
            {
                cmds[i] = cmds[last_idx];
            }
            scheduled_cmds->count--;
        }
        else
        {
            cmds[i].frame_count = cmds[i].interval_frames;
            i++;
        }
    }
}
