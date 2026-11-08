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

// Entity storage and lookup helpers
// Release entity-owned nested surface data and entity arrays for a world.
void DestroyWorldEntityStorage(World2d *world);
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
void MapEntityToASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 snapped_aabb_box, CellOccupancy *occupancy);
void RemapEntityInASpace(Space2d *space, Newtonoid2d *object, Matrix2x2 previous_snapped_aabb_box, CellOccupancy *occupancy);
void RefreshWorldSpatialMap(World2d *world);
CollisionResult_SAT CheckForCollision_SAT(Newtonoid2d *a, Newtonoid2d *b);
bool CheckForCollision_AABB(Newtonoid2d a, Newtonoid2d b);
// Find the vertex with the minimum (or maximum) projection along an axis.
Vector2d FindDeepestVertex(Vector2d *vertices, size_t count, Vector2d axis, bool find_minimum);
// Longest physics step (seconds) any world simulates in one frame. Clamping prevents entities
// tunnelling through one-cell walls during slow frames, at the cost of slow motion (simulation
// time falls behind wall-clock time) during hitches.
#define MAX_PHYSICS_DELTA_TIME (1.0f / 30.0f)
// DETECT half of a pair: gates, SAT, sensor/projectile callbacks, collision marker, and one cached contact.
bool DetectCollisionPair(World2d *world, EntityId obj_id_a, EntityId obj_id_b, int cell_i, FlatMapInt *resolved_collisions, LArray *scheduled_world_cmds);
// Combined material coefficients (geometric mean, clamped); shared by the pair, container and solid-cell detectors.
float CalcCollisionRestitution(const Newtonoid2d *a, const Newtonoid2d *b);
float CalcCollisionFriction(const Newtonoid2d *a, const Newtonoid2d *b);

// Contact solver glue (world_contacts.c)
// Write one cached pair contact from a SAT result (canonical slot order, bodies gathered on first touch).
void EmitPairContact(World2d *world, Newtonoid2d *a, Newtonoid2d *b, const CollisionResult_SAT *result);
// Cache one static contact (container wall or solid cell) for an entity: normal points from the entity into the wall.
// aux = wall id (container) or cell index (solid cell). Material comes from the world's container body.
void EmitStaticContact(World2d *world, Newtonoid2d *entity, ContactKind kind, uint32_t aux, Vector2d normal_to_wall,
                       Vector2d contact_point, float depth);
// Centre of the deepest support face of an entity against a wall (inward = wall to legal region).
Vector2d StaticContactPoint(const Newtonoid2d *entity, const Vector2d *vertices, size_t count, Vector2d inward, float depth);
// DETECT the container walls for every eligible world-owned entity (static contacts, entity order).
void DetectContainerContacts(World2d *world);
// Centre-only safety net after the solve: move any escaped centre back inside the grid. No impulse.
void ClampEntitiesToContainer(World2d *world);
// WAKE pass (between detection and the solve): sweeps the contact persistence set and wakes sleepers on
// impact, new contact, support lost and neighbour waking (world/sleep_wake.h), then detects the pairs of
// the bodies it woke that were carried instead of emitted. Runs every tick, also with no contacts.
void ResolveSleepWakes(World2d *world);
// SOLVE half of the frame: iterate the cached contacts and write the results back to the entities.
void SolveWorldContacts(World2d *world, float dt);
// Physics step used by the solver: the same clamped delta as PhysicsUpdateJob.
float WorldPhysicsDeltaTime(void);

#endif // WORLD_INTERNAL_H
