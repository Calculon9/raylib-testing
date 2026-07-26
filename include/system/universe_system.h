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
void UpdateUniverseSystem(int mouse_x, int mouse_y);

/**
 * Update universe-level input (world selection, camera navigation)
 * Called when no world is selected
 */
void UpdateUniverseInput(int mouse_x, int mouse_y, bool cursor_in_game_viewport);

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

#endif
