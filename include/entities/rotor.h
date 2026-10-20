#ifndef ROTOR_ENTITY_H
#define ROTOR_ENTITY_H

#include "physics/newtonoid.h"

/**********************************************************************************************
 *
 * ROTOR COMPONENT
 *
 * Game-logic state for rotor entities. Rotation is stored in Newtonoid2d.angular_velocity.
 *
 **********************************************************************************************/

// Component data for rotor entities. Holds the resolved physics-flag policy so the
// attach/detach lifecycle hooks read a single resolved source rather than re-deriving.
typedef struct RotorComponent
{
    // --- Resolved physics-flag policy (opt-in; copied from RotorComponentParams) ---
    // The create/parse path defaults these to historic behaviour (lock_position=true,
    // auto_spin=true); the attach path may opt out to leave the body movable/unspun.
    bool lock_position;           // true: attach locks the body in place (POSITION_LOCKED).
    bool auto_spin;               // true: attach seeds angular_velocity when the body is still.
    float spin_rate;              // Auto-spin seed in rad/s; <= 0 means "use the historic 2.0f default".
} RotorComponent;

// Configurable rotor component values supplied by a prefab or caller.
typedef struct RotorComponentParams
{
    // --- Opt-in physics-flag policy (default to historic behaviour on the create path) ---
    // The create/parse path must set lock_position=true and auto_spin=true explicitly (C
    // zero-initialises omitted fields to false), preserving today's locked+spinning rotor.
    bool lock_position;           // true: attaching the rotor locks the body's position.
    bool auto_spin;               // true: attaching seeds angular_velocity when the body is still.
    float spin_rate;              // Auto-spin seed in rad/s; <= 0 means "use the historic 2.0f default".
} RotorComponentParams;

// Create an allocated rotor entity from the supplied creation parameters.
Newtonoid2d *RotorEntity_Create(const Newtonoid2dParams *params);

// Initialise rotor component state from caller/prefab-configurable values. Seeds the
// resolved physics-flag policy (position lock + auto-spin) so the attach lifecycle hook
// and detach release rule read a single resolved source. Mirrors GearComponent_Initialise.
bool RotorComponent_Initialise(RotorComponent *component, const RotorComponentParams *params);

#endif
