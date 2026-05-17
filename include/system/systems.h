/**********************************************************************************************
*
SYSTEMS MODULE
*
**********************************************************************************************/
#ifndef SYSTEMS_H
#define SYSTEMS_H
#include "common/common.h"
#include "math/cvectors.h"
#include "colour/colour.h"
#include "ui/cfont.h"
#include "raylib.h"
#include "system/utility_system.h"


//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct String32 {
    char string[32];
} String32;

typedef struct String64 {
    char string[64];
} String64;

typedef struct String128 {
    char string[128];
} String128;

typedef struct String256 {
    char string[256];
} String256;
//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

extern Font font;
extern FrameCounter frame_counter;
extern int screen_resolution_scalar;
extern size_t memory_allocated;
extern const int screenWidth;// = 1920;
extern const int screenHeight;// = 1080;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
// Utility Functions Declaration
//----------------------------------------------------------------------------------
void InitUtilities();
FrameCounter InitFrameCounter();
void UpdateUtilities();
void UpdateFrameCounter(FrameCounter *fc);
size_t GetCurrentMemoryAllocated();

//----------------------------------------------------------------------------------
// UI Functions Declaration
//----------------------------------------------------------------------------------
void InitUI(void);
void InitPanelSpace(void);
void UpdateUISystem(int mouse_x, int mouse_y);
void DrawUI(void);
void UpdateInput(void);
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region);

//----------------------------------------------------------------------------------
// World Functions Declaration
//----------------------------------------------------------------------------------
void UpdateWorldSystem(int mouse_x, int mouse_y);
int GetPolygonoidCount(void);
void InitGameWorld(void);
void DrawGameWorld(void);

#endif