/**********************************************************************************************
 *
 * WORLD INTERNALS
 *
 **********************************************************************************************/
#ifndef WORLD_INTERNAL_H
#define WORLD_INTERNAL_H

#include "world/world.h"

// World lifecycle internals
bool CreateWorld(GridSpace2d space_obj, float gravity, struct Universe *universe, World2d *out_world);
// Release all allocations owned by a world and clear its state.
void DestroyWorld(World2d *world);
void BindWorldTunnel(World2d *world, Camera2d *camera);
void World_SetBounds(World2d *world, Vector2d min_bound, Vector2d max_bound);
void World_RefreshBoundsFromFrame(World2d *world);
Matrix3x3 ResolveWorldToPixelMatrix(const World2d *world, const Camera2d *universe_camera);
Matrix3x3 ResolvePixelToWorldMatrix(const World2d *world, const Camera2d *universe_camera);
Vector2d ResolvePixelToWorldFrame(const World2d *active_world, Vector2d pixel_coords);

// Entity registry / lookup helpers
// Release entity-owned nested surface data and entity arrays for a world.
void DestroyWorldEntityStorage(World2d *world);
void UpdateEntityWorldRegistry(FlatMapInt *entity_world_index_registry, EntityId entity_id, int type_flag, int entity_arr_index);
EntityId RegisterEntity(World2d *world, Newtonoid2d *entity);
void DeregisterEntity(World2d *world, EntityId object_id);
void StickEntity(World2d *world, Newtonoid2d *child, Newtonoid2d *parent);
void SetObjectFlag(World2d *world, EntityId object_id, int flag_to_update);
void ClearObjectFlag(World2d *world, EntityId object_id, int flag_to_update);
void *GetEntityByID(World2d *world, EntityId entity_id);
Newtonoid2d *ResolveClosestEntityAt(World2d *active_world, Vector2d click_local_coords,
                                    Cell **out_cell, int *out_cell_index,
                                    char *log, size_t log_size, int *log_offset);

// Scheduled world command helpers
void ScheduleEntityFlagSet(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void ScheduleEntityFlagClear(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void ScheduleEntityDeletion(LArray *scheduled_events, EntityId object_id, int flag_to_set, int initial_frame_delay, int interval_frames, int run_limit);
void RunScheduledWorldCmds(LArray *scheduled_cmds, World2d *world);

// Physics / collision helpers
// Return whether an entity is eligible for broad-phase spatial-map occupancy.
bool EntityIsEligbleForSpatialMap(const Newtonoid2d *entity);
void PhysicsUpdateJob(void *context, int start, int end);
void MapEntityToASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 snapped_aabb_box, FlatMapInt *O_entity_to_space_index_map);
void RemapEntityInASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 previous_snapped_aabb_box, FlatMapInt *entity_space_map);
void RefreshWorldSpatialMap(World2d *world);
CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b);
bool CheckForCollision_AABB(Newtonoid2d a, Newtonoid2d b);
void ResolveCollision(Newtonoid2d *a, Newtonoid2d *b);
void ResolveCollision_WithRotation(Newtonoid2d *a, Newtonoid2d *b, Vector2d collision_normal,
                                   Vector2d contact_point, float penetration_depth);
void ResolveCollision_ContainerRect(Newtonoid2d *entity, Newtonoid2d *container);
bool ProcessCollisionPair(World2d *world, EntityId obj_id_a, EntityId obj_id_b, int cell_i, FlatMapInt *resolved_collisions, LArray *scheduled_world_cmds);

#endif // WORLD_INTERNAL_H
