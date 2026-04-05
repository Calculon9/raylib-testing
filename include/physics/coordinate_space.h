/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef COORDINATE_SPACE_H
#define COORDINATE_SPACE_H
#include "common/common.h"
#include "math/cvectors.h"
#include "collections/dynamic_array.h"
#include "physics/rectangloid.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
// #define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
// #define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct Cell {
    Vector2d world_coordinates; // coordinates of the cell in world coordinates
    int occupancy;
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Cell;

typedef struct LineSegment2d {
    Vector2d start;
    Vector2d end;
} LineSegment2d;

typedef struct CoordinateSpace2d {
    ColourRgba lineColour;
    Rectangloid object; 
    Basis2d basis; // basis vectors representing the direction and length of one step to the right and down respectively
    DynamicArray lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
    DynamicArray lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
    DynamicArray cells; // the cells or field units within the coordinate space (in linear form) of field units
    float rows, columns; // number of rows and columns in the coordinate space
} CoordinateSpace2d;

// typedef struct WorldSpace {
//     Vector2d position; // position of the cell in world coordinates
//     float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
// } WorldSpace;

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
CoordinateSpace2d CreateCoordinateSpace(Rectangloid object, int rows, int columns, ColourRgba lineColour);

//Vector2d GetCellIndicesFromCoordinates(Vector2d input_coordinates, Basis2d basis);
// void UpdateUnitCellValues(CoordinateSpace2d *coordinate_space);
//void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif