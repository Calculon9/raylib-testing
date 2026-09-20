#ifndef GEAR_ENTITY_H
#define GEAR_ENTITY_H

#include "physics/newtonoid.h"

/**********************************************************************************************
 *
 * GEAR COMPONENT
 *
 * Game-logic state for gear entities. Rotation is stored in Newtonoid2d.angular_velocity.
 *
 **********************************************************************************************/

// Component data for gear entities; currently placeholder for future behaviour flags or meshing logic.
typedef struct GearComponent
{
    // Reserved storage keeps the placeholder component representable in packed stores.
    unsigned char reserved;
} GearComponent;

// Create an allocated gear entity from the supplied creation parameters.
Newtonoid2d *GearEntity_Create(const Newtonoid2dParams *params);

#endif
