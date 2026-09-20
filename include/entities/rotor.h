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

// Component data for rotor entities; currently placeholder for future behaviour flags.
typedef struct RotorComponent
{
    // Reserved storage keeps the placeholder component representable in packed stores.
    unsigned char reserved;
} RotorComponent;

// Create an allocated rotor entity from the supplied creation parameters.
Newtonoid2d *RotorEntity_Create(const Newtonoid2dParams *params);

#endif
