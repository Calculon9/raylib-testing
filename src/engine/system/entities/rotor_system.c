/**********************************************************************************************
 *
 * ROTOR SYSTEM MODULE
 *
 **********************************************************************************************/
#include "system/entities/rotor_system.h"
#include "entities/entity_registry.h"
#include "world/world.h"

// Update all rotor entities in a world each frame. Currently handles rotation via
// generic Newtonoid2d physics updates. Extensible for future constraints and interactions.
void RotorSystem_Update(World2d *world)
{
    if (!world)
    {
        return;
    }

    // Iterate all objects in the world and find rotors by component query.
    Newtonoid2d *objects = (Newtonoid2d *)world->objects.items;
    for (int object_index = 0; object_index < world->objects.count; object_index++)
    {
        Newtonoid2d *object = &objects[object_index];
        RotorComponent *rotor = EntityRegistry_GetRotor(object->id);
        if (!rotor)
        {
            continue;
        }

        // Rotor-specific behaviour can be added here.
        // Currently, rotation is handled by generic physics updates in PhysicsUpdateJob.
        // Future: add constraints, synchronisation points, or behaviour flags.
    }
}
