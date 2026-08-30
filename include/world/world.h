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
typedef enum WorldFlags
{
    WORLD_FLAG_NONE = 0,
    WORLD_FLAG_ACTIVE = 1 << 0,
    WORLD_FLAG_VISIBLE = 1 << 1,
    WORLD_FLAG_SELECTABLE = 1 << 2,
    WORLD_FLAG_PHYSICS_ENABLED = 1 << 3,
    WORLD_FLAG_SPAWNS_ENABLED = 1 << 4,
    WORLD_FLAG_LOCKED = 1 << 5,
    WORLD_FLAG_DRAGGABLE = 1 << 6,
} WorldFlags;

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
    WorldFlags flags;
    WorldMode mode;
    struct Universe *universe; // Owner used for universe-wide entity ID allocation.
    Vector2d uni_coords_center; // Position of this world in the shared universe space (world-local units). Keeps local coords 0-based.
    Matrix2x2 bounds; // Cached universe-space AABB; only meaningful when bounds_valid.
    bool bounds_valid;
    EntityId camera_marker_id; // Entity ID of this world's camera-position marker, if any.
} World2d;

void DrawNewtonoids(LArray *newtonoids, Matrix3x3 space_to_pixel_mtx);

typedef struct
{
    WorldCmdType type;
    EntityId target_id;
    int payload_value;   // Reuse this slot for flags, statuses, etc.
    int interval_frames; // How often to run it (e.g., 60 for once per 60 frames)
    int run_count;       // Counts down or up to track the elapsed frames
    int run_limit;
    int frame_count;
    int initial_frame_delay;
    bool active; // Can toggle this schedule on or off
} WorldCommand;

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
    Vector2d collision_normal;     // Unit normal directed from entity A towards entity B
    Vector2d contact_point;        // World-space point where the impulse is applied
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
EntityId AddObjectToWorld(World2d *world, Newtonoid2d *object, EntityId parent_id);
EntityId MoveObjectBetweenWorlds(World2d *source_world, World2d *destination_world,
                                 EntityId object_id, EntityId destination_parent_id,
                                 Vector2d destination_coords);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);

#endif

/**********************************************************************************************
*
WORLD MODULE
*
**********************************************************************************************/
#ifndef WORLD_SYSTEM_H
#define WORLD_SYSTEM_H
#include "common/common.h"
#include "math/cvectors.h"
#include "camera/camera.h"
#include "system/systems.h"
#include "system/viewport_system.h"
#include "world/universe.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// Panel Colour Pallette
#define COLOUR_WORLD_DARK_1 OLIVE_GARDEN_GREEN_D // BROWN_1_RGBA
// #define COLOUR_PANEL_MID_1 BEIGE_RGBA
#define COLOUR_WORLD__XIGHT_1 OLIVE_GARDEN_GREEN_XL // BROWN_1_RGBA_4
#define COLOUR_WORLD__LIGHT_1 OLIVE_GARDEN_GREEN_L // BROWN_1_RGBA_4

#define COLOUR_WORLD__LIGHT_3 OLIVE_GARDEN_CREAM

#define COLOUR_WORLD__DARK_2 OLIVE_GARDEN_TAN_D // DARKBROWN_RGBA
// #define COLOUR_PANEL_MID_2 BROWN_2_RGBA_1
#define COLOUR_WORLD_LIGHT_2 OLIVE_GARDEN_TAN_L // BROWN_2_RGBA_4

#define COLOUR_ERROR RED_ERROR_RGBA
#define COLOUR_WARNING YELLOW_WARNING_RGBA
//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------



// Game viewport placement in logical units inside the full screen layout.
// extern Vector2d game_viewport_local_origin, game_viewport_local_end;// = {0};
// extern Vector2d game_viewport_local_resolution;// = {0};

// Game viewport in pixel-space (destination area for world/universe rendering).
// extern Vector2d game_viewport_pixel_origin, game_viewport_pixel_end;// = {0};
// extern Vector2d game_viewport_pixel_u;// = {75, 0};
// extern Vector2d game_viewport_pixel_v;// = {0, 75};
//extern Camera2d camera_world;// = {0};

extern bool world_grid_debug_labels_enabled;

// typedef struct {
//     Texture *texture;
// } UIImageData;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//UIElement *CreateTextField(float width, float height, Vector2d origin_coords, Vector2d parent_offset, Vector2d label_tbox_offset, Vector2d label_tbox_padding, char max_label_chars, char max_text_box_chars);
void ProcessCommandQueue(void);
#endif

