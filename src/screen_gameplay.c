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
    position_field = CreateField(rectangloid_container, 3, 3, fieldLineColour, circloids);

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
    if (keyDown)
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
    ColourRgba colour = BEIGE_RGBA;
    Vector2d pos = {posX, posY};
    Velocity2d velocity = {(Vector2d){0.0f, 40.0f}, 0.0f, 0.0f};
    Acceleration2d acceleration = {(Vector2d){0.0f, 0.0f}, 0.0f, 0.0f};
    Surface2d surface = {0};
    surface.surface_vectors = *NewDynamicArray(8, sizeof(Vector2d)); // Placeholder surface vectors, not used for circloids but required for creating the NewtonObject
    Circloid newCircloid = CreateCircloid(radius, colour, mass, pos, velocity, acceleration, surface);

    Array_Push(circloids, &newCircloid);
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void)
{
    // Draw the side panel
    DrawGameplayScreenPanel(0, 0, panelWidth, panelHeight, (Color)BROWN);

    // Draw the stage
    DrawGameplayScreenStage(panelWidth, 0, stageWidth, stageHeight, (Color){stageBackgroundColour.r, stageBackgroundColour.g, stageBackgroundColour.b, stageBackgroundColour.a});
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
    if (circloids == NULL)// || circloids->coll == NULL || circloids->coll->count <= 0)
    {
        return; // No circloids to draw
    }
    Collection *circloid_coll = &circloids->coll;
    for (int i = 0; i < circloid_coll->count; i++)
    {   
        Circloid *circloid = (Circloid *)((char *)circloid_coll->items + (i * circloid_coll->elemSize));
        Vector2d circloidPos = circloid->newtonian_properties.world_position;
        Vector2d cellIndices = GetCellIndicesFromCoordinates(position_field.shape.newtonian_properties.world_position, circloid->newtonian_properties.world_position, position_field.coordinateSpace.basis);

        //TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

        const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)", ((int)cellIndices.x + 1) * ((int)cellIndices.y + 1), (int)cellIndices.x + 1, (int)cellIndices.y + 1, (int)circloidPos.x, (int)circloidPos.y);
        // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

        //Draw circloid THEN text so text is on top
        DrawCircle(circloidPos.x, circloidPos.y, circloid->radius, (Color)DARKBROWN_RGBA);
        DrawTextEx(font, displayText, (Vector2){circloidPos.x - 0.7 * circloid->radius, circloidPos.y - 0.7 * circloid->radius}, font.baseSize, 1, (Color)BEIGE_RGBA);

        // Debug print
        // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
    }
    // Circloid *circloid = Enumerate(circloids->coll);
    // if (circloid == NULL)
    // {
    //     fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
    // }
    // while (circloid != NULL)
    // {
    //     Vector2d pos = circloid->object.pos;
    //     Vector2d cell = GetCellFromCoordinates(position_field, circloid->object.pos);
    //     DrawCircle(pos.x, pos.y, circloid->radius, (Color)DARKBROWN_RGBA);

    //     // Output the circloid's position and cell as text on top of it
    //     const char *cellText = TextFormat("Cell: (%d,%d)", (int)cell.x, (int)cell.y);
    //     const char *posText = TextFormat("Coord: (%d,%d)", (int)pos.x, (int)pos.y);
    //     const char *allText = TextFormat("%s\n%s", cellText, posText);
    //     DrawTextEx(font, allText, (Vector2){pos.x, pos.y}, font.baseSize, 1, (Color)BEIGE_RGBA);
    //     // DrawTextEx(font, cellText, (Vector2){pos.x - (circloid->radius / 2), pos.y - (circloid->radius / 2)}, font.baseSize, 1, (Color)DARKGREEN_RGBA);

    //     circloid = Enumerate(circloids->coll);
    // }
    // ResetEnumerator(circloids->coll); // Reset enumerator after drawing
}

void DrawFields_Rect(void)
{
    if (position_field.coordinateSpace.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
    {
        return; // No field to draw
    }
    // Draw background
    DrawRectangle(position_field.shape.newtonian_properties.world_position.x, position_field.shape.newtonian_properties.world_position.y, position_field.shape.width, position_field.shape.height, (Color){fieldBackgroundColour.r, fieldBackgroundColour.g, fieldBackgroundColour.b, fieldBackgroundColour.a});

    int rows = position_field.coordinateSpace.rows;
    int cols = position_field.coordinateSpace.columns;
    int totalUnits = rows * cols;
    CoordinateSpace coordinateSpace = position_field.coordinateSpace;
    ColourRgba colour = position_field.lineColour;
    Color color = (Color){colour.r, colour.g, colour.b, colour.a};

    // Draw "Horizontal-ish" lines (Rows)
    // These lines start at (origin + r*v) and end at (origin + r*v + cols*u)
    Vector2d origin = position_field.shape.newtonian_properties.world_position;
    for (int r = 0; r < coordinateSpace.lineSegments_u.coll.count; r++)
    {
        LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_u.coll.items + (r * coordinateSpace.lineSegments_u.coll.elemSize));
        DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
    }

    // Draw "Vertical-ish" lines (Columns)
    // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
    for (int c = 0; c < coordinateSpace.lineSegments_v.coll.count; c++)
    {
        LineSegment2d *segment = (LineSegment2d *)((char *)coordinateSpace.lineSegments_v.coll.items + (c * coordinateSpace.lineSegments_v.coll.elemSize));
        DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
    }

    // Draw field unit values as text on top of each field unit
    Collection *cells = &position_field.coordinateSpace.cells.coll;
    // int textOffsetX = (position_field.coordinateSpace.basis.u.x + position_field.coordinateSpace.basis.v.x) / 2;
    // int textOffsetY = (position_field.coordinateSpace.basis.u.y + position_field.coordinateSpace.basis.v.y) / 2;
    for (int i = 0; i < totalUnits; i++)
    {
        int row = i / cols;
        int col = i % cols;
        Cell *cell = (Cell *)((char *)cells->items + (i * cells->elemSize));
        Vector2d cellPos = cell->coordinates;
        const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)\nValue: %.1f", i + 1, row + 1, col + 1, (int)cellPos.x, (int)cellPos.y, cell->value);
        // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
        DrawTextEx(font, displayText, (Vector2){cellPos.x, cellPos.y}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

        // Debug print
        // printf("Cell %d [Row %d, Col %d] Value: %.1f\n", i + 1, row, col, cell->value);
    }
    // printf("Drew %d cells\n", count);
    //  Use the number of rows, columns, and their dimensions to draw field lines as rectangles
    //  Method 1: Enumerate the grid
    //  float *cell = Enumerate(position_field.cells->coll);
    //  if (cell == NULL)
    //  {
    //      fprintf(stderr, "Failed to retrieve enumerated field unit\n"); // Enumerator failed to retrieve the first item
    //  }
}

void UpdateObjectVectors()
{
    // Update Circloids
    if (circloids == NULL || circloids->coll.count <= 0)
    {
        return; // No circloids to update
    }
    Circloid *circloid = Enumerate(&circloids->coll);
    if (circloid == NULL)
    {
        fprintf(stderr, "Failed to retrieve enumerated Circloid\n"); // Enumerator failed to retrieve the first item
    }
    while (circloid != NULL)
    {
        if (&circloid->newtonian_properties != NULL)
        {
            CalculateVectors(&circloid->newtonian_properties, GetFrameDeltaTime());
        }
        circloid = Enumerate(&circloids->coll);
    }
    ResetEnumerator(&circloids->coll); // Reset enumerator after drawing

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
    return circloids->coll.count;
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