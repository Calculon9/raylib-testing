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
    CMD_ADD_CELL = 9,
    CMD_DELETE_CELL = 10,
    
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

    // Tool-toolbar actions. These are NOT CMD_TOGGLE_* commands, so they are not intercepted by
    // the CMD_TOGGLE_* -> debug-overlay mapping; each toggles its tool radio-exclusively instead.
    CMD_TOOL_NUDGE = 200,
    CMD_TOOL_CELL_PAINT = 201,
    CMD_CYCLE_TILE_TYPE = 202,   // Plain action: advance the active paint tile type. Never changes
                                 // the active tool (not a radio tool); only cycles the paint type.
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

// Payload for CMD_ADD_CELL / CMD_DELETE_CELL: identifies a single cell by its owning world and its
// positional (dense) cell index. Toggling presence never changes the cell's index identity.
typedef struct
{
    int world_index;   // Owning world index (nested index or UNIVERSE_ROOT_WORLD_INDEX).
    int cell_index;    // Linear positional cell index within that world's grid space.
    int tile_type;     // Tile type to stamp on ADD (paint). Ignored by DELETE. 0 = grid default.
} CellPresenceCommand;

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
        CellPresenceCommand cell_presence;   // CMD_ADD_CELL / CMD_DELETE_CELL
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
// Enqueue marking a cell PRESENT (add) / ABSENT (delete) by its owning world and positional index.
// EnqueueAddCell also carries the tile_type to stamp on the cell when it is marked present (paint);
// DELETE never reads or resets tile_type.
bool EnqueueAddCell(int world_index, int cell_index, int tile_type);
bool EnqueueDeleteCell(int world_index, int cell_index);
void ProcessCommandQueue(void);

#endif
