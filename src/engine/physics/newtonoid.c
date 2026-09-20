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
// Module Constants Definition (local)
//----------------------------------------------------------------------------------
static const float default_restitution = 0.9f;
static const float default_friction = 0.8f;
static const float SLEEP_LINEAR_DISPLACEMENT_TOLERANCE_SQ = 0.0001f; // Very small per-frame movement
static const float SLEEP_ANGULAR_DISPLACEMENT_TOLERANCE_SQ = 0.000055f; // Very small per-frame rotation
static const float TIME_TO_SLEEP = 0.3f;

//----------------------------------------------------------------------------------
// Module Functions Declaration (local)
//----------------------------------------------------------------------------------
static bool ValidateNewtonoidSurface(Surface2d surface);
static Surface2d CreateIsoscelesTriangleSurface(Vector2d dimensions);
static Surface2d CreateArrowSurface(Vector2d dimensions, float requested_head_length);
static Surface2d CreatePrimitiveSurface(ShapeType shape_type, NewtonoidPrimitiveParams primitive_params);
static Newtonoid2d CreateNewtonoid2d_FromShape(ShapeBuildType build_type, int vertice_count,
                                               float min_radius, float max_radius,
                                               ColourRgba colour, float mass,
                                               Vector2d anchor_position, Vector2d velocity,
                                               Vector2d acceleration);
static void ConfigureNewtonoidBase(Newtonoid2d *object);
static bool BuildNewtonoid2d(Newtonoid2d *out_object, ShapeType shape_type,
                             float mass, Vector2d anchor_position,
                             Vector2d velocity, Vector2d acceleration,
                             Surface2d surface);

//----------------------------------------------------------------------------------
// Entity Configuration
//----------------------------------------------------------------------------------

// Configure the metadata shared by all Newtonoid creation paths.
void Newtonoid_ConfigureMetadata(Newtonoid2d *object, EntityTypeFlags entity_flags,
                                 EntityTypeFlags collision_mask, EntityAttributeFlags attribute_flags,
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

// Configure the common metadata profile used by ordinary Newtonoid factories.
static void ConfigureNewtonoidBase(Newtonoid2d *object)
{
   Newtonoid_ConfigureMetadata(object, ENTITY_FLAG_NEWTONOID,
                               ENTITY_FLAG_WALL | ENTITY_FLAG_NEWTONOID | ENTITY_FLAG_PROJECTILE,
                               ENTITY_ATTR_FLAG_RIGID,
                               ENTITY_STATUS_FLAG_ALIVE,
                               COLOUR_LINE_DEFAULT, COLOUR_FILL_DEFAULT);
}

// Set an entity's maximum and current health to the supplied non-negative value.
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
   return entity && (entity->attribute_flags & ENTITY_ATTR_FLAG_DAMAGEABLE) != 0 &&
          entity->max_health > 0.0f && (entity->status_flags & ENTITY_STATUS_FLAG_ALIVE) != 0;
}

// Apply damage and clear the alive status when health reaches zero.
bool ApplyEntityDamage(Newtonoid2d *entity, float damage)
{
   if (!IsDamageable(entity) || damage <= 0.0f)
   {
      return false;
   }

   entity->health = 0.0f;
   entity->status_flags &= ~ENTITY_STATUS_FLAG_ALIVE;
   return true;
}

// Put an entity to sleep and clear residual motion so the state is stable.
void ApplySleep(Newtonoid2d *entity)
{
   if (!entity)
   {
      return;
   }

   entity->status_flags |= ENTITY_STATUS_FLAG_SLEEPING;
   entity->sleep_timer = TIME_TO_SLEEP;
   entity->velocity = ZERO_VECTOR_2D;
   entity->momentum = ZERO_VECTOR_2D;
   entity->angular_velocity = 0.0f;
   entity->angular_acceleration = 0.0f;
   entity->torque = 0.0f;
}

// Wake an entity and restart the quiet-period timer used by automatic sleeping.
void WakeUp(Newtonoid2d *entity)
{
   if (!entity)
   {
      return;
   }

   entity->status_flags &= ~ENTITY_STATUS_FLAG_SLEEPING;
   entity->sleep_timer = 0.0f;
}

