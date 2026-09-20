/**********************************************************************************************
 *
 * ROTOR SYSTEM MODULE
 *
 * Governs rotor entity behaviour and updates. Rotors are entities with rotational motion
 * that are locked to a position and rotate continuously.
 *
 **********************************************************************************************/
#ifndef ROTOR_SYSTEM_H
#define ROTOR_SYSTEM_H

typedef struct World2d World2d;

// Update all rotor entities in a world each frame. Currently handles rotation via
// generic Newtonoid2d physics updates. Extensible for future constraints and interactions.
void RotorSystem_Update(World2d *world);

#endif
