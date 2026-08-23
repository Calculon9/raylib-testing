/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include "common/common.h"
#include "physics/newtonoid.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
float CalculateInertia_Polygon(float mass, LArray *surface_vectors);

// Configure the metadata shared by all Newtonoid creation paths.
void Newtonoid_ConfigureMetadata(Newtonoid2d *object, EntityFlags entity_flags,
                                 EntityFlags collision_mask, EntityAttributeFlags attribute_flags,
                                 EntityStatusFlags status_flags,
                                 ColourRgba line_colour, ColourRgba fill_colour)
{
   if (!object)
   {
      return;
   }

   object->entity_flags = entity_flags;
   object->collision_mask = collision_mask;
   object->attribute_flags = attribute_flags;
   object->status_flags = status_flags;
   object->line_colour = line_colour;
   object->fill_colour = fill_colour;
}

// Set an entity's maximum and current health.
void Newtonoid_ConfigureHealth(Newtonoid2d *object, float max_health)
{
   if (!object)
   {
      return;
   }

   object->max_health = max_health > 0.0f ? max_health : 0.0f;
   object->health = object->max_health;
}

// Return whether an entity is alive and configured to receive damage.
bool IsDamageable(const Newtonoid2d *entity)
{
   return entity && (entity->attribute_flags & FLAG_ATTR_DAMAGEABLE) != 0 &&
          entity->max_health > 0.0f && (entity->status_flags & FLAG_STATUS_ALIVE) != 0;
}

// Apply damage and clear the alive status when health reaches zero.
bool ApplyEntityDamage(Newtonoid2d *entity, float damage)
{
   if (!IsDamageable(entity) || damage <= 0.0f)
   {
      return false;
   }

   entity->health -= damage;
   if (entity->health > 0.0f)
   {
      return false;
   }

   entity->health = 0.0f;
   entity->status_flags &= ~FLAG_STATUS_ALIVE;
   return true;
}

// Align an opted-in entity's rendered geometry with its current velocity vector.
void Newtonoid_SyncOrientationToVelocity(Newtonoid2d *object)
{
   if (!object || !(object->attribute_flags & FLAG_ATTR_VELOCITY_ALIGNED) ||
       VectorMagnitude_2d(object->velocity) <= 0.0001f)
   {
      return;
   }

   object->rotation = VectorRadians_2d(object->velocity);
   SyncNewtonoidRotation(object);
}

void RebuildNewtonoidGeometry(Newtonoid2d *object)
{
   if (!object || object->surface.surface_vectors.count == 0)
   {
      return;
   }

   // Rebuild all cached geometry values after the local vertex data changes.
   Matrix2x2 local_bounds = CalcAABBCoords_Tight(
      object->surface.surface_vectors.items,
      (int)object->surface.surface_vectors.count,
      ZERO_VECTOR_2D);
   object->bounds_size = (Vector2d){
      local_bounds.col2.x - local_bounds.col1.x,
      local_bounds.col2.y - local_bounds.col1.y};
   object->local_geometry_center = CalcGeometricCentre_FromBox(local_bounds);
   object->bounds_origin = VectorSum_2d(object->anchor_position, local_bounds.col1);
   object->radius = fmaxf(object->bounds_size.x, object->bounds_size.y);
   object->edge_count = (int)object->surface.surface_vectors.count;
   object->inertia = CalculateInertia_Polygon(object->mass, &object->surface.surface_vectors);
   object->inverse_inertia = object->inertia != 0.0f ? 1.0f / object->inertia : 0.0f;
}

void SyncNewtonoidRotation(Newtonoid2d *object)
{
   if (!object)
   {
      return;
   }

   // Rebuild the local basis immediately after rotation changes, including while paused.
   Basis2d basis = Basis_BuildFromRotationScale(object->rotation, (Vector2d){1.0f, 1.0f});
   object->local_axis_x = basis.u;
   object->local_axis_y = basis.v;
}

