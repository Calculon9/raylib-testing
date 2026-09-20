/**********************************************************************************************
 *
 * GEAR SYSTEM MODULE
 *
 * Governs gear entity behaviour and updates. Gears are entities with rotational motion
 * that are locked to a position and rotate continuously.
 *
 **********************************************************************************************/
#ifndef GEAR_SYSTEM_H
#define GEAR_SYSTEM_H

typedef struct World2d World2d;

// Update all gear entities in a world each frame. Currently handles rotation via
// generic Newtonoid2d physics updates. Extensible for future gear meshing and constraints.
void GearSystem_Update(World2d *world);

#endif