// Align an opted-in entity's rendered geometry with its current velocity vector.
void Newtonoid_SyncOrientationToVelocity(Newtonoid2d *object)
{
   if (!object || !(object->attribute_flags & ENTITY_ATTR_FLAG_VELOCITY_ALIGNED) ||
       VectorMagnitude_2d(object->velocity) <= 0.0001f)
   {
      return;
   }

   object->rotation = VectorRadians_2d(object->velocity);
   SyncNewtonoidRotation(object);
}

// Configure the normal collision response coefficient while preventing invalid
// material values from making the impulse solver add energy unexpectedly.
void Newtonoid_ConfigureRestitution(Newtonoid2d *object, float restitution)
{
   if (!object)
   {
      return;
   }

   if (!(restitution >= 0.0f))
   {
      object->restitution = 0.0f;
   }
   else if (restitution > 1.0f)
   {
      object->restitution = 1.0f;
   }
   else
   {
      object->restitution = restitution;
   }
}

// Configure the tangential collision response coefficient while preventing
// invalid negative or NaN values from reversing the friction limit.
void Newtonoid_ConfigureFriction(Newtonoid2d *object, float friction)
{
   if (!object)
   {
      return;
   }

   object->friction = friction >= 0.0f ? friction : 0.0f;
}

//----------------------------------------------------------------------------------
// Geometry and Coordinate Synchronisation
//----------------------------------------------------------------------------------

// Rebuild the cached bounds, centre, edge count, and inertia after local geometry changes.
void RebuildNewtonoidGeometry(Newtonoid2d *object)
{
   if (!object || object->surface.surface_vectors.count == 0)
   {
      return;
   }

   // Rebuild all cached geometry values after the local vertex data changes.
   // The tight AABB is formed from the local minimum and maximum coordinates;
   // adding the anchor afterwards places that local envelope in world space.
   Matrix2x2 local_bounds = CalcAABBCoords_Tight(
       object->surface.surface_vectors.items,
       (int)object->surface.surface_vectors.count,
       ZERO_VECTOR_2D);
   object->bounds_size = (Vector2d){
       local_bounds.col2.x - local_bounds.col1.x,
       local_bounds.col2.y - local_bounds.col1.y};
   object->local_geometry_center = CalcGeometricCentre_FromBox(local_bounds);
   object->bounds_origin = VectorSum_2d(object->anchor_position, local_bounds.col1);
   // The largest AABB dimension is a cheap conservative extent for callers
   // that need a radius-like broad-phase value rather than exact geometry.
   object->radius = fmaxf(object->bounds_size.x, object->bounds_size.y);
   object->edge_count = (int)object->surface.surface_vectors.count;
   object->inertia = CalcMomentOfInertia(object->mass, &object->surface.surface_vectors);
   object->inverse_inertia = object->inertia != 0.0f ? 1.0f / object->inertia : 0.0f;
}

// Refresh the cached local coordinate basis after an object's rotation changes.
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

//----------------------------------------------------------------------------------
// Shape Construction Helpers
//----------------------------------------------------------------------------------

static bool ValidateNewtonoidSurface(Surface2d surface)
{
   if (surface.surface_vectors.count >= 3 &&
       surface.surface_vectors.count <= MAX_SHAPE_VERTICES)
   {
      return true;
   }

   LOG_ERROR("Entity creation failed: vertex count %d must be between 3 and %d\n",
             surface.surface_vectors.count, MAX_SHAPE_VERTICES);
   return false;
}

