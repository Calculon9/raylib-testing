/**********************************************************************************************
 *
 * PROJECTILE MODULE
 *
 **********************************************************************************************/
#include "combat/projectile.h"
#include "common/common.h"
#include "world/world.h"
#include "world/world_internal.h"

#define PROJECTILE_DEFAULT_RADIUS 0.12f
#define PROJECTILE_DEFAULT_MASS 0.01f
#define PROJECTILE_DEFAULT_LIFETIME_FRAMES 120
#define PROJECTILE_MUZZLE_GAP 0.15f

static const ProjectileDefinition projectile_definitions[PROJECTILE_TYPE_COUNT] = {
    [PROJECTILE_TYPE_BOLT] = {
        .shape_type = SHAPE_TRIANGLE_EQUILATERAL,
        .primitive_params = {
            .radius = PROJECTILE_DEFAULT_RADIUS,
            .colour = COLOUR_GAME_TERRACOTTA_RGBA},
        .mass = PROJECTILE_DEFAULT_MASS,
        .speed = 8.0f,
        .damage = 1.0f,
        .lifetime_frames = PROJECTILE_DEFAULT_LIFETIME_FRAMES,
        .collision_mask = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID,
        .attribute_flags = ENTITY_ATTR_FLAG_NONE,
        .line_colour = COLOUR_GAME_INK_RGBA,
        .fill_colour = COLOUR_GAME_TERRACOTTA_RGBA}};

// Return whether an entity has the projectile type flag.
bool IsProjectile(const Newtonoid2d *entity)
{
    return entity && (entity->entity_flags & FLAG_TYPE_PROJECTILE) != 0;
}

// Return the immutable definition for a projectile type, or NULL when invalid.
const ProjectileDefinition *Projectile_GetDefinition(ProjectileType type)
{
    if (type < 0 || type >= PROJECTILE_TYPE_COUNT)
    {
        return NULL;
    }

    return &projectile_definitions[type];
}

// Mark a projectile inactive and schedule safe removal after collision processing completes.
static void ConsumeProjectile(World2d *world, Newtonoid2d *projectile)
{
    if (!world || !projectile || !(projectile->status_flags & FLAG_STATUS_ALIVE))
    {
        return;
    }

    projectile->status_flags &= ~FLAG_STATUS_ALIVE;
    ScheduleEntityDeletion(&world->scheduled_world_cmds, projectile->id,
                           0, 0, 1, 1);
}

// Return whether a projectile is allowed to affect the entity that owns it.
static bool ProjectileCanAffectTarget(const Newtonoid2d *projectile, const Newtonoid2d *target)
{
    if (!projectile || !target)
    {
        return false;
    }

    return projectile->owner_id != target->id ||
           (projectile->attribute_flags & FLAG_ATTR_AFFECT_OWNER) != 0;
}

