#include "system/command_queue.h"
#include <string.h>
#include "system/systems.h"
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "common/common.h"

// Simple circular buffer queue
#define COMMAND_QUEUE_CAPACITY 128
static Command queue[COMMAND_QUEUE_CAPACITY];
static int q_head = 0;
static int q_tail = 0;
static int q_count = 0;

static bool EnqueueCommand(CommandType type, const Newtonoid2dParams *params, int world_select_delta)
{
    if (q_count >= COMMAND_QUEUE_CAPACITY)
    {
        return false;
    }

    Command *command = &queue[q_tail];
    command->type = type;
    command->params = params ? *params : (Newtonoid2dParams){0};
    command->world_select_delta = world_select_delta;

    q_tail = (q_tail + 1) % COMMAND_QUEUE_CAPACITY;
    q_count++;
    return true;
}

void InitCommandQueue(void)
{
    MemorySet(queue, 0, sizeof(queue));
    q_head = q_tail = q_count = 0;
}

bool EnqueueCreateEntity(const Newtonoid2dParams *params)
{
    if (!params)
    {
        return false;
    }

    if (!EnqueueCommand(CMD_CREATE_ENTITY, params, 0))
    {
        return false;
    }

    LOG_INFO("Enqueued CMD_CREATE_ENTITY (queue_count=%d)\n", q_count);
    return true;
}

bool EnqueueDeleteEntity(Newtonoid2d *obj)
{
    if (!obj)
    {
        return false;
    }

    if (!EnqueueCommand(CMD_DELETE_ENTITY, NULL, 0))
    {
        return false;
    }

    LOG_INFO("Enqueued CMD_DELETE_ENTITY (queue_count=%d)\n", q_count);
    return true;
}

bool EnqueueCreateWorld(void)
{
    if (!EnqueueCommand(CMD_CREATE_WORLD, NULL, 0))
    {
        return false;
    }

    LOG_INFO("Enqueued CMD_CREATE_WORLD (queue_count=%d)\n", q_count);
    return true;
}

bool EnqueueSelectWorld(int delta)
{
    if (!EnqueueCommand(CMD_SELECT_WORLD, NULL, delta))
    {
        return false;
    }

    LOG_INFO("Enqueued CMD_SELECT_WORLD delta=%d (queue_count=%d)\n", delta, q_count);
    return true;
}

void ProcessCommandQueue(void)
{
    while (q_count > 0)
    {
        Command *c = &queue[q_head];
        if (c->type == CMD_CREATE_ENTITY)
        {
            // Resolve params to an entity and add to world
            Newtonoid2d *new_entity = ResolveEntityParamsToEntity(&c->params);
            World2d *world = Universe_GetSelectedWorld(&G_Universe);
            if (new_entity)
            {
                bool added_to_world = false;

                if (world)
                {
                    int entity_id = AddObjectToWorld(world, new_entity, world->grid_space.object.id);
                    if (entity_id >= 0)
                    {
                        added_to_world = true;
                        Newtonoid2d *spawned = GetEntityByID(world, entity_id);
                        if (spawned)
                        {
                            G_UIState.selected_object = spawned;
                        }
                        LOG_INFO("Processed CMD_CREATE_ENTITY -> spawned id=%d\n", entity_id);
                    }
                }

                // If spawn failed, the transient entity still owns its surface buffer.
                if (!added_to_world)
                {
                    LArray *vectors = &new_entity->surface.surface_vectors;
                    if (vectors->items && vectors->capacity > 0 && vectors->elem_bytes > 0)
                    {
                        size_t bytes = (size_t)vectors->capacity * vectors->elem_bytes;
                        Deallocate(&vectors->items, bytes);
                    }
                }

                // Always free the transient wrapper object allocated by CreateNewtonoid2d_Reference.
                Deallocate((void **)&new_entity, sizeof(Newtonoid2d));
            }
        }

        if (c->type == CMD_DELETE_ENTITY)
        {
            if (G_UIState.selected_object)
            {
                World2d *world = Universe_GetSelectedWorld(&G_Universe);
                int entity_id = G_UIState.selected_object->id;
                DeregisterEntity(world, entity_id);
                G_UIState.selected_object = NULL;
                LOG_INFO("Processed CMD_DELETE_ENTITY -> deleted id=%d\n", entity_id);
            }
        }

        if (c->type == CMD_CREATE_WORLD)
        {
            int world_index = CreateNewWorld(IsCreateWorldAutoSelectEnabled());
            if (world_index >= 0)
            {
                LOG_INFO("Processed CMD_CREATE_WORLD -> world_index=%d\n", world_index);
            }
        }

        if (c->type == CMD_SELECT_WORLD)
        {
            int world_count = GetWorldCount();
            if (world_count > 0)
            {
                int current_index = GetSelectedWorldIndex();
                int delta = c->world_select_delta;
                int next_index = (current_index + delta) % world_count;
                if (next_index < 0)
                {
                    next_index += world_count;
                }

                if (SelectWorldByIndex(next_index))
                {
                    LOG_INFO("Processed CMD_SELECT_WORLD -> selected_index=%d\n", next_index);
                }
            }
        }

        // Pop command (always advance exactly once per loop iteration)
        queue[q_head].type = CMD_NONE;
        q_head = (q_head + 1) % COMMAND_QUEUE_CAPACITY;
        q_count--;
    }
}