// Build a Newtonoid from owned surface data and apply its generic physics
// configuration. Release the surface on failure so shape factories cannot
// leak geometry during validation or initialisation.
static bool BuildNewtonoid2d(Newtonoid2d *out_object, ShapeType shape_type,
                             float mass, Vector2d anchor_position,
                             Vector2d velocity, Vector2d acceleration,
                             Surface2d surface)
{
   if (!out_object || !ValidateNewtonoidSurface(surface))
   {
      ClearLArray(&surface.surface_vectors);
      return false;
   }

   out_object->anchor_position = anchor_position;
   out_object->mass = mass;
   out_object->restitution = default_restitution;
   out_object->friction = default_friction;
   // Inverse mass is used by collision response to distribute movement and
   // impulses. Zero mass represents an immovable body, so its inverse is zero
   // rather than an IEEE infinity that could contaminate later calculations.
   out_object->inverse_mass = (mass != 0.0f) ? 1.0f / mass : 0.0f;
   out_object->velocity = velocity;
   out_object->acceleration = acceleration;
   out_object->angular_acceleration = 0.0f;
   out_object->attribute_flags = ENTITY_ATTR_FLAG_NONE;
   out_object->health = 0.0f;
   out_object->max_health = 0.0f;
   out_object->surface = surface;
   RebuildNewtonoidGeometry(out_object);
   out_object->momentum.x = out_object->mass * out_object->velocity.x;
   out_object->momentum.y = out_object->mass * out_object->velocity.y;
   SyncNewtonoidRotation(out_object);
   out_object->shape_type = shape_type;
   ConfigureNewtonoidBase(out_object);

   return true;
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
         surface = CreateSurface_Rectangular(primitive_params.dimensions);
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

   Newtonoid2d newtOb = {0};
   if (!BuildNewtonoid2d(&newtOb, SHAPE_POLYGON, mass, anchor_position,
                         velocity, acceleration, surface))
   {
      return newtOb;
   }

   newtOb.line_colour = colour;
   newtOb.fill_colour = colour;
   newtOb.radius = max_radius;
   return newtOb;
}

//----------------------------------------------------------------------------------
// Newtonoid Creation
//----------------------------------------------------------------------------------

Newtonoid2d CreateNewtonoid2d(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   Newtonoid2d newtonoid = {0};
   BuildNewtonoid2d(&newtonoid, SHAPE_AUTO, mass, anchor_position, velocity,
                    acceleration, surface);
   return newtonoid;
}

// Create a heap-owned Newtonoid directly so entity constructors avoid a struct copy.
Newtonoid2d *CreateNewtonoid2d_Allocated(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface)
{
   Newtonoid2d *newtonoid = AllocateBytes(sizeof(*newtonoid));
   if (!newtonoid)
   {
      LOG_ERROR("Failed to allocate memory for Newtonoid2d object.\n");
      ClearLArray(&surface.surface_vectors);
      return NULL;
   }
   // Reset the memory of the newly allocated Newtonoid2d structure.
   MemorySet(newtonoid, 0, sizeof(*newtonoid));
   if (!BuildNewtonoid2d(newtonoid, SHAPE_AUTO, mass, anchor_position,
                         velocity, acceleration, surface))
   {
      Deallocate((void **)&newtonoid, sizeof(*newtonoid));
      return NULL;
   }

   return newtonoid;
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

   Newtonoid2d newtonoid = {0};
   if (!BuildNewtonoid2d(&newtonoid, shape_type, mass, anchor_position,
                         velocity, acceleration, surface))
   {
      return empty_newtonoid;
   }

   newtonoid.line_colour = primitive_params.colour;
   newtonoid.fill_colour = primitive_params.colour;
   return newtonoid;
}

//----------------------------------------------------------------------------------
// Physics Integration
//----------------------------------------------------------------------------------

