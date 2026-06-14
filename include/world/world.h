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
typedef struct World2d
{
    CoordSpace2d_Grid coord_space_grid; // The coordinate space of the world, containing the basis vectors and line segments for drawing the world (if applicable)
    LArray objects;
    LArray collisions;
    float gravity;
    int next_object_id; // Global variable to keep track of the next available ID for NewtonObjects
} World2d;

typedef struct AxisIntersectionRange2d
{
    Vector2d start; // The 2D world coordinate where the overlap begins
    Vector2d end;   // The 2D world coordinate where the overlap ends
} AxisIntersectionRange2d;

typedef struct CollisionResult_SAT
{
    Polar2d u_unit_axis;                     // normalized vector representing one of the potential separating axes (the "u" axis of entity A)
    Polar2d v_unit_axis;                     // normalized vector representing one of the potential separating axes (the "v" axis of entity A)
    Polar2d separating_unit_axis;            // either u_unit_axis or v_unit_axis, depending on which has the least penetration (smallest overlap distance)
    Matrix2x2 collision_box;                 // The collision box of the two objects
    float penetration_depth;                 // How much the objects are overlapping along the separating_unit_axis (the smaller this value, the less deep the collision is, and the easier it will be to resolve)
    AxisIntersectionRange2d overlap_range_a; // The range along the separating axis where the vertices of object A are located (the "shadow" of object A on the separating axis)
    AxisIntersectionRange2d overlap_range_b; // The range along the separating axis where the vertices of object B are located (the "shadow" of object B on the separating axis)
    NewtonObject2d *entity_a;
    NewtonObject2d *entity_b;
    bool is_colliding;
} CollisionResult_SAT;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
extern World2d world_1;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
World2d CreateWorld(CoordSpace2d_Grid space, float gravity);
void UpdateWorld(World2d *world, float delta_time);
void AddObjectToWorld(World2d *world, Polygonoid *object, int parent_id);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif