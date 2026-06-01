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

Polygonoid CreatePolygonoid_Symmetric(int vertice_count, float radius, ColourRgba colour, size_t mass, Vector2d top_left, Vector2d velocity, Vector2d acceleration)
{
   Polygonoid newPol = {0};
   Surface2d surface = {0};
   LArray surface_vectors = GetPolygonoidSurfaceVectors_Symmetric(radius, vertice_count);
   surface.surface_vectors = surface_vectors;
   NewtonObject2d newtOb = CreateNewtonObject2d(mass, top_left, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newPol.newtonian_properties = newtOb;
   newPol.radius = radius;
   newPol.colourRgba = colour;

   return newPol;
}

LArray GetPolygonoidSurfaceVectors_Symmetric(float radius, int vertice_count)
{
   if (vertice_count < 0)
   {
      fprintf(stderr, "The provided number of contact vertices, %d, is less than 0. Returning an empty surface.\n", vertice_count);
      return MakeLArray(0, sizeof(Vector2d));
   }
   LArray points = MakeLArray(vertice_count, sizeof(Vector2d));
   float angleStep = (2.0f * PI) / vertice_count;

   char log_buffer[512] = {0};
   int log_offset = 0;

   for (int i = 0; i < vertice_count; i++)
   {
      float currentAngle = i * angleStep;
      Vector2d p;
      p.x = radius * cosf(currentAngle);
      p.y = radius * sinf(currentAngle);
      printf("Generated vertice %d: Angle = %.3f\n", i, currentAngle);
      LArray_Push(&points, &p);
      if (log_offset < (int)sizeof(log_buffer) - 30) 
      {
         log_offset += snprintf(log_buffer + log_offset, sizeof(log_buffer) - log_offset, " (%.2f, %.2f)", ((Vector2d *)points.items)[i].x, ((Vector2d *)points.items)[i].y);
      }
   }
   CenterVerticesToExtents(&points);
   printf("SURFACE CREATED:%s\n", log_buffer);
   return points;
}

// void Circloid_GetCollisionObjects(Circloid circloid)
// {

// }