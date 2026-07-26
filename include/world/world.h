/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef WORLD_H
#define WORLD_H
#include "common/common.h"
#include "physics/physics.h"
#include "math/coordinate_space.h"
#include "events/events.h"
#include "system/systems.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define PACKED_INT_LOW_BITS 26
#define PACKED_INT_HIGH_BITS 6

#define PACK_INTS(low, high) \
    (((high) & ((1 << PACKED_INT_HIGH_BITS) - 1)) << PACKED_INT_LOW_BITS) | ((low) & ((1 << PACKED_INT_LOW_BITS) - 1))

#define UNPACK_INT_LOW(packed) \
    ((packed) & ((1 << PACKED_INT_LOW_BITS) - 1))

#define UNPACK_INT_HIGH(packed) \
    (((packed) >> PACKED_INT_LOW_BITS) & ((1 << PACKED_INT_HIGH_BITS) - 1))
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum
{
    CMD_CLEAR_OBJECT_FLAG,
    CMD_SET_OBJECT_FLAG,
    CMD_DELETE_OBJECT,
} WorldCmdType;

typedef enum
{
    ARCHETYPE_INHABITANT = 0,
    ARCHETYPE_CLOCKED = 1
} ArchetypeID;

typedef struct World2d
{
    GridSpace2d grid_space; // The coordinate space of the world, containing the basis vectors and line segments for drawing the world (if applicable)
    FrameTunnel tunnel; // Transform from world-local to universe coordinates
    LArray objects;
    LArray temp_objects;
    LArray collisions;
    FlatMapInt entity_space_map;
    FlatMapInt resolved_collisions;
    FlatMapInt entity_world_index_registry;
    LArray scheduled_world_cmds;
    float gravity;
    WorldMode mode;
    int next_object_id; // Global variable to keep track of the next available ID for NewtonObjects
    Vector2d uni_coords_center; // Position of this world in the shared universe space (world-local units). Keeps local coords 0-based.
} World2d;

typedef struct
{
    WorldCmdType type;
    int target_id;
    int payload_value;   // Reuse this slot for flags, statuses, etc.
    int interval_frames; // How often to run it (e.g., 60 for once per 60 frames)
    int run_count;       // Counts down or up to track the elapsed frames
    int run_limit;
    int frame_count;
    int initial_frame_delay;
    bool active; // Can toggle this schedule on or off
} WorldCommand;

typedef struct
{
    int type_flag; // Which array is it in?
    int index;     // What slot is it in inside that array?
} EntityLocation;

// typedef struct WorldContext
// {
//     FlatMapInt *entity_world_index_registry;
//     LArray *inhabitant_objects;
//     LArray *temp_objects;
//     LArray *collisions;
//     GridSpace2d *space_entity;
//     World2d *world;
//     Vector2d game_viewport_local_origin;
// } WorldContext;

typedef struct AxisIntersectionRange2d
{
    Vector2d start; // The 2D world coordinate where the overlap begins
    Vector2d end;   // The 2D world coordinate where the overlap ends
} AxisIntersectionRange2d;

typedef struct CollisionResult_SAT
{
    Polar2d u_unit_axis;          // normalized vector representing one of the potential separating axes (the "u" axis of entity A)
    Polar2d v_unit_axis;          // normalized vector representing one of the potential separating axes (the "v" axis of entity A)
    Polar2d separating_unit_axis; // either u_unit_axis or v_unit_axis, depending on which has the least penetration (smallest overlap distance)
    Matrix2x2 collision_box;      // The collision box of the two objects
    float penetration_depth;      // How much the objects are overlapping along the separating_unit_axis (the smaller this value, the less deep the collision is, and the easier it will be to resolve)
    // AxisIntersectionRange2d overlap_range_a; // The range along the separating axis where the vertices of object A are located (the "shadow" of object A on the separating axis)
    // AxisIntersectionRange2d overlap_range_b; // The range along the separating axis where the vertices of object B are located (the "shadow" of object B on the separating axis)
    Newtonoid2d *entity_a;
    Newtonoid2d *entity_b;
    Newtonoid2d *penetrating_entity; // The entity that is actually penetrating the other
    bool is_colliding;
} CollisionResult_SAT;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
extern World2d world_1;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdateWorld(World2d *world, float delta_time);
int AddObjectToWorld(World2d *world, Newtonoid2d *object, int parent_id);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif

