#ifndef ENTITY_FLAGS_H
#define ENTITY_FLAGS_H

// Roles classify entities; collision masks use the same role bitset.
typedef enum EntityRoleFlags
{
    ENTITY_ROLE_NONE = 0,
    ENTITY_ROLE_WALL = 1 << 1,
    ENTITY_ROLE_NEWTONOID = 1 << 2,
    ENTITY_ROLE_PROJECTILE = 1 << 3,
    ENTITY_ROLE_EFFECT = 1 << 4,
    ENTITY_ROLE_CAMERA = 1 << 5,
} EntityRoleFlags;

// Default collision mask for standard entities and solid world boundaries.
#define ENTITY_DEFAULT_COLLISION_ROLE_MASK \
    (ENTITY_ROLE_WALL | ENTITY_ROLE_NEWTONOID | ENTITY_ROLE_PROJECTILE)

// Entity runtime state such as alive, sleeping, or clocked.
typedef enum EntityStatusFlags
{
    ENTITY_STATUS_FLAG_NONE = 0,
    ENTITY_STATUS_FLAG_ALIVE = 1 << 0,
    ENTITY_STATUS_FLAG_SLEEPING = 1 << 1,
    ENTITY_STATUS_FLAG_CLOCKED = 1 << 6,
} EntityStatusFlags;

// Capabilities describe gameplay behaviours an entity supports.
typedef enum EntityCapabilityFlags
{
    ENTITY_CAPABILITY_NONE = 0,
    ENTITY_CAPABILITY_DAMAGEABLE = 1 << 0,
    ENTITY_CAPABILITY_VELOCITY_ALIGNED = 1 << 1,
    ENTITY_CAPABILITY_AFFECT_OWNER = 1 << 2,
    ENTITY_CAPABILITY_SENSOR = 1 << 7,
} EntityCapabilityFlags;

// Constraints restrict how an entity participates in physics and contact response.
typedef enum EntityConstraintFlags
{
    ENTITY_CONSTRAINT_NONE = 0,
    ENTITY_CONSTRAINT_POSITION_LOCKED = 1 << 6,
    ENTITY_CONSTRAINT_NO_CONTACT_RESPONSE = 1 << 8,
} EntityConstraintFlags;

#endif