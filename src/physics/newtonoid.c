/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include "common/common.h"
#include "physics/newtonoid.h"
#include "math/cvectors.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
float CalculateInertia_Polygon(float mass, LArray *surface_vectors);

static bool InitializeNewtonoid2d(Newtonoid2d *newtonoid, float mass,
                                  Vector2d coords_center, Vector2d velocity,
                                  Vector2d acceleration, Surface2d surface)
{
   if (!newtonoid || surface.surface_vectors.count > MAX_SHAPE_VERTICES)
   {
      return false;
   }

   newtonoid->coords_center = coords_center;
   newtonoid->mass = mass;
   newtonoid->inverse_mass = 1.0f / mass;
   newtonoid->velocity = velocity;
   newtonoid->acceleration = acceleration;
   newtonoid->surface = surface;
   newtonoid->boxed_dimensions = CalcAABBDimensions(surface.surface_vectors.items, surface.surface_vectors.count);
   newtonoid->coords_origin = (Vector2d){newtonoid->coords_center.x - (newtonoid->boxed_dimensions.x / 2.0), newtonoid->coords_center.y - (newtonoid->boxed_dimensions.y / 2.0)};
   newtonoid->radius = (newtonoid->boxed_dimensions.x > newtonoid->boxed_dimensions.y) ? newtonoid->boxed_dimensions.x : newtonoid->boxed_dimensions.y;
   newtonoid->line_colour = COLOUR_LINE_DEFAULT;
   newtonoid->fill_colour = COLOUR_FILL_DEFAULT;
   newtonoid->inertia = CalculateInertia_Polygon(mass, &surface.surface_vectors);
   newtonoid->inverse_inertia = (newtonoid->inertia != 0.0f) ? (1.0f / newtonoid->inertia) : 0.0f;
   newtonoid->momentum.x = newtonoid->mass * newtonoid->velocity.x;
   newtonoid->momentum.y = newtonoid->mass * newtonoid->velocity.y;
   newtonoid->local_axis_x = (Vector2d){1.0f, 0.0f};
   newtonoid->local_axis_y = (Vector2d){0.0f, 0.0f};
   newtonoid->entity_flags = FLAG_TYPE_NEWTONOID;
   newtonoid->collision_mask = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID;
   newtonoid->status_flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
   return true;
}

static bool ValidateNewtonoidSurface(Surface2d surface)
{
   if (surface.surface_vectors.count <= MAX_SHAPE_VERTICES)
   {
      return true;
   }

   LOG_ERROR("Entity creation failed: vertex count %d exceeds MAX_SHAPE_VERTICES (%d)\n",
             surface.surface_vectors.count, MAX_SHAPE_VERTICES);
   return false;
}

Newtonoid2d CreateNewtonoid2d(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   Newtonoid2d newtonoid = {0};
   if (!ValidateNewtonoidSurface(surface) ||
       !InitializeNewtonoid2d(&newtonoid, mass, coords_center, velocity, acceleration, surface))
   {
      return newtonoid;
   }

   return newtonoid;
}

Newtonoid2d *CreateNewtonoid2d_Reference(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   if (!ValidateNewtonoidSurface(surface))
   {
      return NULL;
   }

   Newtonoid2d *newtOb = AllocateBytes(sizeof(Newtonoid2d));
   if (!newtOb)
   {
      LOG_ERROR("Failed to allocate memory for Newtonoid2d object.\n");
      return NULL;
   }

   InitializeNewtonoid2d(newtOb, mass, coords_center, velocity, acceleration, surface);
   return newtOb;
}

void CreateNewtonoid2d_Out(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface, Newtonoid2d *out_newtonoid)
{
   if (!out_newtonoid)
      return;

   if (!ValidateNewtonoidSurface(surface))
   {
      return;
   }

   InitializeNewtonoid2d(out_newtonoid, mass, coords_center, velocity, acceleration, surface);
}

