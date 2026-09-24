/**********************************************************************************************
*
NEWTONOID MODULE
*
**********************************************************************************************/
#ifndef NEWTONOID_H
#define NEWTONOID_H
#include "common/common.h"
#include "entities/entity_flags.h"
#include "math/cvectors.h"
#include "math/geometry.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

// DEFAULT COLOURS
#define COLOUR_LINE_DEFAULT COLOUR_GAME_INK_RGBA
#define COLOUR_FILL_DEFAULT COLOUR_GAME_TERRACOTTA_RGBA

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// 2D Object with Newtonian properties; mass, position, velocity, acceleration, momentum
// CACHE-OPTIMIZED LAYOUT: Hot fields (physics update) grouped at start for L1 cache efficiency
typedef struct Newtonoid2d
{
    // ============================================================================
    // HOT FIELDS - Physics Update (accessed every frame, ~72 bytes, fits 2 cache lines)
    // ============================================================================
    Vector2d anchor_position; // Authoritative position used by physics, collision, and rendering
    Vector2d velocity;      // Linear velocity (units/sec)
    Vector2d acceleration;  // Linear acceleration (units/sec²)
    Vector2d momentum;      // Linear momentum (mass × velocity)
    float mass;             // Object mass (kg)
    float inverse_mass;     // 1.0f / mass (0.0f if static)
    float restitution;      // Normal contact coefficient in the inclusive range [0, 1]
    float friction;         // Tangential contact coefficient, zero or greater
    float sleep_timer;      // Tracks consecutive frames where kinetic energy is below a threshold

    // ============================================================================
    // WARM FIELDS - Bounds, Rotation, Collision (~96 bytes)
    // ============================================================================
    Vector2d bounds_origin;    // Derived parent-space AABB minimum for box, grid, and hit-test helpers
    Vector2d bounds_size;      // AABB width/height
    Vector2d local_geometry_center; // Cached center of the local vertex bounds
    Vector2d local_axis_x;     // Object's Forward/Right axis (cos(rotation), sin(rotation))
    Vector2d local_axis_y;     // Object's Up axis
    float rotation;            // Rotation angle in radians
    float angular_velocity;    // Rotational velocity (radians/sec)
    float angular_acceleration; // Rotational acceleration (radians/sec²)
    float torque;              // Angular force accumulator
    float inertia;             // Resistance to rotational acceleration
    float inverse_inertia;     // 1.0f / inertia (0.0f if rotationally static)
    float radius;              // Bounding circle radius

    // ============================================================================
    // COLD FIELDS - Metadata, Rendering, Hierarchy (~variable size)
    // ============================================================================
    Surface2d surface;       // Vertex data (heap-allocated LArray inside)
    Vector2d parent_offset;   // Relative to parent (only used for child entities)
    ColourRgba line_colour;  // Outline color
    ColourRgba fill_colour;  // Fill color
    ShapeType shape_type;    // Shape classification for collision algorithms
    int edge_count;          // Cached edge count
    EntityRoleFlags roles;                         // What kind of entity is this?
    EntityCollisionLayerFlags collision_layers;  // Which entity roles can collide with this entity?
    EntityCapabilityFlags capabilities;          // Persistent gameplay behaviours this entity supports
    EntityConstraintFlags constraints;           // Physics restrictions on this entity
    EntityStatusFlags status_flags;              // Runtime status such as alive or sleeping
    EntityId id;             // Universal entity ID
    EntityId owner_id;        // Entity that created this object, when ownership applies
    float damage;             // Gameplay damage carried by damage-dealing entities
    float health;             // Current health for damageable entities
    float max_health;         // Maximum health for damageable entities
    EntityId parent_id;      // Parent entity ID (INVALID_ENTITY_ID if root)
} Newtonoid2d;

typedef struct Newtonoid2dParams
{
    Vector2d anchor_position;
    Vector2d velocity;
    Vector2d acceleration;
    Vector2d momentum;
    float mass;
    float restitution;
    float friction;
    float radius;
    float width;
    float height;
    // int edge_count;
    int vertice_count;
    ShapeType shape_type;
    ColourRgba line_colour;
    ColourRgba fill_colour;
    Surface2d surface;
} Newtonoid2dParams;

typedef struct NewtonoidPrimitiveParams
{
    Vector2d dimensions; // Rectangle uses width/height; triangle and arrow use length/width
    float radius;        // Circumradius used by the equilateral triangle
    float head_length;   // Optional arrow head length; zero selects the default proportion
    ColourRgba colour;
} NewtonoidPrimitiveParams;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Apply roles, collision layers, capabilities, constraints, status, and render colours.
void Newtonoid_ConfigureMetadata(Newtonoid2d *object, EntityRoleFlags roles,
                                 EntityCollisionLayerFlags collision_layers,
                                 EntityCapabilityFlags capabilities,
                                 EntityConstraintFlags constraints,
                                 EntityStatusFlags status_flags,
                                 ColourRgba line_colour, ColourRgba fill_colour);
// Set an entity's maximum and current health.
void Newtonoid_ConfigureHealth(Newtonoid2d *object, float max_health);
// Return whether an entity can receive damage.
bool IsDamageable(const Newtonoid2d *entity);
// Apply damage and return true when the entity becomes dead.
bool ApplyEntityDamage(Newtonoid2d *entity, float damage);
// Align an opted-in entity's rendered geometry with its current velocity vector.
void Newtonoid_SyncOrientationToVelocity(Newtonoid2d *object);
// Configure an entity's normal collision bounce coefficient, clamped to [0, 1].
void Newtonoid_ConfigureRestitution(Newtonoid2d *object, float restitution);
// Configure an entity's tangential collision friction coefficient, clamped to zero or greater.
void Newtonoid_ConfigureFriction(Newtonoid2d *object, float friction);
Newtonoid2d CreateNewtonoid2d(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface);
// Create an allocated Newtonoid directly, transferring surface ownership on success.
Newtonoid2d *CreateNewtonoid2d_Allocated(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface);
Newtonoid2d CreateNewtonoid2d_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
Newtonoid2d CreateNewtonoid2d_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
// Create a coloured Newtonoid from one of the reusable primitive entity shapes.
Newtonoid2d CreateNewtonoid2d_Primitive(ShapeType shape_type, NewtonoidPrimitiveParams primitive_params,
                                         float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
void RebuildNewtonoidGeometry(Newtonoid2d *object);
void SyncNewtonoidRotation(Newtonoid2d *object);
Vector2d CalcVelocityAtPoint(const Newtonoid2d *body, Vector2d radius);
void CalcVectors(Newtonoid2d *object, float deltaTime);
float CalcMomentOfInertia(float mass, LArray *surface_vectors);
void Newtonoid_TransformVertices(const Newtonoid2d *object, Vector2d *out_world_vertices, int max_vertices);
Matrix2x2 UpdateEntityBounds(Newtonoid2d *object, Vector2d out_world_vertices[MAX_SHAPE_VERTICES]);
void ApplySleep(Newtonoid2d *entity);
void WakeUp(Newtonoid2d *entity);
// Matrix2x2 FindBoxedCoords(DArray vertices);
// Vector2d GetObjectCentre(Surface2d object_surface);

#endif
