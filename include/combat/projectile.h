/**********************************************************************************************
 *
 * PROJECTILE MODULE
 *
 **********************************************************************************************/
#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "physics/newtonoid.h"

typedef struct World2d World2d;

typedef enum ProjectileType
{
    PROJECTILE_TYPE_BOLT,
    PROJECTILE_TYPE_COUNT
} ProjectileType;

typedef struct ProjectileDefinition
{
    ShapeType shape_type;
    NewtonoidPrimitiveParams primitive_params;
    float mass;
    float speed;
    float damage;
    int lifetime_frames;
    EntityFlags collision_mask;
    EntityAttributeFlags attribute_flags;
    ColourRgba line_colour;
    ColourRgba fill_colour;
} ProjectileDefinition;

typedef struct ProjectileSpawnParams
{
    ProjectileType type;
    EntityId owner_id;
    Vector2d position;
    Vector2d direction;
} ProjectileSpawnParams;

typedef enum ProjectileCollisionResult
{
    PROJECTILE_COLLISION_NONE = 0,
    PROJECTILE_COLLISION_IGNORED,
    PROJECTILE_COLLISION_CONSUMED
} ProjectileCollisionResult;

// Return whether an entity is configured as a projectile.
bool IsProjectile(const Newtonoid2d *entity);

// Return the immutable definition for a projectile type, or NULL when invalid.
const ProjectileDefinition *Projectile_GetDefinition(ProjectileType type);

// Create and register a projectile in the supplied world.
EntityId SpawnProjectile(World2d *world, const ProjectileSpawnParams *params);

// Fire a projectile type along the shooter's forward axis.
EntityId FireProjectile(World2d *world, const Newtonoid2d *shooter, ProjectileType type);

// Apply projectile-specific collision behaviour and report the response type.
ProjectileCollisionResult Projectile_HandleCollision(World2d *world,Newtonoid2d *first, Newtonoid2d *second);

#endif