// Advance an object's linear and angular state for one simulation step.
void CalcVectors(Newtonoid2d *obj, float delta_time)
{
   if (!obj)
   {
      return;
   }

   bool position_locked = (obj->attribute_flags & ENTITY_ATTR_FLAG_POSITION_LOCKED) != 0;
   bool is_sleeping = (obj->status_flags & ENTITY_STATUS_FLAG_SLEEPING) != 0;

   // A sleeping body can be woken by an impulse applied since its last update.
   float linear_displacement_x = obj->velocity.x * delta_time;
   float linear_displacement_y = obj->velocity.y * delta_time;
   float linear_displacement_sq = (linear_displacement_x * linear_displacement_x) +
                                  (linear_displacement_y * linear_displacement_y);
   float angular_displacement = obj->angular_velocity * delta_time;
   float angular_displacement_sq = angular_displacement * angular_displacement;
   bool below_linear_tolerance = linear_displacement_sq < SLEEP_LINEAR_DISPLACEMENT_TOLERANCE_SQ;
   bool below_angular_tolerance = angular_displacement_sq < SLEEP_ANGULAR_DISPLACEMENT_TOLERANCE_SQ &&
                                   fabsf(obj->torque) <= 0.001f;

   if (is_sleeping)
   {
      if (below_linear_tolerance && below_angular_tolerance)
      {
         return;
      }

      WakeUp(obj);
   }

   // Check if under tolerances
   if (below_linear_tolerance && below_angular_tolerance)
   {
      obj->sleep_timer += delta_time;

      if (obj->sleep_timer >= TIME_TO_SLEEP)
      {
         // Put the object to sleep and zero out micro-movements
         ApplySleep(obj);
         LOG_INFO("----APPLYING SLEEP----");
         return;
      }
   }
   else
   {
      // Object is moving significantly, reset the timer
      obj->sleep_timer = 0.0f;
   }

   // Dynamic Mass/Inertia Safety Pass
   // Recalc inverses up front in case gameplay code mutated mass or bounds this frame
   if (obj->mass != 0.0f)
   {
      // A position-locked body keeps its mass and rotational inertia while
      // contributing no translational inverse mass to collision response.
      obj->inverse_mass = position_locked ? 0.0f : 1.0f / obj->mass;

      // Recalculate from local geometry so rotation-induced AABB changes do not
      // alter the body's physical moment of inertia.
      obj->inertia = CalcMomentOfInertia(obj->mass, &obj->surface.surface_vectors);
      obj->inverse_inertia = obj->inertia > 0.0f ? 1.0f / obj->inertia : 0.0f;
   }
   else
   {
      obj->inverse_mass = 0.0f;
      obj->inertia = 0.0f;
      obj->inverse_inertia = 0.0f; // Infinite resistance to rotation
   }

   if (position_locked)
   {
      // A locked body cannot accumulate linear motion, but its angular state
      // remains available for rotors and other fixed-axis bodies.
      obj->velocity = ZERO_VECTOR_2D;
      obj->momentum = ZERO_VECTOR_2D;
   }
   else
   {
      // Linear kinematics under constant acceleration. The displacement equation
      // s = v * dt + 0.5 * a * dt^2 advances the position using the velocity at
      // the start of the frame plus the acceleration contribution over the frame.
      Vector2d displacement;
      displacement.x = (obj->velocity.x * delta_time) + (0.5f * obj->acceleration.x * delta_time * delta_time);
      displacement.y = (obj->velocity.y * delta_time) + (0.5f * obj->acceleration.y * delta_time * delta_time);

      // Displace tracking origins
      obj->bounds_origin = VectorSum_2d(obj->bounds_origin, displacement);
      obj->anchor_position = VectorSum_2d(obj->anchor_position, displacement);

      // Then update velocity with v_new = v_old + a * dt. Keeping this separate
      // from the displacement calculation makes the time-step convention explicit.
      obj->velocity.x += obj->acceleration.x * delta_time;
      obj->velocity.y += obj->acceleration.y * delta_time;

      // Linear momentum is p = m * v. Store it as a derived value so systems that
      // inspect momentum do not need to reconstruct it from mass and velocity.
      obj->momentum.x = obj->mass * obj->velocity.x;
      obj->momentum.y = obj->mass * obj->velocity.y;
   }

   // Rotational Newton's second law is alpha = torque / inertia. Multiplying
   // by inverse inertia avoids a division in the hot update path.
   obj->angular_acceleration = obj->torque * obj->inverse_inertia;

   // Apply the constant-angular-acceleration equivalents of the linear formulas:
   // angle += omega * dt + 0.5 * alpha * dt^2, then omega += alpha * dt.
   obj->rotation += (obj->angular_velocity * delta_time) +
                    (0.5f * obj->angular_acceleration * delta_time * delta_time);
   obj->angular_velocity += obj->angular_acceleration * delta_time;

   // Matrix Sync Pass: Re-bake local coordinate framework
   SyncNewtonoidRotation(obj);

   // Reset accumulation registers for forces/forces of rotation
   obj->torque = 0.0f;
}

