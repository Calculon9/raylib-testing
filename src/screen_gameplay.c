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
#include "physics/field.h"
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
static Field position_field = {0};
static ColourRgba fieldBackgroundColour = {110, 175, 215, 255};
static ColourRgba fieldLineColour = {250, 230, 60, 125};
static ColourRgba stageBackgroundColour = {225, 225, 225, 255};
static Rectangloid rectangloid_container = {0};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void DrawCircloids();
int GetCircloidCount(void);
void UpdateObjectVectors();
void DrawFields_Rect(void);
void AddStockCircloid_Moving(int posX, int posY);

// Gameplay Screen Initialization logic
void InitGameplayScreen(void)
{
    // Initialise Window dimensions based on current screen size
    panelHeight = GetScreenHeight();
    stageWidth = GetScreenWidth() - panelWidth;
    stageHeight = panelHeight;

    // Initialise Objects
    circloids = NEW_DYNAMIC_ARRAY(initObjectCount, Circloid);

    // Initialise Fields
    rectangloid_container = CreateRectangloid_Static(stageHeight, stageWidth, fieldBackgroundColour, (Vector2d){panelWidth, 0}); // position will start at the end of the panel and take up the rest of the screen, so that it only applies to the stage
    position_field = CreateField(rectangloid_container, 8, 8, fieldLineColour, circloids);

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

// Gameplay Screen Stage Update logic
void UpdateGameplayScreenStage(void)
{

    // Update vectors of all objects
    // DEBUGGING - we will update object vectors if button is pressed
    bool keyDown = IsKeyDown(KEY_LEFT_CONTROL);
    //if (keyDown)
    {
        UpdateObjectVectors();
    }

    // Update Fields

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
        // Array_Push(circloids, &newCircloid);

        // finishScreen = 1;
        // PlaySound(fxCoin);
    }
}

void AddStockCircloid_Moving(int posX, int posY)
{
    float radius = 50.0f;
    float mass = 2.0f;
    ColourRgba colour = {76, 63, 47, 200};
    Vector2d pos = {posX, posY};
    Velocity2d velocity = {(Vector2d){0.0f, 10.0f}, 0.0f, 0.0f};
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
    DrawGameplayScreenStage(panelWidth, 0, stageWidth, stageHeight, (Color){stageBackgroundColour.r, stageBackgroundColour.g, stageBackgroundColour.b, stageBackgroundColour.a});
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

    // Draw fields here
    DrawFields_Rect();

    // Draw circloids last so that they are on top of the fields
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
        Vector2d pos = circloid->object.pos;
        Vector2d cell = GetCellFromWorld(position_field, circloid->object.pos);
        DrawCircleLines(pos.x, pos.y, circloid->radius, (Color){76, 63, 47, 200});

        // Output the circloid's position and cell as text on top of it
        DrawTextEx(font, TextFormat("(%d,%d)", pos.x, pos.y), (Vector2){pos.x - (circloid->radius / 2), pos.y - (circloid->radius / 2)}, font.baseSize, 1, (Color)BEIGE);
        DrawTextEx(font, TextFormat("Cell: (%d,%d)", (int)cell.x, (int)cell.y), (Vector2){pos.x - (circloid->radius / 2), pos.y + (circloid->radius / 2)}, font.baseSize, 1, (Color)BEIGE);
        // DrawCircleLines(circloid->object.pos.x, circloid->object.pos.y, circloid->radius, MAROON);

        circloid = Enumerate(circloids->coll);
    }
    ResetEnumerator(circloids->coll); // Reset enumerator after drawing
}

void DrawFields_Rect(void)
{
    if (position_field.grid == NULL || position_field.grid->coll == NULL) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
    {
        return; // No field to draw
    }
    // Draw background
    DrawRectangle(position_field.shape.object.pos.x, position_field.shape.object.pos.y, position_field.shape.width, position_field.shape.height, (Color){fieldBackgroundColour.r, fieldBackgroundColour.g, fieldBackgroundColour.b, fieldBackgroundColour.a});

    int rows = position_field.gridSpace.rows;
    int cols = position_field.gridSpace.columns;
    GridSpace gridSpace = position_field.gridSpace;
    ColourRgba colour = position_field.lineColour;
    Color color = (Color){colour.r, colour.g, colour.b, colour.a};

    // Draw "Horizontal-ish" lines (Rows)
    // These lines start at (origin + r*v) and end at (origin + r*v + cols*u)

    //-----SEGMENTATION FAULT HERE--------- (lineSegments are not being generated properly in CalculateField)--------------------------------Actuall fixed it most likely (weren't returning the updated field with the line segments in CalculateField) but will keep an eye on it just in case
    //ALSO create a separate CoordinateSpace struct that contains the basis vectors and line segments for drawing the field, so that we can keep the Field struct focused on just the field properties and values, and have a separate struct for the coordinate space representation of the field for drawing purposes - this will also make it easier to manage multiple fields with different coordinate spaces if needed in the future
    Vector2d origin = position_field.shape.object.pos;
    for (int r = 0; r < gridSpace.lineSegments_u->coll->count; r++)
    {
        LineSegment2d *segment = (LineSegment2d *)((char *)gridSpace.lineSegments_u->coll->items + (r * gridSpace.lineSegments_u->coll->elemSize));
        DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
    }

    // Draw "Vertical-ish" lines (Columns)
    // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
    for (int c = 0; c < gridSpace.lineSegments_v->coll->count; c++)
    {
        LineSegment2d *segment = (LineSegment2d *)((char *)gridSpace.lineSegments_v->coll->items + (c * gridSpace.lineSegments_v->coll->elemSize));
        DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
        // Vector2d start = {
        //     origin.x + c * position_field.gridSpace.basis.u.x,
        //     origin.y + c * position_field.gridSpace.basis.u.y};
        // Vector2d end = {
        //     start.x + rows * position_field.gridSpace.basis.v.x,
        //     start.y + rows * position_field.gridSpace.basis.v.y};
    }

    // for (int r = 0; r < position_field.rows; r++)
    // {
    //     for (int c = 0; c < position_field.columns; c++)
    //     {
    //         // Calculate exact pixel positions
    //         float drawX = position_field.shape.object.pos.x + (c * position_field.unit_vect.x);
    //         float drawY = position_field.shape.object.pos.y + (r * position_field.unit_vect.y);

    //         DrawRectangleLines(
    //             (int)drawX,
    //             (int)drawY,
    //             (int)position_field.unit_vect.x,
    //             (int)position_field.unit_vect.y,
    //             (Color){255, 0, 0, 150});
    //     }
    // }

    // Use the number of rows, columns, and their dimensions to draw field lines as rectangles
    // Method 1: Enumerate the grid
    // float *cell = Enumerate(position_field.grid->coll);
    // if (cell == NULL)
    // {
    //     fprintf(stderr, "Failed to retrieve enumerated field unit\n"); // Enumerator failed to retrieve the first item
    // }
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

void UpdateFields()
{
    // Update the collision/position field

    // Update Circloids
    // if (circloids == NULL || circloids->coll == NULL || circloids->coll->count <= 0)
    // {
    //     return; // No circloids to update
    // }
    // Circloid *circloid = Enumerate(circloids->coll);
    // if (circloid == NULL)
    // {
    //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
    // }
    // while (circloid != NULL)
    // {
    //     if (&circloid->object != NULL)
    //     {
    //         CalculateVectors(&circloid->object, GetFrameDeltaTime());
    //     }
    //     circloid = Enumerate(circloids->coll);
    // }
    // ResetEnumerator(circloids->coll); // Reset enumerator after drawing

    // Update Container
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