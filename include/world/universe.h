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
#include "math/cvectors.h"
#include "camera/camera.h"
#include "world/world.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define UNIVERSE_MAX_WORLDS 16

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct Universe
{
    // World container
    World2d worlds[UNIVERSE_MAX_WORLDS];
    int     world_count;
    int     selected_world_index;
    int     camera_marker_ids[UNIVERSE_MAX_WORLDS]; // Per-world camera-position marker entity IDs

    // Universe-space camera (operates in world-local units)
    Camera2d camera;
    Vector2d resolution;    // Total universe dimensions in universe logical units
    Vector2d camera_offset; // Pan offset applied when a world is selected (not used when universe camera is active)

    // Creation parameters for the next world
    Vector2d next_spawn;
    Vector2d spawn_step;
    Vector2d next_resolution;
    float    next_gravity;
} Universe;

//----------------------------------------------------------------------------------
// Global Instance
//----------------------------------------------------------------------------------
extern Universe G_Universe;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Initialise the universe container with default values derived from the viewport.
void Universe_Init(Universe *u,
                   Vector2d default_spawn,
                   Vector2d default_world_resolution,
                   Vector2d universe_resolution,
                   float    default_gravity);

// Create a new world using the universe creation params; returns its index or -1 on failure.
int  Universe_CreateWorld(Universe *u,
                          Basis2d   world_basis,
                          ColourRgba fill_colour,
                          ColourRgba line_colour,
                          Camera2d  *world_camera,
                          ColourRgba camera_marker_colour,
                          WorldState *world_state);

// Select a world by index; pans the universe camera to it. Returns false if out of range.
bool Universe_SelectWorld(Universe *u, int index, Vector2d world_origin);

// Draw all worlds at their universe positions.
void Universe_Draw(Universe *u, Camera2d *world_camera);

// Check whether a universe-space click lands in a different world and auto-select it.
// Returns true if the selection changed. local_out receives the local coords in the
// newly selected (or unchanged) world.
bool Universe_ResolveClick(Universe *u,
                           Vector2d  universe_click,
                           Vector2d  world_origin,
                           Vector2d *local_out);

// --- Camera control ---
Camera2d *Universe_GetCamera(Universe *u);
void Universe_ZoomCamera(Universe *u, float factor);
void Universe_PanCamera(Universe *u, Vector2d delta);
void Universe_RotateCamera(Universe *u, float angle_delta);

// --- Accessors ---
int      Universe_GetWorldCount(const Universe *u);
int      Universe_GetSelectedIndex(const Universe *u);
World2d *Universe_GetSelectedWorld(Universe *u);
World2d *Universe_GetWorld(Universe *u, int index);

#endif
