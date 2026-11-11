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
// View / display settings
//----------------------------------------------------------------------------------
// Universe-space grid-label visibility (view/display setting, not debug). Default OFF. Owned by
// the world layer; the debug-overlay facade's DEBUG_UNIVERSE_GRID_LABELS case delegates here.
extern bool universe_grid_labels_enabled;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Universe
{
    // World container
    World2d worlds[UNIVERSE_MAX_WORLDS];
    int world_count;
    World2d root_world; // Spans the whole universe; holds objects not owned by any nested world.

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
                         ColourRgba camera_marker_colour, Vector2d world_center_in_universe);

// Delete a world by index and return whether the deletion succeeded.
bool Universe_DeleteWorld(Universe *u, int index);

// Outcome of an attempt to grow a world's grid (see Universe_GrowWorldToInclude).
typedef enum WorldGrowthStatus
{
    WORLD_GROWTH_OK,
    WORLD_GROWTH_NOT_NEEDED,
    WORLD_GROWTH_REFUSED_NOT_ADJACENT,
    WORLD_GROWTH_REFUSED_CAP,
    WORLD_GROWTH_REFUSED_OVERLAP,
    WORLD_GROWTH_FAILED_ALLOC,
    WORLD_GROWTH_INVALID_WORLD
} WorldGrowthStatus;

// Grow nested world `world_index` so absolute lattice cell (lattice_x, lattice_y) is inside its grid.
// Validates root/invalid world, edge adjacency to a PRESENT cell, the 512 cap and overlap with other
// worlds (in that order), logs every refusal, and changes nothing on refusal or failure.
// NEVER CALL OUTSIDE THE COMMAND DRAIN (ProcessCommandQueue): growth reallocates the cell buffer.
WorldGrowthStatus Universe_GrowWorldToInclude(Universe *u, int world_index, int lattice_x, int lattice_y);

// Shrink nested world `world_index` to the tight bounding box of its PRESENT cells after the cell at lattice
// (deleted_lx, deleted_ly) was cleared (no-op unless that cell was on an outer row/column). The box also keeps the
// cell holding each world-owned entity's centre, so no entity is stranded. Returns true when the grid was resized;
// a failed allocation leaves the world untouched. The root world is never trimmed.
// NEVER CALL OUTSIDE THE COMMAND DRAIN (ProcessCommandQueue): a trim reallocates the cell buffer.
bool Universe_TrimWorldToPresentCells(Universe *u, int world_index, int deleted_lx, int deleted_ly);

// True when the lattice rectangle [x0,x1)x[y0,y1) of world `world_index`, mapped through its frame, strictly
// overlaps (deeper than eps universe units) the full grid parallelogram of any OTHER nested world.
bool Universe_StripOverlapsOtherWorld(const Universe *u, int world_index, int x0, int y0, int x1, int y1, float eps);
// Update a world's basis and dependent transforms; returns false for invalid bases.
bool Universe_SetWorldBasis(Universe *u, int index, Vector2d basis_u, Vector2d basis_v);

// Draw all worlds at their universe positions.
void Universe_Draw(Universe *u);

// Find the world index at a universe-space point, or -1 if none.
int Universe_FindWorldAt(const Universe *u, Vector2d universe_point);

// Find an entity by universal ID and optionally return its owning world index.
Newtonoid2d *Universe_GetEntityByID(const Universe *u, EntityId entity_id, int *world_index_out);
// Check whether an entity pointer resolves to and is owned by the supplied world.
bool Universe_IsEntityOwnedByWorld(const Universe *u, const World2d *world,
                                   const Newtonoid2d *entity);
World2d *Universe_GetWorldById(Universe *u, EntityId world_id);
int Universe_GetWorldIndexById(const Universe *u, EntityId world_id);

// --- Camera control ---
Camera2d *Universe_GetCamera(Universe *u);
// void Universe_ZoomCamera(Universe *u, float factor);
// void Universe_PanCamera(Universe *u, Vector2d delta);
// void Universe_RotateCamera(Universe *u, float angle_delta);

// --- Accessors ---
int Universe_GetWorldCount(const Universe *u);
// Resolves nested world indices (0..world_count-1) and UNIVERSE_ROOT_WORLD_INDEX.
World2d *Universe_GetWorld(Universe *u, int index);

#endif
