#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include "physics/newtonoid.h"
#include "system/systems.h"

typedef enum
{
    CMD_NONE = 0,
    CMD_CREATE_ENTITY = 1,
    CMD_DELETE_ENTITY = 2,
    CMD_CREATE_WORLD = 3,
    CMD_SELECT_WORLD = 4,
    CMD_MOVE_ENTITY = 5,
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
    CommandType type;
    union
    {
        Newtonoid2dParams create_entity;
        EntityId delete_entity;
        int world_select_delta;
        MoveEntityCommand move_entity;
    } data;
} Command;

void InitCommandQueue(void);
bool EnqueueCreateEntity(const Newtonoid2dParams *params);
bool EnqueueDeleteEntity(EntityId entity_id);
bool EnqueueCreateWorld(void);
bool EnqueueSelectWorld(int delta);
bool EnqueueMoveEntity(EntityId entity_id, int source_world_index,
                       int destination_world_index, EntityId destination_parent_id,
                       Vector2d destination_coords, uint32_t original_collision_mask);
void ProcessCommandQueue(void);

#endif

