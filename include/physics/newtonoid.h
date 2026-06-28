/**********************************************************************************************
*
NEWTONOID MODULE
*
**********************************************************************************************/
#ifndef NEWTONOID_H
#define NEWTONOID_H
#include "common/common.h"
#include "colour/colour.h"
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
    Vector2d coords;     // Top-Left or Center Origin (We will assume Top-Left here)
    Vector2d dimensions; // Width (x) and Height (y)
} Box2d;

// 2D Object with Newtonian properties; mass, position, velocity, acceleration, momentum
typedef struct Newtonoid2d
{
    Vector2d coords_center;
    Vector2d coords_origin; // the top-left
    Vector2d local_offset;  // relative to parent
    Vector2d boxed_dimensions;
    Vector2d velocity;
    Vector2d acceleration;
    Vector2d momentum;
    Surface2d surface;
    int edge_count;
    Vector2d local_axis_x;  // object's Forward/Right arrow is pointing     // Pre-computed (cos(rotation), sin(rotation))
    Vector2d local_axis_y;  // object's Up arrow is pointing
    float rotation;         // The raw angle in radians
    float angular_velocity; // Spin speed (radians per second)
    float torque;           // Angular force accumulator
    float inertia;          // Resistance to rotational acceleration (like mass)
    float inverse_inertia;  // 1.0f / inertia (0.0f if rotationally static)
    float radius;           // Bounding circle radius
    ColourRgba line_colour;
    ColourRgba fill_colour;

    // Surface2d footprint;
    float mass;
    float inverse_mass;

    // --- BITWISE DATA ---
    uint32_t entity_layer;   // 1. What AM I? (e.g., LAYER_PROJECTILE)
    uint32_t collision_mask; // 2. What can I HIT? (e.g., LAYER_ENEMY | LAYER_WALL)
    uint32_t flags;          // 3. What is happening to me RIGHT NOW? (e.g., FLAG_POISONED | FLAG_GROUNDED)
    int id;
    int parent_id;
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
    int edge_count;
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