static bool InitializeNewtonoid2d(Newtonoid2d *newtonoid, float mass,
                                  Vector2d anchor_position, Vector2d velocity,
                                  Vector2d acceleration, Surface2d surface)
{
   if (!newtonoid || surface.surface_vectors.count > MAX_SHAPE_VERTICES)
   {
      return false;
   }

   newtonoid->anchor_position = anchor_position;
   newtonoid->mass = mass;
   // Zero mass means infinite/immovable (e.g. static geometry), not divide-by-zero infinity.
   newtonoid->inverse_mass = (mass != 0.0f) ? 1.0f / mass : 0.0f;
   newtonoid->velocity = velocity;
   newtonoid->acceleration = acceleration;
   newtonoid->angular_acceleration = 0.0f;
   newtonoid->attribute_flags = ENTITY_ATTR_FLAG_NONE;
   newtonoid->health = 0.0f;
   newtonoid->max_health = 0.0f;
   newtonoid->surface = surface;
   RebuildNewtonoidGeometry(newtonoid);
   newtonoid->momentum.x = newtonoid->mass * newtonoid->velocity.x;
   newtonoid->momentum.y = newtonoid->mass * newtonoid->velocity.y;
   SyncNewtonoidRotation(newtonoid);
   Newtonoid_ConfigureMetadata(newtonoid, FLAG_TYPE_NEWTONOID,
                               FLAG_TYPE_WALL | FLAG_TYPE_NEWTONOID | FLAG_TYPE_PROJECTILE,
                               FLAG_ATTR_RIGID,
                               FLAG_STATUS_ALIVE,
                               COLOUR_LINE_DEFAULT, COLOUR_FILL_DEFAULT);
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

// Create a centred isosceles triangle with its point facing along the local positive X axis.
static Surface2d CreateIsoscelesTriangleSurface(Vector2d dimensions)
{
   Surface2d surface = {0};
   float length = dimensions.x;
   float width = dimensions.y;
   if (length <= 0.0f || width <= 0.0f)
   {
      return surface;
   }

   surface.surface_vectors = MakeLArray(3, sizeof(Vector2d));
   Vector2d vertices[3] = {
      {length * 0.5f, 0.0f},
      {-length * 0.5f, -width * 0.5f},
      {-length * 0.5f, width * 0.5f}};
   for (int vertex_index = 0; vertex_index < 3; vertex_index++)
   {
      LArray_Push(&surface.surface_vectors, &vertices[vertex_index]);
   }
   return surface;
}

// Create a centred arrow polygon with its point facing along the local positive X axis.
static Surface2d CreateArrowSurface(Vector2d dimensions, float requested_head_length)
{
   Surface2d surface = {0};
   float length = dimensions.x;
   float width = dimensions.y;
   if (length <= 0.0f || width <= 0.0f)
   {
      return surface;
   }

   float head_length = requested_head_length > 0.0f ? requested_head_length : length * 0.35f;
   if (head_length >= length)
   {
      head_length = length * 0.5f;
   }

   float half_length = length * 0.5f;
   float half_width = width * 0.5f;
   float body_half_width = half_width * 0.4f;
   float body_end = half_length - head_length;
   Vector2d vertices[7] = {
      {-half_length, -body_half_width},
      {body_end, -body_half_width},
      {body_end, -half_width},
      {half_length, 0.0f},
      {body_end, half_width},
      {body_end, body_half_width},
      {-half_length, body_half_width}};

   surface.surface_vectors = MakeLArray(7, sizeof(Vector2d));
   for (int vertex_index = 0; vertex_index < 7; vertex_index++)
   {
      LArray_Push(&surface.surface_vectors, &vertices[vertex_index]);
   }
   return surface;
}

// Build the local surface for a primitive without applying entity metadata or physics state.
static Surface2d CreatePrimitiveSurface(ShapeType shape_type,
                                        NewtonoidPrimitiveParams primitive_params)
{
   Surface2d surface = {0};
   switch (shape_type)
   {
   case SHAPE_TRIANGLE_EQUILATERAL:
      if (primitive_params.radius > 0.0f)
      {
         surface.surface_vectors = CreateVertices_Symmetric(3, primitive_params.radius,
                                                              primitive_params.radius);
      }
      break;
   case SHAPE_TRIANGLE_ISOSCELES:
      surface = CreateIsoscelesTriangleSurface(primitive_params.dimensions);
      break;
   case SHAPE_RECTANGLE:
      if (primitive_params.dimensions.x > 0.0f && primitive_params.dimensions.y > 0.0f)
      {
         surface = CreateSurface_Rectangular(primitive_params.dimensions, ZERO_VECTOR_2D);
      }
      break;
   case SHAPE_ARROW:
      surface = CreateArrowSurface(primitive_params.dimensions, primitive_params.head_length);
      break;
   default:
      break;
   }
   return surface;
}

Newtonoid2d CreateNewtonoid2d(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   Newtonoid2d newtonoid = {0};
   if (!ValidateNewtonoidSurface(surface) ||
       !InitializeNewtonoid2d(&newtonoid, mass, anchor_position, velocity, acceleration, surface))
   {
      return newtonoid;
   }

   return newtonoid;
}

Newtonoid2d *CreateNewtonoid2d_Reference(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface)
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

   InitializeNewtonoid2d(newtOb, mass, anchor_position, velocity, acceleration, surface);
   return newtOb;
}

