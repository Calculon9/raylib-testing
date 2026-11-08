/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_INPUT_H
#define UI_INPUT_H
#include "common/common.h"
#include "system/systems.h"
#include "system/utility_system.h"
#include "ui/ui.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

// #define FONT_DEFAULT FONT_BASIC

typedef struct
{
    bool has_pending_edit;
    String64 temp_buffer;
} Text_64_IOState;

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------


//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);
void ClearUIFocus(void);
void ResetUIInputState(void);

#endif
