/**********************************************************************************************
*
SYSTEMS MODULE
*
**********************************************************************************************/
#ifndef SYSTEMS_H
#define SYSTEMS_H
#include "common/common.h"
#include "math/coordinate_space.h"
#include "camera/camera.h"
#include "raylib.h"
#include "system/utility_system.h"
#include "system/viewport_system.h"
#include "system/ui_state.h"
// #include "system/ui_system.h"
// #include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef enum
{
    FLOAT,
    INT,
    VECTOR2D,
    STRING64,
    STRING128,
    STRING256
} DataType;

typedef enum
{
    PAUSED,
    EDITING,
    RUNNING,
} WorldMode;

typedef struct World2d World2d;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

extern Font font;
extern FrameCounter frame_counter;
extern int screen_resolution_scalar;
extern size_t memory_allocated;
extern const int screenWidth;  // = 1920;
extern const int screenHeight; // = 1080;
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
void *GetEntityByID(World2d *world, int entity_id);
//----------------------------------------------------------------------------------
// UI Functions Declaration
//----------------------------------------------------------------------------------
void InitUI(void);
void InitLPanel(void);
void InitRPanel(void);
void DrawLPanel(void);
void DrawRPanel(void);
void UpdateUISystem(int mouse_x, int mouse_y);
void DrawUI(void);
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region);
void HandleBtnSwitchClick(UIElement *target);
void HandleBtnEnumerateClick(UIElement *btn);
void HandleBtnSubmitClick(UIElement *btn);
//----------------------------------------------------------------------------------
// World Functions Declaration
//----------------------------------------------------------------------------------
void UpdateWorldSystem(int mouse_x, int mouse_y);
int GetNewtonoidCount(void);
void InitWorldSystem(void);
void InitUniverseSystem(void);
void UpdateUniverseSystem(int mouse_x, int mouse_y);
void DrawWorldRegion(World2d *world, Camera2d *universe_camera);
Newtonoid2d *ResolveEntityParamsToEntity(Newtonoid2dParams *newtonoid_params);
//----------------------------------------------------------------------------------
// Integration Functions Declaration
//----------------------------------------------------------------------------------
bool PipelineTextToVector(char *input_buffer, Vector2d *target_vector);
bool PipelineTextToFloat(char *input_buffer, float *target_float);
bool PipelineTextToInt(char *input_buffer, int *target_int);
void PipelineVectorToText(Vector2d input_vector, char *target_buffer, size_t target_buffer_bytes); //, NewtonProperty object_property);
void PipelineNumberToText(float input_float, int precision, char *target_buffer, size_t target_buffer_bytes); //, NewtonProperty object_property);
void BindTextbox(UIElement *textbox, void *data_bind);
void BindTextboxData(UIElement *textbox, DataType type, void *data_bind);
void BindTextboxGroup(UIElement **textboxes, void **bindings, size_t count);
void ClearTextbox(UIElement *textbox);
void ClearAndUnbindTextbox(UIElement *textbox);
void ClearAndUnbindTextboxGroup(UIElement **textboxes, size_t count);
void WriteTextboxText(UIElement *textbox, const char *value);
void WriteTextboxInt(UIElement *textbox, int value);
void WriteTextboxFloat(UIElement *textbox, float value, int precision);
void WriteTextboxVector(Vector2d value, UIElement *textbox);
void WriteTextboxVectorPair(UIElement *textbox, Vector2d value);
void WriteTextboxNumberIfUnfocused(UIElement *textbox, float value, int precision);
void WriteTextboxVectorIfUnfocused(UIElement *textbox, Vector2d value);

//----------------------------------------------------------------------------------
// Viewport Functions Declaration
//----------------------------------------------------------------------------------
Vector2d ResolveGameViewportPixelCenter(void);
Vector2d ResolveGameViewportLocalCenter(void);
#endif
