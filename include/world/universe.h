/**********************************************************************************************
*
UNIVERSE MODULE
*
A Universe is the top-level container for all World2d instances, analogous to a root
UIElement in the UI system. It owns the world array, the universe-space camera offset,
and all creation parameters for new worlds.
*
**********************************************************************************************/
#ifndef UNIVERSE_H
#define UNIVERSE_H

#include "common/common.h"
#include "input/pointer_input.h"
#include "input/drag_interaction.h"
#include "math/cvectors.h"
#include "camera/camera.h"
#include "world/world.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define UNIVERSE_MAX_WORLDS 16
// Sentinel world index identifying the universe's own root world (objects not owned by a nested world).
#define UNIVERSE_ROOT_WORLD_INDEX (-2)

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Universe
{
    // World container
    World2d worlds[UNIVERSE_MAX_WORLDS];
    int world_count;
    int selected_world_index;
    World2d root_world; // Spans the whole universe; holds objects not owned by any nested world.
    EntityId next_entity_id; // IDs are allocated once for the whole universe, not once per world.

    // Universe-space camera (operates in world-local units)
    CameraController camera_ctrl;
    Camera2d camera;
    Vector2d resolution; // Total universe dimensions in universe logical units

    // Creation parameters for the next world
    Vector2d next_spawn; // World center in universe space; top-left is derived from this and world size
    Vector2d spawn_step;
    Vector2d next_resolution;
    Vector2d next_basis_u;
    Vector2d next_basis_v;
    float next_gravity;
    int next_object_count;
} Universe;

//----------------------------------------------------------------------------------
// Global Instance
//----------------------------------------------------------------------------------
extern Universe G_Universe;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Initialise the universe container with default values derived from the viewport.
void Universe_Init(Universe *u, Vector2d default_spawn, Vector2d default_new_world_resolution,
                   float default_gravity);

// Create a new world using the universe creation params; returns its index or -1 on failure.
int Universe_CreateWorld(Universe *u, ColourRgba fill_colour, ColourRgba line_colour,
                         ColourRgba camera_marker_colour, Vector2d world_center_in_universe,
                         bool auto_select);

// Select a world by index.
bool Universe_SelectWorld(Universe *u, int index);

// Update a world's basis and dependent transforms; returns false for invalid bases.
bool Universe_SetWorldBasis(Universe *u, int index, Vector2d basis_u, Vector2d basis_v);

// Draw all worlds at their universe positions.
void Universe_Draw(Universe *u);

// Check whether a universe-space click lands in a world and auto-select it when needed.
// Returns true when any world was hit. local_out receives local coords in that world.
bool Universe_ResolveClick(Universe *u,Vector2d universe_click, Vector2d *local_out);
bool ConsumeUniverseWorldClick(void);

// Find the world index at a universe-space point, or -1 if none.
int Universe_FindWorldAt(const Universe *u, Vector2d universe_point);

// Find the world that owns an entity pointer, or -1 if it is not registered.
int Universe_FindWorldContainingObject(const Universe *u, const Newtonoid2d *object);

// Find an entity by universal ID and optionally return its owning world index.
Newtonoid2d *Universe_GetEntityByID(const Universe *u, EntityId entity_id, int *world_index_out);
EntityId Universe_AllocateEntityId(Universe *u);

// --- Camera control ---
Camera2d *Universe_GetCamera(Universe *u);
// void Universe_ZoomCamera(Universe *u, float factor);
// void Universe_PanCamera(Universe *u, Vector2d delta);
// void Universe_RotateCamera(Universe *u, float angle_delta);

// --- Accessors ---
int Universe_GetWorldCount(const Universe *u);
int Universe_GetSelectedIndex(const Universe *u);
World2d *Universe_GetSelectedWorld(Universe *u);
// Resolves nested world indices (0..world_count-1) and UNIVERSE_ROOT_WORLD_INDEX.
World2d *Universe_GetWorld(Universe *u, int index);

#endif

/**********************************************************************************************
 *
 *   UNIVERSE SYSTEM MODULE
 *
 *   Multi-world container management and universe-level navigation
 *
 **********************************************************************************************/
#ifndef UNIVERSE_SYSTEM_H
#define UNIVERSE_SYSTEM_H
#include "common/common.h"
#include "math/cvectors.h"
#include "world/universe.h"

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

extern ColourRgba camera_marker_colour;
/**
 * Initialize the universe system with independent universe coordinates.
 * The camera renders into the game viewport region resolved by viewport_system.
 */
void InitUniverseSystem(void);

// Re-sync universe camera destination frame to current viewport geometry.
void SyncUniverseCameraToViewport(void);

// Apply a universe camera basis edit by updating the camera's canonical
// zoom/rotation state (used by UpdateCameraFull).
bool SetUniverseCameraBasis(Basis2d basis);

// Configure total universe grid dimensions independent of panel or viewport layout.
void SetUniverseGridCellCounts(int cells_x, int cells_y);

// Configure universe grid cell size in logical units.
void SetUniverseGridCellSize(float cell_size);

/**
 * Update universe-level state while no world is selected.
 * Handles viewport hit testing, camera navigation, and world selection.
 */
InputRouteResult UpdateUniverseSystem(const InputFrame *input, InputRouteResult prior_result);

/**
 * Update universe-level input (world selection, camera navigation)
 * Called when no world is selected
 */
void UpdateUniverseInput(const InputFrame *input, bool cursor_in_game_viewport);

/**
 * Draw all worlds in the universe using the appropriate camera
 * Either universe camera (no world selected) or world camera (world selected)
 */
void DrawUniverse(void);

// World orchestration entrypoints owned by universe system.
int CreateNewWorld(bool auto_select);
bool SelectWorldByIndex(int index);
bool IsCreateWorldAutoSelectEnabled(void);
int *GetCreateWorldAutoSelectPtr(void);

// Universe/world accessors used by UI and gameplay systems.
int GetWorldCount(void);
int GetSelectedWorldIndex(void);
World2d *GetSelectedWorld(void);
World2d *GetWorldByIndex(int index);

// Next-world creation params.
Vector2d *GetNextWorldSpawnOriginPtr(void);
Vector2d *GetNextWorldResolutionPtr(void);
Vector2d *GetNextWorldBasisUPtr(void);
Vector2d *GetNextWorldBasisVPtr(void);
float *GetNextWorldGravityPtr(void);
int *GetNextWorldObjectCountPtr(void);

#endif
