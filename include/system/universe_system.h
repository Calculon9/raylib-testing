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

// Configure total universe grid dimensions independent of panel or viewport layout.
void SetUniverseGridCellCounts(int cells_x, int cells_y);

// Configure universe grid cell size in logical units.
void SetUniverseGridCellSize(float cell_size);

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

#endif