//----------------------------------------------------------------------------------
// Physics Queries
//----------------------------------------------------------------------------------

// Computes the scalar 2D mass moment of inertia about the local origin, corresponding to the axis perpendicular to the polygon
// I=∫r^2dm
float CalcMomentOfInertia(float mass, LArray *surface_vectors)
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

      // Fan the polygon into triangles from the local origin. The magnitude of
      // the 2D cross product is twice each triangle's area, so it supplies the
      // area weight without requiring a separate triangulation structure.
      float cross_product_area = fabsf(p1.x * p2.y - p2.x * p1.y);

      // Calculate how far that triangle is from the rotation origin
      // It represents the squared-distance contribution of that triangle to rotational inertia
      float geometry_factor = (VectorDot_2d(p1, p1) + VectorDot_2d(p1, p2) + VectorDot_2d(p2, p2));
      total_accumulated_numerator += cross_product_area * geometry_factor;
      total_accumulated_denominator += cross_product_area;
   }

   // Dividing the accumulated term by the total cross-product area gives
   // the area-weighted inertia per unit mass about the local origin.
   // Combine the wedges to obtain the structural inertia per unit mass, then
   // scale by the body's actual mass. The local vertices are expected to be
   // centred around the body's anchor for this origin-based result to apply.
   // For one triangle, the polar second-moment term is |cross(p1,p2)| * (|p1|^2 + p1 dot p2 + |p2|^2) / 6.
   float structural_inertia = total_accumulated_numerator / (6.0f * total_accumulated_denominator);

   // Scale it uniformly by the actual physical mass of the object
   return mass * structural_inertia;
}

// A rotating body does not have the same velocity at every point. The velocity at a certain point (or radius) is calculated.
Vector2d CalcVelocityAtPoint(const Newtonoid2d *body, Vector2d radius)
{
   Vector2d rotational_velocity = {
       -body->angular_velocity * radius.y,
       body->angular_velocity * radius.x};

   return VectorSum_2d(body->velocity, rotational_velocity);
}

//----------------------------------------------------------------------------------
// Geometry Utilities
//----------------------------------------------------------------------------------

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

// Transform an object's surface, refresh its world-space AABB, and return it.
Matrix2x2 UpdateEntityBounds(Newtonoid2d *object, Vector2d out_world_vertices[MAX_SHAPE_VERTICES])
{
   if (!object || !out_world_vertices)
      return (Matrix2x2){0};

   int vertex_count = (int)object->surface.surface_vectors.count;
   Newtonoid_TransformVertices(object, out_world_vertices, MAX_SHAPE_VERTICES);
   Matrix2x2 bounds = CalcAABBCoords_Tight(out_world_vertices, vertex_count, ZERO_VECTOR_2D);
   object->bounds_origin = bounds.col1;
   object->bounds_size = (Vector2d){
       bounds.col2.x - bounds.col1.x,
       bounds.col2.y - bounds.col1.y};
   return bounds;
}

//----------------------------------------------------------------------------------
// Legacy Utility Functions - do not delete
//----------------------------------------------------------------------------------

// Configure an entity to accept collisions with every defined entity category.
// void Newtonoid_ConfigureUniversalCollisionMask(Newtonoid2d *object)
// {
//    if (!object)
//    {
//       return;
//    }

//    object->collision_mask = ENTITY_FLAG_WALL | ENTITY_FLAG_NEWTONOID |
//                             ENTITY_FLAG_PROJECTILE | ENTITY_FLAG_EFFECT |
//                             ENTITY_FLAG_CAMERA;
// }

// void RotateEntity(Newtonoid2d *entity, float radians)
// {
//    // Update the entity's rotation angle
//    entity->rotation += radians;

//    rotated.x = local_vertex.x * local_axis.x - local_vertex.y * local_axis.y;
//    rotated.y = local_vertex.x * local_axis.y + local_vertex.y * local_axis.x;
//    return rotated;
// }

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
