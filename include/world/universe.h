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
    int selected_world_index;
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
                         ColourRgba camera_marker_colour, Vector2d world_center_in_universe,
                         bool auto_select);

// Select a world by index.
bool Universe_SelectWorld(Universe *u, int index);

// Delete a world by index and return whether the deletion succeeded.
bool Universe_DeleteWorld(Universe *u, int index);

// Update a world's basis and dependent transforms; returns false for invalid bases.
bool Universe_SetWorldBasis(Universe *u, int index, Vector2d basis_u, Vector2d basis_v);

// Draw all worlds at their universe positions.
void Universe_Draw(Universe *u);

// Check whether a universe-space click lands in a world and auto-select it when needed.
// Returns true when any world was hit. local_out receives local coords in that world.
bool Universe_ResolveClick(Universe *u,Vector2d universe_click, Vector2d *local_out);

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
int Universe_GetSelectedIndex(const Universe *u);
World2d *Universe_GetSelectedWorld(Universe *u);
// Resolves nested world indices (0..world_count-1) and UNIVERSE_ROOT_WORLD_INDEX.
World2d *Universe_GetWorld(Universe *u, int index);

#endif