void CreateNewtonoid2d_Out(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface, Newtonoid2d *out_newtonoid)
{
   if (!out_newtonoid)
      return;

   if (!ValidateNewtonoidSurface(surface))
   {
      return;
   }

   InitializeNewtonoid2d(out_newtonoid, mass, anchor_position, velocity, acceleration, surface);
}

// Builds surface vertices from shape parameters and creates a coloured Newtonoid.
static Newtonoid2d CreateNewtonoid2d_FromShape(ShapeBuildType build_type, int vertice_count,
                                                float min_radius, float max_radius,
                                                ColourRgba colour, float mass,
                                                Vector2d anchor_position, Vector2d velocity,
                                                Vector2d acceleration)
{
   Surface2d surface = {0};
   if (build_type == SHAPE_BUILD_IRREGULAR)
   {
      surface.surface_vectors = CreateVertices_Irregular(vertice_count, min_radius, max_radius);
   }
   else
   {
      surface.surface_vectors = CreateVertices_Symmetric(vertice_count, max_radius, max_radius);
   }

   Newtonoid2d newtOb = CreateNewtonoid2d(mass, anchor_position, velocity, acceleration, surface);
   newtOb.line_colour = colour;
   newtOb.fill_colour = colour;
   newtOb.radius = max_radius;
   return newtOb;
}

// Static bodies are simply Newtonoids with zero mass and no motion.
Newtonoid2d CreateNewtonoid2d_Static(Vector2d anchor_position, Surface2d surface)
{
   return CreateNewtonoid2d(0.0f, anchor_position, ZERO_VECTOR_2D, ZERO_VECTOR_2D, surface);
}

Newtonoid2d CreateNewtonoid2d_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration)
{
   return CreateNewtonoid2d_FromShape(SHAPE_BUILD_REGULAR, vertice_count, radius, radius,
                                       colour, mass, anchor_position, velocity, acceleration);
}

Newtonoid2d CreateNewtonoid2d_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration)
{
   return CreateNewtonoid2d_FromShape(SHAPE_BUILD_IRREGULAR, vertice_count, min_radius, max_radius,
                                       colour, mass, anchor_position, velocity, acceleration);
}

// Create a fully initialised Newtonoid from a reusable primitive shape specification.
Newtonoid2d CreateNewtonoid2d_Primitive(ShapeType shape_type,
                                         NewtonoidPrimitiveParams primitive_params,
                                         float mass, Vector2d anchor_position,
                                         Vector2d velocity, Vector2d acceleration)
{
   Newtonoid2d empty_newtonoid = {0};
   Surface2d surface = CreatePrimitiveSurface(shape_type, primitive_params);
   if (shape_type == SHAPE_AUTO || surface.surface_vectors.count < 3)
   {
      ClearLArray(&surface.surface_vectors);
      return empty_newtonoid;
   }

   Newtonoid2d newtonoid = CreateNewtonoid2d(mass, anchor_position, velocity,
                                              acceleration, surface);
   newtonoid.shape_type = shape_type;
   newtonoid.line_colour = primitive_params.colour;
   newtonoid.fill_colour = primitive_params.colour;
   return newtonoid;
}

