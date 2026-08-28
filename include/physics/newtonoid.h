/**********************************************************************************************
*
NEWTONOID MODULE
*
**********************************************************************************************/
#ifndef NEWTONOID_H
#define NEWTONOID_H
#include "common/common.h"
#include "math/cvectors.h"
#include "math/geometry.h"
#include "memory/cmemory.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
typedef enum EntityTypeFlags
{
    ENTITY_FLAG_NONE = 0,
    ENTITY_FLAG_WALL = 1 << 1,
    ENTITY_FLAG_NEWTONOID = 1 << 2,
    ENTITY_FLAG_PROJECTILE = 1 << 3,
    ENTITY_FLAG_EFFECT = 1 << 4,
    ENTITY_FLAG_CAMERA = 1 << 5,
} EntityTypeFlags;

typedef enum EntityStatusFlags
{
    ENTITY_STATUS_FLAG_NONE = 0,
    ENTITY_STATUS_FLAG_ALIVE = 1 << 0,
    ENTITY_STATUS_FLAG_CLOCKED = 1 << 6,
} EntityStatusFlags;

typedef enum EntityAttributeFlags
{
    ENTITY_ATTR_FLAG_NONE = 0,
    ENTITY_ATTR_FLAG_DAMAGEABLE = 1 << 0,
    ENTITY_ATTR_FLAG_VELOCITY_ALIGNED = 1 << 1,
    ENTITY_ATTR_FLAG_AFFECT_OWNER = 1 << 2,
    ENTITY_ATTR_FLAG_RIGID = 1 << 5,
} EntityAttributeFlags;

typedef EntityTypeFlags EntityFlags;
typedef EntityTypeFlags EntityTypeFlag;
typedef EntityStatusFlags EntityStatusFlag;
typedef EntityAttributeFlags EntityAttributeFlag;

// Compatibility aliases used throughout the existing codebase.
#define FLAG_STATUS_ALIVE ENTITY_STATUS_FLAG_ALIVE
#define FLAG_TYPE_WALL ENTITY_FLAG_WALL
#define FLAG_TYPE_NEWTONOID ENTITY_FLAG_NEWTONOID
#define FLAG_TYPE_PROJECTILE ENTITY_FLAG_PROJECTILE
#define FLAG_TYPE_EFFECT ENTITY_FLAG_EFFECT
#define FLAG_TYPE_CAMERA ENTITY_FLAG_CAMERA
#define FLAG_ATTR_DAMAGEABLE ENTITY_ATTR_FLAG_DAMAGEABLE
#define FLAG_ATTR_VELOCITY_ALIGNED ENTITY_ATTR_FLAG_VELOCITY_ALIGNED
#define FLAG_ATTR_AFFECT_OWNER ENTITY_ATTR_FLAG_AFFECT_OWNER
#define FLAG_ATTR_RIGID ENTITY_ATTR_FLAG_RIGID
#define FLAG_LIFETIME_CLOCKED ENTITY_STATUS_FLAG_CLOCKED

// DEFAULT COLOURS
#define COLOUR_LINE_DEFAULT COLOUR_GAME_INK_RGBA
#define COLOUR_FILL_DEFAULT COLOUR_GAME_TERRACOTTA_RGBA

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// 2D velocity state
typedef struct Velocity2d
{
    Vector2d velocityXy;
    float radians;
    float magnitude;
} Velocity2d;

// 2D acceleration state
typedef struct Acceleration2d
{
    Vector2d accelerationXy;
    float radians;
    float magnitude;
} Acceleration2d;

// 2D mommentum state
typedef struct Momentum2d
{
    Vector2d momentumXy;
    float radians;
    float magnitude;
} Momentum2d;

typedef struct Box2d
{
    Vector2d coords;     // Box origin, usually the top-left corner for AABB/bounds helpers
    Vector2d dimensions; // Width (x) and Height (y)
} Box2d;

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
    EntityFlags entity_flags;        // What AM I? (e.g., LAYER_PROJECTILE)
    EntityFlags collision_mask;      // What can I HIT? (e.g., LAYER_ENEMY | LAYER_WALL)
    EntityAttributeFlags attribute_flags; // Persistent gameplay capabilities (e.g., damageable)
    EntityStatusFlags status_flags;   // Runtime status flags (e.g., FLAG_POISONED)
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

typedef struct Newtonoid2d_Static
{
    Vector2d anchor_position;
    Vector2d bounds_origin;
    Surface2d surface;
    float mass;
    float inverse_mass;
    EntityId id;
    EntityId parent_id;
} Newtonoid2d_Static;

typedef enum
{
    NOTHING,
    ID,
    MASS,
    VELOCITY,
    ACCELERATION,
    MOMENTUM,
    POSITION,
} NewtonProperty;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Apply the entity flags, collision mask, attribute flags, status flags, and render colours for a Newtonoid.
void Newtonoid_ConfigureMetadata(Newtonoid2d *object, EntityFlags entity_flags,
                                 EntityFlags collision_mask, EntityAttributeFlags attribute_flags,
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
Newtonoid2d CreateNewtonoid2d(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface);
Newtonoid2d *CreateNewtonoid2d_Reference(float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration, Surface2d surface);
Newtonoid2d CreateNewtonoid2d_Static(Vector2d anchor_position, Surface2d surface);
Newtonoid2d CreateNewtonoid2d_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
Newtonoid2d CreateNewtonoid2d_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
// Create a coloured Newtonoid from one of the reusable primitive entity shapes.
Newtonoid2d CreateNewtonoid2d_Primitive(ShapeType shape_type, NewtonoidPrimitiveParams primitive_params,
                                         float mass, Vector2d anchor_position, Vector2d velocity, Vector2d acceleration);
void RebuildNewtonoidGeometry(Newtonoid2d *object);
void SyncNewtonoidRotation(Newtonoid2d *object);
Vector2d CalcVelocityAtPoint(const Newtonoid2d *body, Vector2d radius);
void CalcVectors(Newtonoid2d *object, float deltaTime);
Vector2d RotateVertex(Vector2d local_vertex, Vector2d local_axis);
void Newtonoid_TransformVertices(const Newtonoid2d *object, Vector2d *out_world_vertices, int max_vertices);
// Matrix2x2 FindBoxedCoords(DArray vertices);
// Vector2d GetObjectCentre(Surface2d object_surface);

#endif
