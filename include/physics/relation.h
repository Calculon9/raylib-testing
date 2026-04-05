/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef RELATION_H
#define RELATION_H
#include "common/common.h"
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
enum Objects{
    NEWTON_OBJECT,
    
    // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
};

typedef struct Rule {
    Vector2d coordinates; // coordinates of the cell in world coordinates
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Rule;

typedef struct Basis2d {
    Vector2d u; // The "Right-ish" step
    Vector2d v; // The "Down-ish" step
} Basis2d;

typedef struct LineSegment2d {
    Vector2d start;
    Vector2d end;
} LineSegment2d;

typedef struct CoordinateSpace {
    Basis2d basis; // basis vectors representing the direction and length of one step to the right and down respectively
    DynamicArray *lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
    DynamicArray *lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
    DynamicArray *cells; // the cells or field units within the coordinate space (in linear form) of field units
    float rows, columns; // number of rows and columns in the coordinate space
} CoordinateSpace;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// Field CreateField(Rectangloid object, int rows, int columns, ColourRgba lineColour, DynamicArray *items);
// Field CalculateFieldLines(Field field);
// Field InitialiseFieldCells(Field field);
// Vector2d GetCellIndicesFromCoordinates(Field field, Vector2d objectPos);
// Field UpdateFieldCellValues(Field field);
//void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif