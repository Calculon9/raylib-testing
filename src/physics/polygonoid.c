/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "physics/polygonoid.h"
#include "physics/newton_object.h"
#include "math/geometry.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// Circloid CreatePolygonoid(Surface2d surface, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration);

Polygonoid CreatePolygonoid_Symmetric(int vertice_count, float radius, ColourRgba colour, size_t mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
   Polygonoid newPol = {0};
   Surface2d surface = {0};
   LArray surface_vectors = CreateVertices_Symmetric(vertice_count, radius);
   surface.surface_vectors = surface_vectors;
   NewtonObject2d newtOb = CreateNewtonObject2d(mass, coords_center, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newPol.newtonian_properties = newtOb;
   newPol.radius = radius;
   newPol.colourRgba = colour;

   return newPol;
}

Polygonoid CreatePolygonoid_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, size_t mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
   Polygonoid newPol = {0};
   Surface2d surface = {0};
   LArray surface_vectors = CreateVertices_Irregular(vertice_count, min_radius, max_radius);
   surface.surface_vectors = surface_vectors;
   NewtonObject2d newtOb = CreateNewtonObject2d(mass, coords_center, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newPol.newtonian_properties = newtOb;
   newPol.radius = max_radius;
   newPol.colourRgba = colour;

   return newPol;
}

// void Circloid_GetCollisionObjects(Circloid circloid)
// {

// }