/**********************************************************************************************
 *
 * PORTAL MODULE
 *
 **********************************************************************************************/
#include "common/common.h"
// #include "world/world.h"
// #include "world/world_internal.h"

// #define PROJECTILE_DEFAULT_RADIUS 0.12f


// // Create a projectile Newtonoid, register it with the world, and schedule its expiry.
// EntityId SpawnProjectile(World2d *world, const ProjectileSpawnParams *params)
// {
//     if (!world || !params)
//     {
//         return INVALID_ENTITY_ID;
//     }

//     const ProjectileDefinition *definition = Projectile_GetDefinition(params->type);
//     if (!definition)
//     {
//         LOG_WARN("Cannot spawn projectile: invalid projectile type %d.\n", params->type);
//         return INVALID_ENTITY_ID;
//     }

//     // Normalise first so the configured speed is independent of the caller's
//     // input length: velocity = unit_direction * projectile_speed.
//     Vector2d direction = VectorNormalize_2d(params->direction);
//     if (VectorMagnitude_2d(direction) <= 0.0001f)
//     {
//         LOG_WARN("Cannot spawn projectile without a direction.\n");
//         return INVALID_ENTITY_ID;
//     }

//     Newtonoid2d projectile = CreateNewtonoid2d_Primitive(
//         definition->shape_type, definition->primitive_params,
//         definition->mass, params->position,
//         VectorScale_2d(direction, definition->speed), ZERO_VECTOR_2D);
//     if (!projectile.surface.surface_vectors.items || projectile.surface.surface_vectors.count < 3)
//     {
//         LOG_WARN("Cannot spawn projectile: projectile geometry could not be created.\n");
//         ClearLArray(&projectile.surface.surface_vectors);
//         return INVALID_ENTITY_ID;
//     }

//     projectile.owner_id = params->owner_id;
//     projectile.damage = definition->damage;
//     projectile.rotation = VectorRadians_2d(direction);
//     SyncNewtonoidRotation(&projectile);
//     Newtonoid_ConfigureMetadata(&projectile, ENTITY_FLAG_PROJECTILE,
//                                 definition->collision_mask,
//                                 definition->attribute_flags | ENTITY_ATTR_FLAG_VELOCITY_ALIGNED,
//                                 ENTITY_STATUS_FLAG_ALIVE | ENTITY_STATUS_FLAG_CLOCKED,
//                                 definition->line_colour, definition->fill_colour);

//     EntityId projectile_id = AddObjectToWorld(world, &projectile, world->grid_space.object.id);
//     if (projectile_id == INVALID_ENTITY_ID)
//     {
//         ClearLArray(&projectile.surface.surface_vectors);
//         return INVALID_ENTITY_ID;
//     }

//     ScheduleEntityDeletion(&world->scheduled_world_cmds, projectile_id, 0,
//                            definition->lifetime_frames, 1, 1);
//     LOG_INFO("Spawned projectile id=%d type=%d owner=%d world=%p\n",
//              projectile_id, params->type, params->owner_id, (void *)world);
//     return projectile_id;
// }

// // Consume projectiles on valid contacts, applying damage to non-owner targets first.
// ProjectileCollisionResult Projectile_HandleCollision(World2d *world, Newtonoid2d *first, Newtonoid2d *second)
// {
//     if (!world || !first || !second || (!IsProjectile(first) && !IsProjectile(second)))
//     {
//         return PROJECTILE_COLLISION_NONE;
//     }

//     bool consumed_projectile = false;
//     if (IsProjectile(first))
//     {
//         if (ProjectileCanAffectTarget(first, second))
//         {
//             if (IsDamageable(second) && ApplyEntityDamage(second, first->damage))
//             {
//                 ScheduleEntityDeletion(&world->scheduled_world_cmds, second->id,
//                                        0, 0, 1, 1);
//             }
//             ConsumeProjectile(world, first);
//             consumed_projectile = true;
//         }
//     }
//     if (IsProjectile(second))
//     {
//         if (ProjectileCanAffectTarget(second, first))
//         {
//             if (IsDamageable(first) && ApplyEntityDamage(first, second->damage))
//             {
//                 ScheduleEntityDeletion(&world->scheduled_world_cmds, first->id,
//                                        0, 0, 1, 1);
//             }
//             ConsumeProjectile(world, second);
//             consumed_projectile = true;
//         }
//     }

//     return consumed_projectile ? PROJECTILE_COLLISION_CONSUMED : PROJECTILE_COLLISION_IGNORED;
// }
