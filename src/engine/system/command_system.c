#include "system/command_system.h"
#include <string.h>
#include "entities/entity_factory.h"
#include "entities/entity_registry.h"
#include "system/systems.h"
#include "system/universe_system.h"
#include "world/world_internal.h"
#include "input/drag_interaction.h"
#include "common/common.h"
#include "system/ui_system.h"
#include "system/ui/state_manager_system.h"
#include "system/debug_overlay_system.h"

// Forward declaration for internal command execution logic
static void ExecuteCommandInternal(Command *c);

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

// ============================================================================
// Initialisation
// ============================================================================

void InitCommandSystem(void)
{
    MemorySet(queue, 0, sizeof(queue));
    q_head = q_tail = q_count = 0;
}

// ============================================================================
// Command String Resolution
// ============================================================================

/**
 * Resolve command name strings to CommandType codes.
 * Maps game-specific action names (from XML/config) to CommandType codes.
 * Returns 0 (CMD_NONE) if command string is not recognised.
 */
int CommandSystem_ResolveString(const char *cmd_string)
{
    if (!cmd_string)
        return 0;  // Unresolved
    
    // Standard entity/world actions
    if (!strcmp(cmd_string, "create-entity"))
        return CMD_CREATE_ENTITY;
    else if (!strcmp(cmd_string, "delete-entity"))
        return CMD_DELETE_ENTITY;
    else if (!strcmp(cmd_string, "create-world"))
        return CMD_CREATE_WORLD;
    else if (!strcmp(cmd_string, "select-world-prev"))
        return CMD_SELECT_WORLD;
    else if (!strcmp(cmd_string, "select-world-next"))
        return CMD_SELECT_WORLD;
    // Debug toggle actions
    else if (!strcmp(cmd_string, "toggle-debug-dashboard"))
        return CMD_TOGGLE_DEBUG_DASHBOARD;
    else if (!strcmp(cmd_string, "toggle-viewport-grid"))
        return CMD_TOGGLE_VIEWPORT_GRID;
    else if (!strcmp(cmd_string, "toggle-world-grid"))
        return CMD_TOGGLE_WORLD_GRID;
    else if (!strcmp(cmd_string, "toggle-world-grid-labels"))
        return CMD_TOGGLE_WORLD_GRID_LABELS;
    else if (!strcmp(cmd_string, "toggle-universe-grid-labels"))
        return CMD_TOGGLE_UNIVERSE_GRID_LABELS;
    else if (!strcmp(cmd_string, "toggle-ui-borders"))
        return CMD_TOGGLE_UI_BORDERS;
    else if (!strcmp(cmd_string, "toggle-object-axes"))
        return CMD_TOGGLE_OBJECT_AXES;
    else if (!strcmp(cmd_string, "toggle-object-hull"))
        return CMD_TOGGLE_OBJECT_HULL;
    else if (!strcmp(cmd_string, "toggle-object-aabb"))
        return CMD_TOGGLE_OBJECT_AABB;
    
    return 0;  // Unresolved
}

// ============================================================================
// Deferred Command Enqueue
// ============================================================================

bool EnqueueCreateEntity(const EntityCreateParams *params)
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

bool EnqueueDeleteWorld(int world_index)
{
    if (world_index < 0)
    {
        return false;
    }

    return EnqueueCommandWithQueueLog(CMD_DELETE_WORLD, &world_index,
                                      sizeof(world_index), "CMD_DELETE_WORLD");
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
                       Vector2d destination_coords, EntityRoleFlags original_collision_role_mask)
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
        .original_collision_role_mask = original_collision_role_mask};
    if (!EnqueueCommandWithQueueLog(CMD_MOVE_ENTITY, &move, sizeof(move), "CMD_MOVE_ENTITY"))
    {
        return false;
    }

    return true;
}

// Enqueue attaching a component to an existing entity.
bool EnqueueAttachComponent(EntityId entity_id, const EntityComponent *component)
{
    if (entity_id == INVALID_ENTITY_ID || !component || component->type == ENTITY_COMPONENT_NONE)
    {
        return false;
    }

    AttachComponentCommand cmd = {
        .entity_id = entity_id,
        .component = *component,
    };
    return EnqueueCommandWithQueueLog(CMD_ATTACH_COMPONENT, &cmd, sizeof(cmd), "CMD_ATTACH_COMPONENT");
}

