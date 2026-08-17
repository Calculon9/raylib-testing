#include "system/command_queue.h"
#include <string.h>
#include "system/systems.h"
#include "world/universe.h"
#include "world/world.h"
#include "world/world_internal.h"
#include "input/drag_interaction.h"
#include "common/common.h"
#include "system/ui_system.h"

// Simple circular buffer queue
#define COMMAND_QUEUE_CAPACITY 128
static Command queue[COMMAND_QUEUE_CAPACITY];
static int q_head = 0;
static int q_tail = 0;
static int q_count = 0;

static bool EnqueueCommand(CommandType type, const void *payload, size_t payload_size)
{
    if (q_count >= COMMAND_QUEUE_CAPACITY || payload_size > sizeof(queue[q_tail].data))
    {
        return false;
    }

    Command *command = &queue[q_tail];
    MemorySet(&command->data, 0, sizeof(command->data));
    command->type = type;
    if (payload && payload_size > 0)
    {
        memcpy(&command->data, payload, payload_size);
    }

    q_tail = (q_tail + 1) % COMMAND_QUEUE_CAPACITY;
    q_count++;
    return true;
}

static bool EnqueueCommandWithQueueLog(CommandType type, const void *payload,
                                       size_t payload_size, const char *command_name)
{
    if (!EnqueueCommand(type, payload, payload_size))
    {
        return false;
    }

    if (command_name)
    {
        LOG_INFO("Enqueued %s (queue_count=%d)\n", command_name, q_count);
    }

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

    return EnqueueCommandWithQueueLog(CMD_CREATE_ENTITY, params, sizeof(*params), "CMD_CREATE_ENTITY");
}

bool EnqueueDeleteEntity(EntityId entity_id)
{
    if (entity_id == INVALID_ENTITY_ID)
    {
        return false;
    }

    return EnqueueCommandWithQueueLog(CMD_DELETE_ENTITY, &entity_id, sizeof(entity_id), "CMD_DELETE_ENTITY");
}

bool EnqueueCreateWorld(void)
{
    return EnqueueCommandWithQueueLog(CMD_CREATE_WORLD, NULL, 0, "CMD_CREATE_WORLD");
}

bool EnqueueSelectWorld(int delta)
{
    if (!EnqueueCommandWithQueueLog(CMD_SELECT_WORLD, &delta, sizeof(delta), NULL))
    {
        return false;
    }

    LOG_INFO("Enqueued CMD_SELECT_WORLD delta=%d (queue_count=%d)\n", delta, q_count);
    return true;
}

bool EnqueueMoveEntity(EntityId entity_id, int source_world_index,
                       int destination_world_index, EntityId destination_parent_id,
                       Vector2d destination_coords, uint32_t original_collision_mask)
{
    if (entity_id == INVALID_ENTITY_ID || q_count >= COMMAND_QUEUE_CAPACITY)
    {
        return false;
    }

    MoveEntityCommand move = {
        .entity_id = entity_id,
        .source_world_index = source_world_index,
        .destination_world_index = destination_world_index,
        .destination_parent_id = destination_parent_id,
        .destination_coords = destination_coords,
        .original_collision_mask = original_collision_mask};
    if (!EnqueueCommandWithQueueLog(CMD_MOVE_ENTITY, &move, sizeof(move), "CMD_MOVE_ENTITY"))
    {
        return false;
    }

    return true;
}

void ProcessCommandQueue(void)
{
    while (q_count > 0)
    {
        Command *c = &queue[q_head];
        if (c->type == CMD_CREATE_ENTITY)
        {
            // New entities always enter the universe's root world first, unworlded.
            Newtonoid2d *new_entity = ResolveEntityParamsToEntity(&c->data.create_entity);
            if (new_entity)
            {
                EntityId spawned_id = AddObjectToWorld(&G_Universe.root_world, new_entity,
                                                       G_Universe.root_world.grid_space.object.id);
                if (spawned_id != INVALID_ENTITY_ID)
                {
                    UIState_SetSelectedObjectById(spawned_id);
                    LOG_INFO("Processed CMD_CREATE_ENTITY -> spawned id=%d\n", spawned_id);
                }
                else
                {
                    LArray *vectors = &new_entity->surface.surface_vectors;
                    if (vectors->items && vectors->capacity > 0 && vectors->elem_bytes > 0)
                    {
                        size_t bytes = (size_t)vectors->capacity * vectors->elem_bytes;
                        Deallocate(&vectors->items, bytes);
                    }
                }

                // The world's object array owns the surface buffer after a successful copy.
                Deallocate((void **)&new_entity, sizeof(Newtonoid2d));
            }
        }

        if (c->type == CMD_DELETE_ENTITY)
        {
            EntityId entity_id = c->data.delete_entity;
            int world_index = -1;
            Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe, entity_id, &world_index);
            World2d *owner_world = Universe_GetWorld(&G_Universe, world_index);
            if (entity && owner_world)
            {
                DeregisterEntity(owner_world, entity_id);
                if (UIState_GetSelectedObjectId() == entity_id)
                {
                    UIState_ClearSelectedObject();
                }
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
                int delta = c->data.world_select_delta;
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

        if (c->type == CMD_MOVE_ENTITY)
        {
            World2d *source_world = Universe_GetWorld(&G_Universe, c->data.move_entity.source_world_index);
            World2d *destination_world = Universe_GetWorld(&G_Universe, c->data.move_entity.destination_world_index);
            EntityId moved_id = MoveObjectBetweenWorlds(source_world,destination_world,c->data.move_entity.entity_id,
                                                         c->data.move_entity.destination_parent_id,
                                                         c->data.move_entity.destination_coords);
            if (moved_id != INVALID_ENTITY_ID)
            {
                Universe_SelectWorld(&G_Universe, c->data.move_entity.destination_world_index);
                UIState_SetSelectedObjectById(moved_id);
                Newtonoid2d *selected_object = UIState_GetSelectedObject();
                if (selected_object)
                {
                    selected_object->collision_mask = c->data.move_entity.original_collision_mask;

                    DragInteractionState *game_drag_ctx = DragInteraction_GetContext(DRAG_CONTEXT_GAME);
                    if (game_drag_ctx && game_drag_ctx->has_capture &&
                        game_drag_ctx->target_kind == DRAG_TARGET_WORLD_ENTITY)
                    {
                        game_drag_ctx->target = selected_object;
                        game_drag_ctx->target_anchor = selected_object->anchor_position;
                        game_drag_ctx->pointer_state.initial_pos = game_drag_ctx->pointer_state.current_pos;
                        game_drag_ctx->pointer_state.previous_pos = game_drag_ctx->pointer_state.current_pos;
                    }
                }
                LOG_INFO("Processed CMD_MOVE_ENTITY -> moved id=%d\n", moved_id);
            }
            else
            {
                Newtonoid2d *entity = Universe_GetEntityByID(&G_Universe,
                                                              c->data.move_entity.entity_id,
                                                              NULL);
                if (entity)
                {
                    entity->collision_mask = c->data.move_entity.original_collision_mask;
                }
            }
        }

        // Pop command (always advance exactly once per loop iteration)
        queue[q_head].type = CMD_NONE;
        q_head = (q_head + 1) % COMMAND_QUEUE_CAPACITY;
        q_count--;
    }
}
