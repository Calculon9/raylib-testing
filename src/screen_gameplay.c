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
#include "utility/utility.h"
#include "physics/circloid.h"
#include "physics/rectangloid.h"
#include "physics/polygonoid.h"
#include "physics/field.h"
#include "collections/dynamic_array.h"
#include "common/common.h"
#include "world/world.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------

// Dimensions of each Window component (calculated as necessary in InitGameplayScreen)

// static int framesCounter = 0;
static int finishScreen = 0;
static int initObjectCount = 8;
static DynamicArray *circloids = NULL; // = {0};
static Field position_field = {0};

static ColourRgba stageBackgroundColour = {225, 225, 225, 255};
// static Rectangloid rectangloid = {0};

// ----------WORLD----------
// Properties
static World world = {0};
static Basis2d world_basis = {0};
static Vector2d world_u = {1, 0};
static Vector2d world_v = {0, 1};
static Vector2d resolution_ixj = {10, 10};
static ColourRgba world_bg_colour = {110, 175, 215, 255};
static ColourRgba world_line_colour = {250, 230, 60, 125};
static float gravity = 10;

// Objects
static DynamicArray *polygonoids = NULL;

// ----------SCREEN----------
// Properties
int panelWidth = 250;
int panelHeight = 0;
int stageWidth = 0;
int stageHeight = 0;
static Vector2d screen_u = {10, 0};
static Vector2d screen_v = {0, 10};
Matrix3x3 screen_basis_transform = {0};
// static float world_u_to_screen_u = 0;
// static float world_v_to_screen_v = 0;

// Objects

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------
void DrawCircloids();
int GetCircloidCount(void);
int GetPolygonoidCount(void);
void UpdatePolygonoidVectors();
void DrawWorldCoordinateSpace(CoordinateSpace2d world_space);
void DrawFields_Rect(void);
void AddStockCircloid_Moving(int posX, int posY);
Vector2d WorldToScreenCoordinates(Matrix3x3 screen_basis_transform, Vector2d world_coordinates);

// Gameplay Screen Initialization logic
void InitGameplayScreen(void)
{
    // Initialise Window dimensions based on current screen size
    panelHeight = GetScreenHeight();
    stageWidth = GetScreenWidth() - panelWidth;
    stageHeight = panelHeight;

    // Initialise Objects
    polygonoids = NEW_DYNAMIC_ARRAY(initObjectCount, Polygonoid);

    // Create the World
    // 1 . Create the coordinate space for the world
    world_basis = (Basis2d){world_u, world_v};
    Rectangloid world_shape_object = CreateRectangloid_Static(resolution_ixj.y, resolution_ixj.x, world_bg_colour, (Vector2d){0, 0}); // position will start at the end of the panel and take up the rest of the screen, so that it only applies to the stage
    CoordinateSpace2d coord_space = CreateCoordinateSpace(world_shape_object, resolution_ixj, world_basis, world_line_colour);
    world = CreateWorld(coord_space, *polygonoids, gravity);

    // Calculate world-to-screen basis transform matrix
    screen_basis_transform = BasisTransform_2d(world_basis, (Basis2d){screen_u, screen_v},(Vector2d){panelWidth, 0}); // For now we will assume the screen basis is the standard basis, so we are just calculating the transform from world to standard basis. TODO: make this more flexible to allow for different screen bases and transformations.

    // Initialise Fields
    // rectangloid = CreateRectangloid_Static(stageHeight, stageWidth, fieldBackgroundColour, (Vector2d){panelWidth, 0}); // position will start at the end of the panel and take up the rest of the screen, so that it only applies to the stage
    // position_field = CreateField(rectangloid, 3, 3, world_line_colour, circloids);

    // Add 1 Circloid in the middle of container to start with
    // AddStockCircloid_Moving(stageWidth / 2, stageHeight / 2);

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
        // UpdateObjectoidVectors();
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
    char text[32];                 
    snprintf(text, sizeof(text), "Polygonoids: %d", GetPolygonoidCount()); // Format the FPS value into the buffer
    DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);                                    // Buffer to hold the text
    // snprintf(text, sizeof(text), "Circloids: %d", GetCircloidCount()); // Format the FPS value into the buffer
    // DrawTextEx(font, text, pos, font.baseSize * 2.0f, 2, (Color)BEIGE);

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

    // DEBUGGING - Draw the world coordinate space basis vectors to check they are correct
    DrawWorldCoordinateSpace(world.world_space);
    // Draw fields here
    // DrawFields_Rect();

    // Draw circloids last so that they are on top of the fields
    // DrawCircloids();
}

