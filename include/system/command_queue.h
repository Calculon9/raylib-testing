#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

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
} CommandType;

typedef struct
{
    EntityId entity_id;
    int source_world_index;
    int destination_world_index;
    EntityId destination_parent_id;
    Vector2d destination_coords;
    uint32_t original_collision_mask;
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

void InitCommandQueue(void);
bool EnqueueCreateEntity(const EntityCreateParams *params);
bool EnqueueDeleteEntity(EntityId entity_id);
bool EnqueueCreateWorld(void);
bool EnqueueSelectWorld(int delta);
bool EnqueueDeleteWorld(int world_index);
bool EnqueueMoveEntity(EntityId entity_id, int source_world_index,
                       int destination_world_index, EntityId destination_parent_id,
                       Vector2d destination_coords, uint32_t original_collision_mask);
// Enqueue attaching a component to an existing entity.
bool EnqueueAttachComponent(EntityId entity_id, const EntityComponent *component);
// Enqueue detaching a component from an existing entity.
bool EnqueueRemoveComponent(EntityId entity_id, EntityComponentType component_type);
void ProcessCommandQueue(void);

#endif

