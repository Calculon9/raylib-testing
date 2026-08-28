/**********************************************************************************************
*
UNIVERSE SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UNIVERSE_SYSTEM_H
#define UNIVERSE_SYSTEM_H

#include "common/common.h"
#include "input/drag_interaction.h"
#include "input/pointer_input.h"
#include "math/cvectors.h"
#include "math/coordinate_space.h"
#include "world/universe.h"

extern ColourRgba camera_marker_colour;

void InitUniverseSystem(void);
void SyncUniverseCameraToViewport(void);
bool SetUniverseCameraBasis(Basis2d basis);
InputRouteResult UpdateUniverseSystem(const InputFrame *input, InputRouteResult prior_result);
void UpdateUniverseInput(const InputFrame *input, bool cursor_in_game_viewport);
void DrawUniverse(void);

int CreateNewWorld(bool auto_select);
int CreateNewWorld_Preset(bool auto_select, CoordinateSpacePreset preset);
bool SelectWorldByIndex(int index);
bool IsCreateWorldAutoSelectEnabled(void);

int GetWorldCount(void);
int GetSelectedWorldIndex(void);
World2d *GetSelectedWorld(void);
World2d *GetWorldByIndex(int index);

Vector2d *GetNextWorldSpawnOriginPtr(void);
Vector2d *GetNextWorldResolutionPtr(void);
Vector2d *GetNextWorldBasisUPtr(void);
Vector2d *GetNextWorldBasisVPtr(void);
float *GetNextWorldGravityPtr(void);
int *GetNextWorldObjectCountPtr(void);

#endif