// Create a projectile Newtonoid, register it with the world, and schedule its expiry.
EntityId SpawnProjectile(World2d *world, const ProjectileSpawnParams *params)
{
    if (!world || !params)
    {
        return INVALID_ENTITY_ID;
    }

    const ProjectileDefinition *definition = Projectile_GetDefinition(params->type);
    if (!definition)
    {
        LOG_WARN("Cannot spawn projectile: invalid projectile type %d.\n", params->type);
        return INVALID_ENTITY_ID;
    }

    Vector2d direction = VectorNormalize_2d(params->direction);
    if (VectorMagnitude_2d(direction) <= 0.0001f)
    {
        LOG_WARN("Cannot spawn projectile without a direction.\n");
        return INVALID_ENTITY_ID;
    }

    Newtonoid2d projectile = CreateNewtonoid2d_Primitive(
        definition->shape_type, definition->primitive_params,
        definition->mass, params->position,
        VectorScale_2d(direction, definition->speed), ZERO_VECTOR_2D);
    if (!projectile.surface.surface_vectors.items || projectile.surface.surface_vectors.count < 3)
    {
        LOG_WARN("Cannot spawn projectile: projectile geometry could not be created.\n");
        ClearLArray(&projectile.surface.surface_vectors);
        return INVALID_ENTITY_ID;
    }

    projectile.owner_id = params->owner_id;
    projectile.damage = definition->damage;
    projectile.rotation = VectorRadians_2d(direction);
    SyncNewtonoidRotation(&projectile);
    Newtonoid_ConfigureMetadata(&projectile, FLAG_TYPE_PROJECTILE,
                                definition->collision_mask,
                                definition->attribute_flags | FLAG_ATTR_VELOCITY_ALIGNED,
                                FLAG_STATUS_ALIVE | FLAG_LIFETIME_CLOCKED,
                                definition->line_colour, definition->fill_colour);

    EntityId projectile_id = AddObjectToWorld(world, &projectile, world->grid_space.object.id);
    if (projectile_id == INVALID_ENTITY_ID)
    {
        ClearLArray(&projectile.surface.surface_vectors);
        return INVALID_ENTITY_ID;
    }

    ScheduleEntityDeletion(&world->scheduled_world_cmds, projectile_id, 0,
                           definition->lifetime_frames, 1, 1);
    LOG_INFO("Spawned projectile id=%d type=%d owner=%d world=%p\n",
             projectile_id, params->type, params->owner_id, (void *)world);
    return projectile_id;
}

// Fire a projectile from the shooter's forward axis with a small muzzle offset.
EntityId FireProjectile(World2d *world, const Newtonoid2d *shooter,
                        ProjectileType type)
{
    if (!world || !shooter)
    {
        return INVALID_ENTITY_ID;
    }

    Vector2d direction = VectorNormalize_2d(shooter->local_axis_x);
    if (VectorMagnitude_2d(direction) <= 0.0001f)
    {
        direction = VectorNormalize_2d(shooter->velocity);
    }
    if (VectorMagnitude_2d(direction) <= 0.0001f)
    {
        LOG_WARN("Cannot fire projectile from entity %d without a forward direction.\n", shooter->id);
        return INVALID_ENTITY_ID;
    }

    float shooter_radius = fmaxf(shooter->bounds_size.x, shooter->bounds_size.y) * 0.5f;
    Vector2d muzzle_offset = VectorScale_2d(direction, shooter_radius + PROJECTILE_MUZZLE_GAP);
    ProjectileSpawnParams params = {
        .type = type,
        .owner_id = shooter->id,
        .position = VectorSum_2d(shooter->anchor_position, muzzle_offset),
        .direction = direction};
    return SpawnProjectile(world, &params);
}

// Consume projectiles on valid contacts, applying damage to non-owner targets first.
ProjectileCollisionResult Projectile_HandleCollision(World2d *world, Newtonoid2d *first, Newtonoid2d *second)
{
    if (!world || !first || !second || (!IsProjectile(first) && !IsProjectile(second)))
    {
        return PROJECTILE_COLLISION_NONE;
    }

    bool consumed_projectile = false;
    if (IsProjectile(first))
    {
        if (ProjectileCanAffectTarget(first, second))
        {
            if (IsDamageable(second) && ApplyEntityDamage(second, first->damage))
            {
                ScheduleEntityDeletion(&world->scheduled_world_cmds, second->id,
                                       0, 0, 1, 1);
            }
            ConsumeProjectile(world, first);
            consumed_projectile = true;
        }
    }
    if (IsProjectile(second))
    {
        if (ProjectileCanAffectTarget(second, first))
        {
            if (IsDamageable(first) && ApplyEntityDamage(first, second->damage))
            {
                ScheduleEntityDeletion(&world->scheduled_world_cmds, first->id,
                                       0, 0, 1, 1);
            }
            ConsumeProjectile(world, second);
            consumed_projectile = true;
        }
    }

    return consumed_projectile ? PROJECTILE_COLLISION_CONSUMED : PROJECTILE_COLLISION_IGNORED;
}
