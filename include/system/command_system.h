#ifndef COMMAND_SYSTEM_H
#define COMMAND_SYSTEM_H

#include "entities/entity_flags.h"
#include "entities/entity_factory.h"
#include "system/systems.h"

typedef enum
{
    CMD_NONE = 0,
    CMD_CREATE_ENTITY = 1,
    CMD_DELETE_ENTITY = 2,
    CMD_CREATE_WORLD = 3,
    CMD_SELECT_WORLD = 4,
    CMD_MOVE_ENTITY = 5,
    CMD_DELETE_WORLD = 6,
    CMD_ATTACH_COMPONENT = 7,
    CMD_REMOVE_COMPONENT = 8,
    
    // Toggle actions
    CMD_TOGGLE_DEBUG_DASHBOARD = 100,
    CMD_TOGGLE_VIEWPORT_GRID,
    CMD_TOGGLE_WORLD_GRID,
    CMD_TOGGLE_WORLD_GRID_LABELS,
    CMD_TOGGLE_UNIVERSE_GRID_LABELS,
    CMD_TOGGLE_UI_BORDERS,
    CMD_TOGGLE_OBJECT_AXES,
    CMD_TOGGLE_OBJECT_HULL,
    CMD_TOGGLE_OBJECT_AABB,
} CommandType;

typedef struct
{
    EntityId entity_id;
    int source_world_index;
    int destination_world_index;
    EntityId destination_parent_id;
    Vector2d destination_coords;
    EntityRoleFlags original_collision_role_mask;
} MoveEntityCommand;

typedef struct
{
    EntityId entity_id;
    EntityComponent component;
} AttachComponentCommand;

typedef struct
{
    EntityId entity_id;
    EntityComponentType component_type;
} RemoveComponentCommand;

typedef struct
{
    CommandType type;
    union
    {
        EntityCreateParams create_entity;
        EntityId delete_entity;
        int world_select_delta;
        int world_delete_index;
        MoveEntityCommand move_entity;
        AttachComponentCommand attach_component;
        RemoveComponentCommand remove_component;
    } data;
} Command;

void InitCommandSystem(void);

// Resolve command name strings to CommandType codes.
// Returns 0 (CMD_NONE) if the command string is not recognised.
int CommandSystem_ResolveString(const char *cmd_string);

// Map a CMD_TOGGLE_* command code to its debug-overlay id via an EXPLICIT per-command mapping
// (no reliance on CMD_TOGGLE_* and DebugOverlayId sharing enum order). The overlay id is returned
// as a plain int so this header stays free of the debug-overlay type; the caller casts to
// DebugOverlayId at the dispatch point. Returns true and writes *out_overlay_id for a toggle
// command; returns false (and leaves *out_overlay_id untouched) for any non-toggle command.
bool CommandSystem_ResolveToggleOverlay(CommandType type, int *out_overlay_id);

// Execute a command immediately (synchronous)
void ExecuteCommand(CommandType type, const void *data);

// Enqueue a command for deferred processing (next frame)
bool EnqueueCreateEntity(const EntityCreateParams *params);
bool EnqueueDeleteEntity(EntityId entity_id);
bool EnqueueCreateWorld(void);
bool EnqueueSelectWorld(int delta);
bool EnqueueDeleteWorld(int world_index);
bool EnqueueMoveEntity(EntityId entity_id, int source_world_index,
                       int destination_world_index, EntityId destination_parent_id,
                       Vector2d destination_coords, EntityRoleFlags original_collision_role_mask);
// Enqueue attaching a component to an existing entity.
bool EnqueueAttachComponent(EntityId entity_id, const EntityComponent *component);
// Enqueue detaching a component from an existing entity.
bool EnqueueRemoveComponent(EntityId entity_id, EntityComponentType component_type);
void ProcessCommandQueue(void);

#endif