void DrawCircloids(void)
{
    if (circloids == NULL) // || circloids->coll == NULL || circloids->coll->count <= 0)
    {
        return; // No circloids to draw
    }
    Collection *circloid_coll = &circloids->coll;
    for (int i = 0; i < circloid_coll->count; i++)
    {
        Circloid *circloid = (Circloid *)((char *)circloid_coll->items + (i * circloid_coll->elemSize));
        Vector2d circloidPos = circloid->newtonian_properties.world_position;
        Vector2d cellIndices = GetCellIndicesFromCoordinates(position_field.shape.newtonian_properties.world_position, circloid->newtonian_properties.world_position, position_field.coordinateSpace.basis);

        // TODO: If circloid coordinates are negative, it is in the left half of stage then the indices will be negative because the origin of the field is at the top left corner of the stage, so we can check for this and adjust the indices accordingly to get the correct cell

        const char *displayText = TextFormat("Cell: %d (%d,%d)\nCoord: (%d,%d)", ((int)cellIndices.x + 1) * ((int)cellIndices.y + 1), (int)cellIndices.x + 1, (int)cellIndices.y + 1, (int)circloidPos.x, (int)circloidPos.y);
        // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

        // Draw circloid THEN text so text is on top
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

void DrawWorldCoordinateSpace(CoordinateSpace2d world_space)
{
    if (!world_space.cells->coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
    {
        return; // No field to draw
    }
    // Draw background
    DrawRectangle(world_space.object.newtonian_properties.world_position.x,
                  world_space.object.newtonian_properties.world_position.y,
                  world_space.object.width,
                  position_field.shape.height,
                  (Color){world_bg_colour.r, world_bg_colour.g, world_bg_colour.b, world_bg_colour.a});

    // float i_total = world_space.object.width;
    // float j_total = world_space.object.height;
    float cellArea = fabsf((world_space.basis.u.x * world_space.basis.v.y) - (world_space.basis.u.y * world_space.basis.v.x));
    float totalArea = resolution_ixj.x * resolution_ixj.y;

    // Total units needed to fill that area
    int totalUnits = (int)ceilf(totalArea / cellArea);
    ColourRgba colour = world_space.lineColour;
    Color color = (Color){colour.r, colour.g, colour.b, colour.a};

    // Need to convert world coordinates to screen coordinates
    // Draw "Horizontal-ish" lines (j) -
    // These lines start at {(world_space_origin + j*v),0} and end at {(world_space_origin + j*v) + cols*u)}
    Basis2d basis = world_space.basis;

    // The world position of the coordinate space object is the origin of the coordinate space, so (0,0). But to make it more flexible for different coordinate space origins, we will add the world position to the start and end points of the lines to get their actual coordinates in world space, and then convert those to screen coordinates using the basis transform matrix
    Vector2d origin = world_space.object.newtonian_properties.world_position;
    
    // We need to know how many "steps" to take in each direction
    int stepsU = ceilf((float)world_space.resolution_ixj.x / VectorMagnitude_2d(basis.u));
    int stepsV = ceilf((float)world_space.resolution_ixj.y / VectorMagnitude_2d(basis.v));

    //world_space.cells->coll.count = 0; // Reset cell count before drawing

    // 1. Draw "Horizontal-ish" lines (along the U direction)
    // We create a line at every 'v' step that spans the entire 'u' width
    for (int j = 0; j <= stepsV; j++) 
    {
        // Start of line (World Space): Origin + j steps of V
        Vector2d worldStart = {
            origin.x + (j * basis.v.x),
            origin.y + (j * basis.v.y)
        };
        
        // End of line (World Space): worldStart + total width of U
        Vector2d worldEnd = {
            worldStart.x + (stepsU * basis.u.x),
            worldStart.y + (stepsU * basis.u.y)
        };

        // Convert World to Screen (using transform function)
        Vector2d screenStart = WorldToScreenCoordinates(screen_basis_transform, worldStart);
        Vector2d screenEnd = WorldToScreenCoordinates(screen_basis_transform, worldEnd);

        DrawLineV((Vector2){screenStart.x, screenStart.y}, (Vector2){screenEnd.x, screenEnd.y}, color);
    }

    // 2. Draw "Vertical-ish" lines (along the V direction)
    // We create a line at every 'u' step that spans the entire 'v' height
    for (int i = 0; i <= stepsU; i++) 
    {
        // Start of line (World Space): Origin + i steps of U
        Vector2d worldStart = {
            origin.x + (i * basis.u.x),
            origin.y + (i * basis.u.y)
        };
        
        // End of line (World Space): worldStart + total height of V
        Vector2d worldEnd = {
            worldStart.x + (stepsV * basis.v.x),
            worldStart.y + (stepsV * basis.v.y)
        };

        Vector2d screenStart = WorldToScreenCoordinates(screen_basis_transform, worldStart);
        Vector2d screenEnd = WorldToScreenCoordinates(screen_basis_transform, worldEnd);

        DrawLineV((Vector2){screenStart.x, screenStart.y}, (Vector2){screenEnd.x, screenEnd.y}, color);
    }

    // Draw "Horizontal-ish" lines (Rows) in increments of the v basis vector

    // for (int j = 0; j < i_total; j++)
    // {
    //     // Calculate the start and end points of the line segment for this row
    //     Vector2d start = {origin.x + j * basis.v.x, origin.y + j * basis.v.y}; // Start point is the origin plus j steps down the v basis vector
    //     Vector2d end = {start.x + i_total * basis.u.x, start.y + i_total * basis.u.y}; // End point is the start point plus cols steps across the u basis vector

    //     start = WorldToScreenCoordinates(screen_basis_transform, start);
    //     end = WorldToScreenCoordinates(screen_basis_transform, end);

    //     DrawLineV((Vector2){start.x, start.y}, (Vector2){end.x, end.y}, color);
    //     // float startX = WorldToScreenCoordinates(screen_basis_transform, origin); //origin.x + j * (basis.v.x + basis.u.x);
    //     // float startY = origin.y + j * (basis.v.y + basis.u.y);
    //     // float endX = startX + j * (basis.v.x + basis.u.x);
    //     // float endY = origin.y + j * (basis.v.y + basis.u.y);
    //     //LineSegment2d *segment = (LineSegment2d *)((char *)world_space.lineSegments_u.coll.items + (j * world_space.lineSegments_u.coll.elemSize));
    //     //DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
    // }

    // Draw "Vertical-ish" lines (Columns)
    // These lines start at (origin + c*u) and end at (origin + c*u + rows*v)
    // for (int i = 0; c < world_space.lineSegments_v.coll.count; c++)
    // {
    //     Vector2d start = {origin.x + j * basis.v.x, origin.y + j * basis.v.y}; // Start point is the origin plus j steps down the v basis vector
    //     Vector2d end = {start.x + i_total * basis.u.x, start.y + i_total * basis.u.y}; // End point is the start point plus cols steps across the u basis vector

    //     start = WorldToScreenCoordinates(screen_basis_transform, start);
    //     end = WorldToScreenCoordinates(screen_basis_transform, end);

    //     DrawLineV((Vector2){start.x, start.y}, (Vector2){end.x, end.y}, color);
    //     LineSegment2d *segment = (LineSegment2d *)((char *)world_space.lineSegments_v.coll.items + (c * world_space.lineSegments_v.coll.elemSize));
    //     DrawLineV((Vector2){(*segment).start.x, (*segment).start.y}, (Vector2){(*segment).end.x, (*segment).end.y}, color);
    // }

    // Draw field unit values as text on top of each field unit
    Collection *cells = &world_space.cells->coll;
    // int textOffsetX = (position_field.coordinateSpace.basis.u.x + position_field.coordinateSpace.basis.v.x) / 2;
    // int textOffsetY = (position_field.coordinateSpace.basis.u.y + position_field.coordinateSpace.basis.v.y) / 2;
    for (int k = 0; k < totalUnits; k++)
    {
        int i = k / stepsU; // Row index (based on horizontal lines)
        int j = k % stepsV; // Column index (based on vertical lines)
        Cell *cell = (Cell *)((char *)cells->items + (i * cells->elemSize));
        Vector2d cell_world_coords = cell->world_coordinates;
        Vector2d cell_screen_coords = WorldToScreenCoordinates(screen_basis_transform, cell_world_coords);
        const char *displayText = TextFormat("Cell: %d (%d,%d)\nWorldCoord: (%d,%d)\nScreenCoord: (%d,%d)\nValue: %.1f", k + 1, i + 1, j + 1, (int)cell_world_coords.x, (int)cell_world_coords.y, cell->value);
        // DrawTextEx(font, displayText, (Vector2){cellPos.x + textOffsetX, cellPos.y - textOffsetY}, font.baseSize, 1, (Color)DARKBLUE_RGBA);
        DrawTextEx(font, displayText, (Vector2){cell_screen_coords.x, cell_screen_coords.y}, font.baseSize, 1, (Color)DARKBLUE_RGBA);

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

void DrawFields_Rect(void)
{
    if (position_field.coordinateSpace.cells.coll.capacity > 0) // Don't need to check count here because we can still draw the field lines even if there are no items in the field
    {
        return; // No field to draw
    }
    // Draw background
    DrawRectangle(position_field.shape.newtonian_properties.world_position.x, position_field.shape.newtonian_properties.world_position.y, position_field.shape.width, position_field.shape.height, (Color){world_bg_colour.r, world_bg_colour.g, world_bg_colour.b, world_bg_colour.a});

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
        Vector2d cellPos = cell->world_coordinates;
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

void UpdatePolygonoidVectors()
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

int GetPolygonoidCount(void)
{
    return polygonoids->coll.count;
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

// Gameplay Screen should finish
Vector2d WorldToScreenCoordinates(Matrix3x3 basis_transform, Vector2d world_coordinates)
{
    Vector2d screen_coords;

    // 1. Get the "transformation" or "mapping" basis to go from world basis to screen basis.
    // 2. Get the scaling factor to go from world basis magnitude to screen basis magnitude.

    // Since we are using a 3x  matrix for 2D, we treat the 2D point as a 3D vector where z=1. This is a trick called Homogeneous Coordinates that allows the matrix to move (translate) the point, not just rotate or scale it.
    //  Multiply: (Row 1 * WorldColumn)
    //  screenX = (m0 * x) + (m3 * y) + m6
    screen_coords.x = (world_coordinates.x * basis_transform.m0) + (world_coordinates.y * basis_transform.m3) + basis_transform.m6;

    // Multiply: (Row 2 * WorldColumn)
    // screenY = (m1 * x) + (m4 * y) + m7
    screen_coords.y = (world_coordinates.x * basis_transform.m1) + (world_coordinates.y * basis_transform.m4) + basis_transform.m7;

    return screen_coords;

    // //Get the angles of world and screen basis
    // float world_basis_u_rad = VectorRadians_2d(world_basis.u);
    // float world_basis_v_rad = VectorRadians_2d(world_basis.v);
    // float screen_basis_u_rad = VectorRadians_2d(screen_basis.u);
    // float screen_basis_v_rad = VectorRadians_2d(screen_basis.v);

    // // x_s = (world.x * basisU.x) + (world.y * basisV.x) + origin.x
    // screen_coords.x = (world_coordinates.x * screen_basis.u.x) + (world_coordinates.y * screen_basis.v.x) + screen_origin_coordinates.x;

    // // y_s = (world.x * basisU.y) + (world.y * basisV.y) + origin.y
    // screen_coords.y = (world_coordinates.x * screen_basis.u.x) + (world_coordinates.y * screen_basis.v.x) + screen_origin_coordinates.x;

    // // Need to translate the world's basis to screen basis. Therefore need basis_u and basis_v scaling factor
    // float world_basis_u_mag = VectorMagnitude_2d(world_basis.u);
    // float world_basis_v_mag = VectorMagnitude_2d(world_basis.v);

    // float screen_basis_u_mag = VectorMagnitude_2d(screen_basis.u);
    // float screen_basis_v_mag = VectorMagnitude_2d(screen_basis.v);

    // float scale_u = screen_basis_u_mag / world_basis_u_mag;
    // float scale_v = screen_basis_v_mag / world_basis_v_mag;

    // // Scale the world coordinates by the basis scaling factors to get the coordinates in terms of the screen basis
    // screen_coords.x = (world_coordinates.x * scale_u) + screen_origin_coordinates.x;
    // screen_coords.y = (world_coordinates.y * scale_v) + screen_origin_coordinates.y;
    // return screen_coords;
}

// Vector2d WorldToScreenBasisTransformVector(Basis2d world_basis, Basis2d screen_basis, Vector2d screen_origin_coordinates, Vector2d world_coordinates)
// {
//     Vector2d screen_coordinates;

//     // Need to translate the world's basis to screen basis. Therefore need basis_u and basis_v scaling factor
//     float world_basis_u_mag = VectorMagnitude_2d(world_basis.u);
//     float world_basis_v_mag = VectorMagnitude_2d(world_basis.v);

//     float screen_basis_u_mag = VectorMagnitude_2d(screen_basis.u);
//     float screen_basis_v_mag = VectorMagnitude_2d(screen_basis.v);

//     float scale_u = screen_basis_u_mag / world_basis_u_mag;
//     float scale_v = screen_basis_v_mag / world_basis_v_mag;

//     // Scale the world coordinates by the basis scaling factors to get the coordinates in terms of the screen basis
//     screen_coordinates.x = (world_coordinates.x * scale_u) + screen_origin_coordinates.x;
//     screen_coordinates.y = (world_coordinates.y * scale_v) + screen_origin_coordinates.y;
//     return screen_coordinates;
// }

// Vector2d GetCellCoordinates(Field field, Vector2d objectPos)
// {
//    Vector2d origin = field.shape.object.position;
//    Vector2d u = field.coordinateSpace.basis.u;
//    Vector2d v = field.coordinateSpace.basis.v;

//    // // Get position relative to the grid origin
//    // float px = objectPos.x - origin.x;
//    // float py = objectPos.y - origin.y;

//    // // Calculate the Determinant
//    // float det = (u.x * v.y) - (u.y * v.x);

//    // // If determinant is 0, the grid is collapsed (invalid)
//    // if (fabs(det) < 0.0001f)
//    //    return (Vector2d){-1, -1};

//    // // Solve for Grid Coordinates (c, r) using the Inverse Matrix logic
//    // float c = (px * v.y - py * v.x) / det;
//    // float r = (py * u.x - px * u.y) / det;

//    // // Use floor() to get the integer index of the cell
//    // return (Vector2d){floorf(c), floorf(r)};
// }
// // Gameplay Screen Draw logic
// void DrawGameplayScreen(void)
// {
//     // TODO: Draw GAMEPLAY screen here!
//     DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), PURPLE);
//     Vector2 pos = { 20, 100 };
//     DrawTextEx(font, "GAMEPLAY SCREEN", pos, font.baseSize*3.0f, 4, MAROON);
//     DrawText("PRESS ENTER or TAP to JUMP to ENDING SCREEN", 130, 220, 20, MAROON);
// }