// Creates an immobile, massless NewtonObject at the assigned world_position
Newtonoid2d CreateNewtonoid2d_Static(Vector2d coords_center, Surface2d surface)
{
   Newtonoid2d newtOb = {0};
   // Initialize the NewtonObject2d properties here (e.g., set world_position, velocity, etc.)
   newtOb.mass = 0.0;
   newtOb.inverse_mass = 0.0;
   newtOb.boxed_dimensions = CalcAABBDimensions(surface.surface_vectors.items, surface.surface_vectors.count);
   newtOb.coords_center = coords_center;
   newtOb.surface = surface;
   newtOb.coords_origin = (Vector2d){newtOb.coords_center.x - (newtOb.boxed_dimensions.x / 2.0), newtOb.coords_center.y - (newtOb.boxed_dimensions.y / 2.0)};
   newtOb.radius = (newtOb.boxed_dimensions.x > newtOb.boxed_dimensions.y) ? newtOb.boxed_dimensions.x : newtOb.boxed_dimensions.y;
   newtOb.line_colour = COLOUR_LINE_DEFAULT;
   newtOb.fill_colour = COLOUR_FILL_DEFAULT;
   newtOb.inertia = 0.0f; // Infinite inertia for static objects
   newtOb.inverse_inertia = 0.0f;

   newtOb.torque = 0.0f;           // Initialize torque accumulator to zero
   newtOb.rotation = 0.0f;         // Initial rotation angle in radians
   newtOb.angular_velocity = 0.0f; // Initial angular velocity

   // Set default flags
   newtOb.entity_flags = FLAG_TYPE_NEWTONOID;
   newtOb.collision_mask = FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID;
   newtOb.status_flags = FLAG_ATTR_RIGID | FLAG_STATUS_ALIVE;
   return newtOb;
}

Newtonoid2d CreateNewtonoid2d_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
   Surface2d surface = {0};
   LArray surface_vectors = CreateVertices_Symmetric(vertice_count, radius, radius);
   surface.surface_vectors = surface_vectors;
   Newtonoid2d newtOb = CreateNewtonoid2d(mass, coords_center, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newtOb.radius = radius;
   newtOb.line_colour = colour;
   newtOb.fill_colour = colour;

   return newtOb;
}

Newtonoid2d CreateNewtonoid2d_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration)
{
   Surface2d surface = {0};
   LArray surface_vectors = CreateVertices_Irregular(vertice_count, min_radius, max_radius);
   surface.surface_vectors = surface_vectors;
   Newtonoid2d newtOb = CreateNewtonoid2d(mass, coords_center, velocity, acceleration, surface);

   // Initialize the NewtonObject2d properties here (e.g., set position, velocity, etc.)
   newtOb.line_colour = colour;
   newtOb.fill_colour = colour;
   newtOb.radius = max_radius;

   return newtOb;
}

void CalcVectors(Newtonoid2d *object, float deltaTime)
{
   // 1. Dynamic Mass/Inertia Safety Pass
   // Recalc inverses up front in case gameplay code mutated mass or bounds this frame
   if (object->mass != 0.0f)
   {
      object->inverse_mass = 1.0f / object->mass;

      // If mass changed, re-evaluate box inertia baseline
      // I = (1/12) * m * (w^2 + h^2)
      object->inertia = (1.0f / 12.0f) * object->mass * (object->boxed_dimensions.x * object->boxed_dimensions.x + object->boxed_dimensions.y * object->boxed_dimensions.y);
      object->inverse_inertia = 1.0f / object->inertia;
   }
   else
   {
      object->inverse_mass = 0.0f;
      object->inertia = 0.0f;
      object->inverse_inertia = 0.0f; // Infinite resistance to rotation
   }

   // 2. Linear Kinematics (Linear Integration)
   Vector2d displacement;
   displacement.x = (object->velocity.x * deltaTime) + (0.5f * object->acceleration.x * deltaTime * deltaTime);
   displacement.y = (object->velocity.y * deltaTime) + (0.5f * object->acceleration.y * deltaTime * deltaTime);

   // Displace tracking origins
   object->coords_origin = VectorSum_2d(object->coords_origin, displacement);
   object->coords_center = VectorSum_2d(object->coords_center, displacement);

   // Update linear velocity and state vectors for next frame
   object->velocity.x += object->acceleration.x * deltaTime;
   object->velocity.y += object->acceleration.y * deltaTime;

   object->momentum.x = object->mass * object->velocity.x;
   object->momentum.y = object->mass * object->velocity.y;

   // 3. Angular Kinematics (Rotational Integration)
   float angular_acceleration = object->torque * object->inverse_inertia;

   // Update raw rotation angle scalar and spin speed
   object->rotation += (object->angular_velocity * deltaTime) + (0.5f * angular_acceleration * deltaTime * deltaTime);
   object->angular_velocity += angular_acceleration * deltaTime;

   // 4. Matrix Sync Pass: Re-bake local coordinate framework
   object->local_axis_x.x = cosf(object->rotation);
   object->local_axis_x.y = sinf(object->rotation);
   object->local_axis_y.x = -object->local_axis_x.y;
   object->local_axis_y.y = object->local_axis_x.x;

   // Reset accumulation registers for forces/forces of rotation
   object->torque = 0.0f;
}

float CalculateInertia_Polygon(float mass, LArray *surface_vectors)
{
   if (mass <= 0.0f || surface_vectors->count < 3)
      return 0.0f;

   float total_accumulated_numerator = 0.0f;
   float total_accumulated_denominator = 0.0f;
   int num_vertices = surface_vectors->count;
   Vector2d *verts = (Vector2d *)surface_vectors->items;

   for (int i = 0; i < num_vertices; i++)
   {
      // Get current vertex and wrap around to the next one to close the edge loop
      Vector2d p1 = verts[i];
      Vector2d p2 = verts[(i + 1) % num_vertices];

      // 2D Cross Product acts as the area scalar component (parallelogram area)
      float cross_product_area = fabsf(p1.x * p2.y - p2.x * p1.y);

      // Calculate the geometry distribution factor for this triangle wedge
      float geometry_factor = (VectorDot_2d(p1, p1) + VectorDot_2d(p1, p2) + VectorDot_2d(p2, p2));

      total_accumulated_numerator += cross_product_area * geometry_factor;
      total_accumulated_denominator += cross_product_area;
   }

   // Combine components to yield the final absolute structural mass moment scalar
   float structural_inertia = total_accumulated_numerator / (6.0f * total_accumulated_denominator);

   // Scale it uniformly by the actual physical mass of the object
   return mass * structural_inertia;
}

Vector2d RotateVertex(Vector2d local_vertex, Vector2d local_axis)
{
   Vector2d rotated;
   // Standard 2D Rotation Matrix layout using our pre-computed local_axis vector:
   // cos(theta) is local_axis.x, sin(theta) is local_axis.y
   rotated.x = local_vertex.x * local_axis.x - local_vertex.y * local_axis.y;
   rotated.y = local_vertex.x * local_axis.y + local_vertex.y * local_axis.x;
   return rotated;
}

// Vector2d CalculateCenterRelativeToOrigin_Fast(NewtonObject2d *object)
// {
//    // Update velocity based on acceleration and time
//    Collection *points = &object->surface.surface_vectors;
// }

// Returns the boxed coords from a collection of vertice vectors (must all be relative to the associated object's coords)
// Matrix2x2 FindBoxedCoords(DArray vertices)
// {
//    Matrix2x2 box_coords = {0};
//    if (vertices.count < 2)
//    {
//       return box_coords;
//    }
//    Vector2d *pts = vertices.items;

//    // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
//    box_coords.col1 = pts[0];
//    box_coords.col2 = pts[0];
//    Vector2d vertice = {0};
//    for (size_t i = 1; i < vertices.count; i++)
//    {
//       vertice = pts[i];

//       box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
//       box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

//       box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
//       box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);

//       // // Check if x is a min or max
//       // if (vertice.x > box_coords.col2.x)
//       // {
//       //    box_coords.col2.x = vertice.x;
//       // }
//       // else if (vertice.x < box_coords.col1.x)
//       // {
//       //    box_coords.col1.x = vertice.x;
//       // }

//       // // Check if y is a min or max
//       // if (vertice.y > box_coords.col2.y)
//       // {
//       //    box_coords.col2.y = vertice.y;
//       // }
//       // else if (vertice.y < box_coords.col1.y)
//       // {
//       //    box_coords.col1.y = vertice.y;
//       // }
//    }
//    return box_coords;
// }

