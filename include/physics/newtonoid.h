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
// Status Flags
#define FLAG_STATUS_ALIVE (1 << 0) // 00000001 (1)
// Identity Flags
#define FLAG_TYPE_WALL (1 << 1)       // 00000010 (2)
#define FLAG_TYPE_NEWTONOID (1 << 2)  // 00000100 (4)
#define FLAG_TYPE_PROJECTILE (1 << 3) // 00001000 (8)
#define FLAG_TYPE_EFFECT (1 << 4)     // 00010000 (16)
#define FLAG_TYPE_CAMERA (1 << 5)     // 00100000 (32)
// Attribute Flags
#define FLAG_ATTR_RIGID (1 << 5) // 00100000 (32)
// Lifetime type
#define FLAG_LIFETIME_CLOCKED (1 << 6) // 01000000 (64)

// DEFAULT COLOURS
#define COLOUR_LINE_DEFAULT BLACK_RGBA
#define COLOUR_FILL_DEFAULT OLIVE_GARDEN_TAN_L

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
    Vector2d coords_center; // Authoritative position used by physics, collision, and rendering
    Vector2d velocity;      // Linear velocity (units/sec)
    Vector2d acceleration;  // Linear acceleration (units/sec²)
    Vector2d momentum;      // Linear momentum (mass × velocity)
    float mass;             // Object mass (kg)
    float inverse_mass;     // 1.0f / mass (0.0f if static)

    // ============================================================================
    // WARM FIELDS - Bounds, Rotation, Collision (~96 bytes)
    // ============================================================================
    Vector2d coords_origin;    // Derived top-left/AABB origin for box, grid, and hit-test helpers
    Vector2d boxed_dimensions; // AABB width/height
    Vector2d local_axis_x;     // Object's Forward/Right axis (cos(rotation), sin(rotation))
    Vector2d local_axis_y;     // Object's Up axis
    float rotation;            // Rotation angle in radians
    float angular_velocity;    // Rotational velocity (radians/sec)
    float torque;              // Angular force accumulator
    float inertia;             // Resistance to rotational acceleration
    float inverse_inertia;     // 1.0f / inertia (0.0f if rotationally static)
    float radius;              // Bounding circle radius

    // ============================================================================
    // COLD FIELDS - Metadata, Rendering, Hierarchy (~variable size)
    // ============================================================================
    Surface2d surface;       // Vertex data (heap-allocated LArray inside)
    Vector2d local_offset;   // Relative to parent (only used for child entities)
    ColourRgba line_colour;  // Outline color
    ColourRgba fill_colour;  // Fill color
    ShapeType shape_type;    // Shape classification for collision algorithms
    int edge_count;          // Cached edge count
    uint32_t entity_flags;   // What AM I? (e.g., LAYER_PROJECTILE)
    uint32_t collision_mask; // What can I HIT? (e.g., LAYER_ENEMY | LAYER_WALL)
    uint32_t status_flags;   // Runtime status flags (e.g., FLAG_POISONED)
    int id;                  // Unique entity ID
    int parent_id;           // Parent entity ID (-1 if root)
} Newtonoid2d;

typedef struct Newtonoid2dParams
{
    Vector2d coords_center;
    Vector2d velocity;
    Vector2d acceleration;
    Vector2d momentum;
    float mass;
    float radius;
    float width;
    float height;
    // int edge_count;
    int vertice_count;
    ColourRgba line_colour;
    ColourRgba fill_colour;
    Surface2d surface;
} Newtonoid2dParams;

typedef struct Newtonoid2d_Static
{
    Vector2d coords_center;
    Vector2d coords_origin;
    Surface2d surface;
    float mass;
    float inverse_mass;
    int id;
    int parent_id;
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

Newtonoid2d CreateNewtonoid2d(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface);
Newtonoid2d *CreateNewtonoid2d_Reference(float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration, Surface2d surface);
Newtonoid2d CreateNewtonoid2d_Static(Vector2d coords_center, Surface2d surface);
Newtonoid2d CreateNewtonoid2d_Symmetric(int vertice_count, float radius, ColourRgba colour, float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration);
Newtonoid2d CreateNewtonoid2d_Irregular(int vertice_count, float min_radius, float max_radius, ColourRgba colour, float mass, Vector2d coords_center, Vector2d velocity, Vector2d acceleration);
void CalcVectors(Newtonoid2d *object, float deltaTime);
Vector2d RotateVertex(Vector2d local_vertex, Vector2d local_axis);
// Matrix2x2 FindBoxedCoords(DArray vertices);
// Vector2d GetObjectCentre(Surface2d object_surface);

#endif
