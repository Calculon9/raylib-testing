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
#include "physics/rectangloid.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

// Dimensions of each Window component (calculated as necessary in InitGameplayScreen)
int panelWidth = 250;
int panelHeight = 0;
int stageWidth = 0;
int stageHeight = 0;

// static int framesCounter = 0;
static int finishScreen = 0;
static int initObjectCount = 8;
static DynamicArray *circloids = NULL; // = {0};
static Rectangloid container = {0};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void DrawCircloids();
int GetCircloidCount(void);
void UpdateObjectVectors();
void AddStockCircloid_Moving(int posX, int posY);

// Gameplay Screen Initialization logic
void InitGameplayScreen(void)
{
    // Initialise Window dimensins based on current screen size
    panelHeight = GetScreenHeight();
    stageWidth = GetScreenWidth() - panelWidth;
    stageHeight = panelHeight;

    // Initialise Objects
    circloids = NEW_DYNAMIC_ARRAY(initObjectCount, Circloid);
    container = CreateRectangloid(stageHeight, stageWidth, (ColourRgba){0, 0, 0, 0}, 0, (Vector2d){stageWidth / 2, stageHeight / 2}, (Velocity2d){0.0f, 0.0f}, (Acceleration2d){0.0f, 0.0f});

    // Add 1 Circloid in the middle of container to start with
    AddStockCircloid_Moving(stageWidth / 2, stageHeight / 2);

    // Initialise utilities (FPS tracking, etc.)
    InitUtilities();

    // framesCounter = 0;
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
void UpdateGameplayScreenPanel(void)
{

    UpdateUtilities();
}

void UpdateObjectVectors()
{
    // Update Circloids
    if (circloids == NULL || circloids->coll == NULL || circloids->coll->count <= 0)
    {
        return; // No circloids to update
    }
    Circloid *circloid = Enumerate(circloids->coll);
    if (circloid == NULL)
    {
        fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
    }
    while (circloid != NULL)
    {
        if (&circloid->object != NULL)
        {
            CalculateVectors(&circloid->object, GetFrameDeltaTime());
        }
        circloid = Enumerate(circloids->coll);
    }
    ResetEnumerator(circloids->coll); // Reset enumerator after drawing

    // Update Container
}

// Gameplay Screen Stage Update logic
void UpdateGameplayScreenStage(void)
{

    // Update vectors of all objects
    UpdateObjectVectors();

    // Draw a circle where the mouse clicks and add it to the state
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // Add a new circloid to the state with the position of the mouse click - give an initial velocity
        AddStockCircloid_Moving(GetMouseX(), GetMouseY());
        // float radius = 24.0f;
        // float mass = 1.0f;
        // ColourRgba colour = {76, 63, 47, 200};
        // Vector2d pos = {GetMouseX(), GetMouseY()};
        // Velocity2d velocity = {(Vector2d){0.0f, 5.0f}, 0.0f, 0.0f};
        // Acceleration2d acceleration = {(Vector2d){0.0f, 5.0f}, 0.0f, 0.0f};
        // Circloid newCircloid = CreateCircloid(radius, colour, mass, pos, velocity, acceleration);
        // newCircloid.object.pos.x = GetMouseX();
        // newCircloid.object.pos.y = GetMouseY();
        // newCircloid.radius = 24;

        // Add a new circloid to the state with the position of the mouse click
        //Array_Push(circloids, &newCircloid);

        // finishScreen = 1;
        // PlaySound(fxCoin);
    }
}

void AddStockCircloid_Moving(int posX, int posY)
{
    float radius = 24.0f;
    float mass = 1.0f;
    ColourRgba colour = {76, 63, 47, 200};
    Vector2d pos = {posX, posY};
    Velocity2d velocity = {(Vector2d){0.0f, 20.0f}, 0.0f, 0.0f};
    Acceleration2d acceleration = {(Vector2d){0.0f, 0.0f}, 0.0f, 0.0f};
    Circloid newCircloid = CreateCircloid(radius, colour, mass, pos, velocity, acceleration);
   
    Array_Push(circloids, &newCircloid);
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void)
{
    // int panel_width = 250;
    // int panel_height = GetScreenHeight();
    // int stage_width = GetScreenWidth() - panel_width;
    // int stage_height = GetScreenHeight();

    // Draw the side panel
    DrawGameplayScreenPanel(0, 0, panelWidth, panelHeight, (Color)BROWN);

    // Draw the stage
    DrawGameplayScreenStage(panelWidth, 0, stageWidth, stageHeight, (Color)DARKGREEN);
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
    char text[32];                                                     // Buffer to hold the text
    snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
    DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // FPS display
    snprintf(text, sizeof(text), "FPS: %.1f", GetFps().fps); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // Memory display - Total allocated memory in bytes
    snprintf(text, sizeof(text), "Memory (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
    DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);

    // Memory display - Consumed memory in bytes out of the total allocated bytes
    // snprintf(text, sizeof(text), "Memory Consumed (bytes): %zu", GetCurrentMemoryAllocated()); // Format the FPS value into the buffer
    // DrawTextEx(font, text, (Vector2){pos.x + lineSpacing.x, pos.y + 2 * lineSpacing.y}, font.baseSize * 2.0f, 2, (Color)BEIGE);
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
    if (circloids == NULL || circloids->coll == NULL || circloids->coll->count <= 0)
    {
        return; // No circloids to draw
    }
    Circloid *circloid = Enumerate(circloids->coll);
    if (circloid == NULL)
    {
        fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
    }
    while (circloid != NULL)
    {
        DrawCircle(circloid->object.pos.x, circloid->object.pos.y, circloid->radius, (Color){76, 63, 47, 200});
        // DrawCircleLines(circloid->object.pos.x, circloid->object.pos.y, circloid->radius, MAROON);

        circloid = Enumerate(circloids->coll);
    }
    ResetEnumerator(circloids->coll); // Reset enumerator after drawing
}



int GetCircloidCount(void)
{
    return circloids->coll->count;
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