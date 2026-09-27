#ifndef HEALTH_COMPONENT_H
#define HEALTH_COMPONENT_H

#include <stdbool.h>

#include "physics/newtonoid.h"

typedef struct HealthComponent
{
    float current_health;
    float max_health;
} HealthComponent;

typedef struct HealthComponentParams
{
    float max_health;
} HealthComponentParams;

// Initialise health to the configured maximum.
bool HealthComponent_Initialise(HealthComponent *component, const HealthComponentParams *params);

// Return whether health values are finite and within their valid range.
bool HealthComponent_IsValid(const HealthComponent *component);

// Subtract damage and return true when the component reaches zero health.
bool HealthComponent_ApplyDamage(HealthComponent *component, float damage);

// Return whether an entity has health and is currently eligible to take damage.
bool IsDamageable(const Newtonoid2d *entity);

// Apply damage to an entity and return true when it is defeated.
bool ApplyEntityDamage(Newtonoid2d *entity, float damage);

#endif