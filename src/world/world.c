/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "world/world.h"
#include "physics/rectangloid.h"
#include "physics/field.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//Physical state variables
float gravity = 9.8f; // Gravitational acceleration (m/s^2)
float field = {0};


//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
World2d CreateWorld(CoordSpace2d_Grid space_obj, DynamicArray objects, float gravity)
{
   World2d world = {0};
   world.coord_space_grid = space_obj;
   world.objects = objects;
   world.gravity = gravity;

   return world;
}

void UpdateWorld(World2d *world) 
{

}
//World CalculateFieldLines(Field field);
//World InitialiseFieldCells(Field field);

