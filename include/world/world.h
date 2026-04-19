/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef WORLD_H
#define WORLD_H
#include "common/common.h"
#include "physics/polygonoid.h"
#include "math/coordinate_space.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct World2d {
    CoordSpace2d_Grid coord_space_grid; // The coordinate space of the world, containing the basis vectors and line segments for drawing the world (if applicable)
    DynamicArray objects;
    float gravity;
    int next_object_id; // Global variable to keep track of the next available ID for NewtonObjects
} World2d;


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
extern World2d world_1; 

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
World2d CreateWorld(CoordSpace2d_Grid space, DynamicArray objects, float gravity);
void UpdateWorld(World2d *world, float delta_time);;
void AddObjectToWorld(World2d *world, Polygonoid *object);
//Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
//Field UpdateFieldCellValues(Field field);

#endif