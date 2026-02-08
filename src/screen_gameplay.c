/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 *   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
 *
 *   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
 *
 *   This software is provided "as-is", without any express or implied warranty. In no event
 *   will the authors be held liable for any damages arising from the use of this software.
 *
 *   Permission is granted to anyone to use this software for any purpose, including commercial
 *   applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not claim that you
 *     wrote the original software. If you use this software in a product, an acknowledgment
 *     in the product documentation would be appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *     as being the original software.
 *
 *     3. This notice may not be removed or altered from any source distribution.
 *
 **********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include <stdio.h>
#include "utility/utility.h"
#include "physics/circloid.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
//static int framesCounter = 0;
static int finishScreen = 0;
static int maxObjects = 128;
//static Font fontSidePanel = {0};
//static Font fontStage = {0};
static DynamicArray *circloids = NULL;// = {0};


//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void DrawCircloids();
int GetCircloidCount(void);

// Gameplay Screen Initialization logic
void InitGameplayScreen(void)
{
    // Initialise state variables
    circloids = NEW_DYNAMIC_ARRAY(maxObjects, Circloid);

    // Initialise utilities (FPS tracking, etc.)
    InitUtilities();

    // TODO: Initialize GAMEPLAY screen variables here!
    //framesCounter = 0;
    finishScreen = 0;
}

//-

// Gameplay Screen Update logic
void UpdateGameplayScreen(void)
{
    // TODO: Update GAMEPLAY screen variables here!
    UpdateGameplayScreenPanel();
    
    UpdateGameplayScreenStage();
    
    // Press enter or tap to change to ENDING screen
    // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
    // {
    //     finishScreen = 1;
    //     PlaySound(fxCoin);
    // }


    // Press enter or tap to change to ENDING screen
    // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
    // {
    //     finishScreen = 1;
    //     PlaySound(fxCoin);
    // }
}

// Gameplay Screen Stage Update logic
void UpdateGameplayScreenPanel(void) {

    UpdateUtilities();
}

// Gameplay Screen Stage Update logic
void UpdateGameplayScreenStage(void) {

    //Draw a circle where the mouse clicks and add it to the state
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        //Add a new circloid to the state with the position of the mouse click
        Circloid newCircloid = {0};
        newCircloid.object.pos.x = GetMouseX();
        newCircloid.object.pos.y = GetMouseY();
        newCircloid.radius = 32;

        //Add a new circloid to the state with the position of the mouse click
        pushArray(circloids, &newCircloid);

        //finishScreen = 1;
        //PlaySound(fxCoin);
    }
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void)
{
    int panel_width = 250;
    int panel_height = GetScreenHeight();
    int stage_width = GetScreenWidth() - panel_width;
    int stage_height = GetScreenHeight();

    // Draw the side panel
    DrawGameplayScreenPanel(0, 0, panel_width, panel_height, (Color)BROWN);

    // Draw the stage
    DrawGameplayScreenStage(panel_width, 0, stage_width, stage_height, (Color)DARKGREEN);
    // Vector2 pos = { 20, 100 };
    // DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);

    // DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
}

// Gameplay Screen - Stats panel Draw
void DrawGameplayScreenPanel(int startX, int startY, int width, int height, Color color)
{
    // TODO: Draw GAMEPLAY screen here!
    DrawRectangle(startX, startY, width, height, color);
    Vector2 pos = {20, 100};
    Vector2 lineSpacing = {0, 40};

    // Circloid count display
    char text[32]; // Buffer to hold the text
    snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
    DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // FPS display 
    snprintf(text, sizeof(text), "FPS: %.1f", GetFps().fps); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // Memory display
    snprintf(text, sizeof(text), "Memory (bytes): %i", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // Update FPS
    // DrawTextEx(font, fpsText, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize*2.0f, 2, GREEN);
}

// Gameplay Screen - Main stage Draw
void DrawGameplayScreenStage(int startX, int startY, int width, int height, Color color)
{
    // Stage canvas for circloids to interact on
    DrawRectangle(startX, startY, width, height, color);

    // Draw circloids here
    DrawCircloids();
}

void DrawCircloids(void) 
{
    Circloid *circloid = Enumerate(circloids);
    while (circloid != NULL) 
    {
        DrawCircle(circloid->object.pos.x, circloid->object.pos.y, circloid->radius, DARKBROWN);

        circloid = Enumerate(circloids);
    }   
    ResetEnumerator(circloids); // Reset enumerator after drawing
}

int GetCircloidCount(void) 
{
    return 0;
}


// Gameplay Screen Unload logic
void UnloadGameplayScreen(void)
{
    // TODO: Unload GAMEPLAY screen variables here!
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void)
{
    return finishScreen;
}

// // Gameplay Screen Draw logic
// void DrawGameplayScreen(void)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
//     Vector2 pos = { 20, 100 };
//     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
//     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// }