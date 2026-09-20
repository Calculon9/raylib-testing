/**********************************************************************************************
 *
 * GEAR SYSTEM MODULE
 *
 **********************************************************************************************/
#include "system/entities/gear_system.h"
#include "entities/entity_registry.h"
#include "world/world.h"

// Update all gear entities in a world each frame. Currently handles rotation via
// generic Newtonoid2d physics updates. Extensible for future gear meshing and constraints.
void GearSystem_Update(World2d *world)
{
    if (!world)
    {
        return;
    }

    // Iterate all objects in the world and find gears by component query.
    Newtonoid2d *objects = (Newtonoid2d *)world->objects.items;
    for (int object_index = 0; object_index < world->objects.count; object_index++)
    {
        Newtonoid2d *object = &objects[object_index];
        GearComponent *gear = EntityRegistry_GetGear(object->id);
        if (!gear)
        {
            continue;
        }

        // Gear-specific behaviour can be added here.
        // Currently, rotation is handled by generic physics updates in PhysicsUpdateJob.
        // Future: add meshing detection, gear ratio synchronisation, or constraint coupling.
    }
}
