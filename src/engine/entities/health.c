#include <math.h>

#include "entities/entity_registry.h"
#include "entities/health.h"

// Initialise health to the configured maximum.
bool HealthComponent_Initialise(HealthComponent *component, const HealthComponentParams *params)
{
    if (!component || !params || !isfinite(params->max_health) || params->max_health <= 0.0f)
    {
        return false;
    }

    *component = (HealthComponent){
        .current_health = params->max_health,
        .max_health = params->max_health};
    return true;
}

// Return whether health values are finite and within their valid range.
bool HealthComponent_IsValid(const HealthComponent *component)
{
    return component && isfinite(component->current_health) &&
           isfinite(component->max_health) && component->max_health > 0.0f &&
           component->current_health >= 0.0f &&
           component->current_health <= component->max_health;
}

// Subtract damage and return true when the component reaches zero health.
bool HealthComponent_ApplyDamage(HealthComponent *component, float damage)
{
    if (!HealthComponent_IsValid(component) || !isfinite(damage) || damage <= 0.0f)
    {
        return false;
    }

    component->current_health = damage >= component->current_health ? 0.0f : component->current_health - damage;
    return component->current_health == 0.0f;
}

// Return whether an entity has health and is currently eligible to take damage.
bool IsDamageable(const Newtonoid2d *entity)
{
    if (!entity || entity->id == INVALID_ENTITY_ID ||
        (entity->capabilities & ENTITY_CAPABILITY_DAMAGEABLE) == 0 ||
        (entity->status_flags & ENTITY_STATUS_FLAG_ALIVE) == 0)
    {
        return false;
    }

    const HealthComponent *health = EntityRegistry_GetHealth(entity->id);
    return HealthComponent_IsValid(health) && health->current_health > 0.0f;
}

// Apply damage to an entity and return true when it is defeated.
bool ApplyEntityDamage(Newtonoid2d *entity, float damage)
{
    if (!IsDamageable(entity))
    {
        return false;
    }

    HealthComponent *health = EntityRegistry_GetHealth(entity->id);
    if (!HealthComponent_ApplyDamage(health, damage))
    {
        return false;
    }

    entity->status_flags &= ~ENTITY_STATUS_FLAG_ALIVE;
    return true;
}