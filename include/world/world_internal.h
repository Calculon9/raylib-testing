/**********************************************************************************************
*
* WORLD INTERNALS
*
**********************************************************************************************/
#ifndef WORLD_INTERNAL_H
#define WORLD_INTERNAL_H

#include "world/world.h"

// World lifecycle internals
void CreateWorld(GridSpace2d space_obj, float gravity, World2d *out_world);

// Entity registry / lookup helpers
void UpdateEntityWorldRegistry(FlatMapInt *entity_world_index_registry, int entity_id, int type_flag, int entity_arr_index);
int RegisterEntity(World2d *world, Newtonoid2d *entity);
void DeregisterEntity(World2d *world, int object_id);
void StickEntity(World2d *world, Newtonoid2d *child, Newtonoid2d *parent);
void SetObjectFlag(World2d *world, int object_id, int flag_to_update);
void ClearObjectFlag(World2d *world, int object_id, int flag_to_update);
void *GetEntityByID(World2d *world, int entity_id);

// Scheduled world command helpers
void ScheduleEntityFlagSet(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void ScheduleEntityFlagClear(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void ScheduleEntityDeletion(LArray *scheduled_events, int object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void RunScheduledWorldCmds(LArray *scheduled_cmds, World2d *world);

// Physics / collision helpers
void PhysicsUpdateJob(void *context, int start, int end);
void MapEntityToASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 snapped_aabb_box, FlatMapInt *O_entity_to_space_index_map);
CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b);
bool CheckForCollision_AABB(Newtonoid2d a, Newtonoid2d b);
void ResolveCollision(Newtonoid2d *a, Newtonoid2d *b);
void ResolveCollision_ContainerRect(Newtonoid2d *entity, Newtonoid2d *container);
void PrintVerticeCoords(LArray *vertices_arr, Vector2d offset);

#endif // WORLD_INTERNAL_H