// Enqueue detaching a component from an existing entity.
bool EnqueueRemoveComponent(EntityId entity_id, EntityComponentType component_type)
{
    if (entity_id == INVALID_ENTITY_ID || component_type == ENTITY_COMPONENT_NONE)
    {
        return false;
    }

    RemoveComponentCommand cmd = {
        .entity_id = entity_id,
        .component_type = component_type,
    };
    return EnqueueCommandWithQueueLog(CMD_REMOVE_COMPONENT, &cmd, sizeof(cmd), "CMD_REMOVE_COMPONENT");
}

// ============================================================================
// Command Execution
// ============================================================================

/**
 * Execute a command immediately (synchronous dispatch).
 * Can be called from UI, hotkeys, scripts, or network messages.
 * Does not use the queue.
 */
// Explicit CMD_TOGGLE_* -> DebugOverlayId mapping.
bool CommandSystem_ResolveToggleOverlay(CommandType type, int *out_overlay_id)
{
    DebugOverlayId overlay_id;
    switch (type)
    {
    case CMD_TOGGLE_DEBUG_DASHBOARD:      overlay_id = DEBUG_DASHBOARD; break;
    case CMD_TOGGLE_VIEWPORT_GRID:        overlay_id = DEBUG_VIEWPORT_GRID; break;
    case CMD_TOGGLE_WORLD_GRID:           overlay_id = DEBUG_WORLD_GRID; break;
    case CMD_TOGGLE_WORLD_GRID_LABELS:    overlay_id = DEBUG_WORLD_GRID_LABELS; break;
    case CMD_TOGGLE_UNIVERSE_GRID_LABELS: overlay_id = DEBUG_UNIVERSE_GRID_LABELS; break;
    case CMD_TOGGLE_UI_BORDERS:           overlay_id = DEBUG_UI_BORDERS; break;
    case CMD_TOGGLE_OBJECT_AXES:          overlay_id = DEBUG_OBJECT_AXES; break;
    case CMD_TOGGLE_OBJECT_HULL:          overlay_id = DEBUG_OBJECT_HULL; break;
    case CMD_TOGGLE_OBJECT_AABB:          overlay_id = DEBUG_OBJECT_AABB; break;
    default:
        return false; // not a toggle command
    }

    if (out_overlay_id)
    {
        *out_overlay_id = (int)overlay_id;
    }
    return true;
}

void ExecuteCommand(CommandType type, const void *data)
{
    // Debug toggle commands: dispatch via the explicit mapping (no enum-order arithmetic).
    int overlay_id = 0;
    if (CommandSystem_ResolveToggleOverlay(type, &overlay_id))
    {
        ToggleDebug((DebugOverlayId)overlay_id);
        return;
    }
    
    // Standard deferred commands - convert to Command struct for consistency
    Command cmd = {.type = type};
    if (data)
    {
        switch (type)
        {
        case CMD_CREATE_ENTITY:
            if (data) cmd.data.create_entity = *(const EntityCreateParams *)data;
            break;
        case CMD_DELETE_ENTITY:
            if (data) cmd.data.delete_entity = *(const EntityId *)data;
            break;
        case CMD_SELECT_WORLD:
            if (data) cmd.data.world_select_delta = *(const int *)data;
            break;
        case CMD_DELETE_WORLD:
            if (data) cmd.data.world_delete_index = *(const int *)data;
            break;
        case CMD_MOVE_ENTITY:
            if (data) cmd.data.move_entity = *(const MoveEntityCommand *)data;
            break;
        case CMD_ATTACH_COMPONENT:
            if (data) cmd.data.attach_component = *(const AttachComponentCommand *)data;
            break;
        case CMD_REMOVE_COMPONENT:
            if (data) cmd.data.remove_component = *(const RemoveComponentCommand *)data;
            break;
        default:
            break;
        }
    }
    
    // Execute the command
    ExecuteCommandInternal(&cmd);
}

/**
 * Internal: Execute a Command struct.
 * Shared by both immediate dispatch (ExecuteCommand) and deferred queue (ProcessCommandQueue).
 */