void CalcVectors(Newtonoid2d *object, float deltaTime)
{
   // Dynamic Mass/Inertia Safety Pass
   // Recalc inverses up front in case gameplay code mutated mass or bounds this frame
   if (object->mass != 0.0f)
   {
      object->inverse_mass = 1.0f / object->mass;

      // If mass changed, re-evaluate box inertia baseline
      // I = (1/12) * m * (w^2 + h^2)
      object->inertia = (1.0f / 12.0f) * object->mass * (object->bounds_size.x * object->bounds_size.x + object->bounds_size.y * object->bounds_size.y);
      object->inverse_inertia = 1.0f / object->inertia;
   }
   else
   {
      object->inverse_mass = 0.0f;
      object->inertia = 0.0f;
      object->inverse_inertia = 0.0f; // Infinite resistance to rotation
   }

   // Linear Kinematics (Linear Integration)
   Vector2d displacement;
   displacement.x = (object->velocity.x * deltaTime) + (0.5f * object->acceleration.x * deltaTime * deltaTime);
   displacement.y = (object->velocity.y * deltaTime) + (0.5f * object->acceleration.y * deltaTime * deltaTime);

   // Displace tracking origins
   object->bounds_origin = VectorSum_2d(object->bounds_origin, displacement);
   object->anchor_position = VectorSum_2d(object->anchor_position, displacement);

   // Update linear velocity and state vectors for next frame
   object->velocity.x += object->acceleration.x * deltaTime;
   object->velocity.y += object->acceleration.y * deltaTime;

   object->momentum.x = object->mass * object->velocity.x;
   object->momentum.y = object->mass * object->velocity.y;

   // Angular Kinematics (Rotational Integration)
   object->angular_acceleration = object->torque * object->inverse_inertia;

   // Update raw rotation angle scalar and spin speed
   object->rotation += (object->angular_velocity * deltaTime) +
                       (0.5f * object->angular_acceleration * deltaTime * deltaTime);
   object->angular_velocity += object->angular_acceleration * deltaTime;

   // Matrix Sync Pass: Re-bake local coordinate framework
   SyncNewtonoidRotation(object);

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

// void RotateEntity(Newtonoid2d *entity, float radians)
// {
//    // Update the entity's rotation angle
//    entity->rotation += radians;

//    rotated.x = local_vertex.x * local_axis.x - local_vertex.y * local_axis.y;
//    rotated.y = local_vertex.x * local_axis.y + local_vertex.y * local_axis.x;
//    return rotated;
// }


Vector2d RotateVertex(Vector2d local_vertex, Vector2d local_axis)
{
   Vector2d rotated;
   // Standard 2D Rotation Matrix layout using our pre-computed local_axis vector:
   // cos(theta) is local_axis.x, sin(theta) is local_axis.y
   rotated.x = local_vertex.x * local_axis.x - local_vertex.y * local_axis.y;
   rotated.y = local_vertex.x * local_axis.y + local_vertex.y * local_axis.x;
   return rotated;
}

// Transforms the object's local surface vertices into its local space using its
// cached rotation basis and anchor position. Writes at most max_vertices entries.
void Newtonoid_TransformVertices(const Newtonoid2d *object, Vector2d *out_world_vertices, int max_vertices)
{
   if (!object || !out_world_vertices || max_vertices <= 0)
   {
      return;
   }

   Vector2d *local = (Vector2d *)object->surface.surface_vectors.items;
   int count = (int)object->surface.surface_vectors.count;
   if (count > max_vertices)
   {
      count = max_vertices;
   }

   for (int i = 0; i < count; i++)
   {
      Vector2d rotated_x = VectorScale_2d(object->local_axis_x, local[i].x);
      Vector2d rotated_y = VectorScale_2d(object->local_axis_y, local[i].y);
      Vector2d rotated = VectorSum_2d(rotated_x, rotated_y);
      out_world_vertices[i] = VectorSum_2d(object->anchor_position, rotated);
   }
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

