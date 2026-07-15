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
} CommandType;

typedef struct
{
    CommandType type;
    Newtonoid2dParams params; // used for create-entity
    int world_select_delta;   // used for world selection commands
} Command;

void InitCommandQueue(void);
bool EnqueueCreateEntity(const Newtonoid2dParams *params);
bool EnqueueDeleteEntity(Newtonoid2d *obj);
bool EnqueueCreateWorld(void);
bool EnqueueSelectWorld(int delta);
void ProcessCommandQueue(void);

#endif