static void ExecuteCommandInternal(Command *c)
{
    if (!c)
        return;
    
    if (c->type == CMD_CREATE_ENTITY)
    {
        EntityId spawned_id = EntityFactory_Spawn(&G_Universe.root_world,
                                                  &c->data.create_entity,
                                                  G_Universe.root_world.grid_space.object.id);
        if (spawned_id != INVALID_ENTITY_ID)
        {
            UIState_SetSelectedObjectById(spawned_id);
            LOG_INFO("Processed CMD_CREATE_ENTITY -> spawned id=%d\n", spawned_id);
        }
    }
    else if (c->type == CMD_DELETE_ENTITY)
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
    else if (c->type == CMD_CREATE_WORLD)
    {
        int world_index = CreateNewWorld(IsCreateWorldAutoSelectEnabled());
        if (world_index >= 0)
        {
            LOG_INFO("Processed CMD_CREATE_WORLD -> world_index=%d\n", world_index);
        }
    }
    else if (c->type == CMD_DELETE_WORLD)
    {
        int world_index = c->data.world_delete_index;
        if (Universe_DeleteWorld(&G_Universe, world_index))
        {
            // World compaction can invalidate selected entity and cell pointers.
            UIState_SetSelection(NULL, NULL, -1);
            DragInteraction_ClearCapture(DragInteraction_GetContext(DRAG_CONTEXT_GAME));
            LOG_INFO("Processed CMD_DELETE_WORLD -> world_index=%d\n", world_index);
        }
    }
    else if (c->type == CMD_SELECT_WORLD)
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
    else if (c->type == CMD_MOVE_ENTITY)
    {
        World2d *source_world = Universe_GetWorld(&G_Universe, c->data.move_entity.source_world_index);
        World2d *destination_world = Universe_GetWorld(&G_Universe, c->data.move_entity.destination_world_index);
        EntityId moved_id = MoveObjectBetweenWorlds(source_world, destination_world, c->data.move_entity.entity_id,
                                                    c->data.move_entity.destination_parent_id,
                                                    c->data.move_entity.destination_coords);
        if (moved_id != INVALID_ENTITY_ID)
        {
            Universe_SelectWorld(&G_Universe, c->data.move_entity.destination_world_index);
            UIState_SetSelectedObjectById(moved_id);
            Newtonoid2d *selected_object = UIState_GetSelectedObject();
            if (selected_object)
            {
                selected_object->collision_role_mask = c->data.move_entity.original_collision_role_mask;

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
                entity->collision_role_mask = c->data.move_entity.original_collision_role_mask;
            }
        }
    }
    else if (c->type == CMD_ATTACH_COMPONENT)
    {
        EntityId entity_id = c->data.attach_component.entity_id;
        EntityComponentType type = c->data.attach_component.component.type;
        const void *comp_data = NULL;
        switch (type)
        {
        case ENTITY_COMPONENT_ROTOR:
            comp_data = &c->data.attach_component.component.data.rotor;
            break;
        case ENTITY_COMPONENT_GEAR:
            comp_data = &c->data.attach_component.component.data.gear;
            break;
        case ENTITY_COMPONENT_PORTAL:
            comp_data = &c->data.attach_component.component.data.portal;
            break;
        case ENTITY_COMPONENT_RELATION:
            comp_data = &c->data.attach_component.component.data.relation;
            break;
        case ENTITY_COMPONENT_HEALTH:
            comp_data = &c->data.attach_component.component.data.health;
            break;
        case ENTITY_COMPONENT_NONE:
        default:
            break;
        }

        if (comp_data && EntityRegistry_AttachComponent(entity_id, type, comp_data))
        {
            LOG_INFO("Processed CMD_ATTACH_COMPONENT -> entity_id=%d, type=%d\n", entity_id, type);
            MarkStateManagerRefreshDirty();
        }
    }
    else if (c->type == CMD_REMOVE_COMPONENT)
    {
        EntityId entity_id = c->data.remove_component.entity_id;
        EntityComponentType type = c->data.remove_component.component_type;
        if (EntityRegistry_RemoveComponent(entity_id, type))
        {
            LOG_INFO("Processed CMD_REMOVE_COMPONENT -> entity_id=%d, type=%d\n", entity_id, type);
            MarkStateManagerRefreshDirty();
        }
    }
}

// ============================================================================
// Deferred Command Processing
// ============================================================================

void ProcessCommandQueue(void)
{
    while (q_count > 0)
    {
        Command *c = &queue[q_head];
        
        // Execute the command
        ExecuteCommandInternal(c);
        
        // Pop command (always advance exactly once per loop iteration)
        queue[q_head].type = CMD_NONE;
        q_head = (q_head + 1) % COMMAND_QUEUE_CAPACITY;
        q_count--;
    }
}
