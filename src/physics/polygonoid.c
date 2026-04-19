/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/
#include "common/common.h"
#include "physics/circloid.h"
#include "physics/polygonoid.h"
#include "physics/newton_object.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// Circloid CreatePolygonoid(Surface2d surface, ColourRgba colour, size_t mass, Vector2d position, Velocity2d velocity, Acceleration2d acceleration);

Polygonoid CreatePolygonoid_Symmetric(int vertice_count, float radius, ColourRgba colour, size_t mass, Vector2d origin, Vector2d velocity, Vector2d acceleration)
{
   //
   Polygonoid newPol = {0};
   Surface2d surface = {0};
   DynamicArray *surface_vectors = GetPolygonoidSurfaceVectors_Symmetric(radius, vertice_count);
   surface.surface_vectors = *surface_vectors;
   NewtonObject2d newtOb = CreateNewtonObject2d(mass, origin, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newPol.newtonian_properties = newtOb;
   newPol.radius = radius;
   newPol.colourRgba = colour;

   return newPol;
}


DynamicArray *GetPolygonoidSurfaceVectors_Symmetric(float radius, int vertice_count)
{

   if (vertice_count < 0)
   {
      fprintf(stderr, "The provided number of contact vertices, %f, is less than 0. Continuing with 0 vertices instead so expect very inaccurate collisions.");
      vertice_count = 0;
   }
   DynamicArray *points = NewDynamicArray(vertice_count, sizeof(Vector2d));

   // Use radians to define the points as polygonoid edges that collectively will describe the shape's surface
   float angleStep = (2.0 * PI) / vertice_count;
   for (int i = 0; i < vertice_count; i++)
   {
      float currentAngle = i * angleStep;

      Vector2d p;
      // Origin position is world coordinates of the center of the polygonoid, so we can calculate the position of each vertex based on the radius and angle from the origin
      p.x = radius * cosf(currentAngle);
      p.y = radius * sinf(currentAngle);
      //   p.x = origin.x + radius * cosf(currentAngle);
      //   p.y = origin.y + radius * sinf(currentAngle);

      Array_Push(points, &p);
   }

   return points;
}

// void Circloid_GetCollisionObjects(Circloid circloid)
// {

// }