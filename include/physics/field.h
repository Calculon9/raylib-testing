/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef FIELD_H
#define FIELD_H
#include <stddef.h>
#include "math/cvectors.h"
#include "collections/dynamic_array.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
// #define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct Cell {
    Vector2d position; // position of the cell in world coordinates
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Cell;

typedef struct {
    Vector2d u; // The "Right-ish" step
    Vector2d v; // The "Down-ish" step
} Basis2d;

typedef struct LineSegment2d {
    Vector2d start;
    Vector2d end;
} LineSegment2d;

typedef struct GridSpace {
    Basis2d basis; // basis vectors representing the direction and length of one step to the right and down respectively
    DynamicArray *lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
    DynamicArray *lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
    float rows, columns; // number of rows and columns in the field grid
} GridSpace;

// typedef struct WorldSpace {
//     Vector2d position; // position of the cell in world coordinates
//     float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
// } WorldSpace;

typedef struct Field
{
    Rectangloid shape;  // shape
    ColourRgba lineColour; // colour of the field lines (if applicable)
    GridSpace gridSpace; // the grid space of the field, containing the basis vectors and line segments for drawing the field (if applicable)
    DynamicArray *grid; // grid (in linear form) of field units, with values representing the field's properties at that unit (e.g., occupied, empty, etc.)
    DynamicArray *items; // objects that will be mapped to & interacting with the field
} Field;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
Field CreateField(Rectangloid object, int rows, int columns, ColourRgba lineColour, DynamicArray *items);
Field CalculateField(Field field);
Vector2d GetCellFromWorld(Field field, Vector2d objectPos);
// Rectangloid CreateRectangloid_FromObject(NewtonObject2d newtOb, float height, float width, ColourRgba colour);
void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif