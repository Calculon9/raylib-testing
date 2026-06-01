// /**********************************************************************************************
// *
// CIRCLOID MODULE
// *
// **********************************************************************************************/
// #ifndef FIELD_H
// #define FIELD_H
// #include "common/common.h"
// #include "math/cvectors.h"
// #include "collections/dynamic_array.h"
// #include "physics/rectangloid.h"

// //----------------------------------------------------------------------------------
// // Macros and Defines
// //----------------------------------------------------------------------------------
// // #define NEW_CIRCLOID() AllocateBytes(sizeof(Circloid))
// // #define NEW_CIRCLOID() allocate_block(sizeof(Circloid))

// //----------------------------------------------------------------------------------
// // Types and Structures Definition
// //----------------------------------------------------------------------------------

// typedef struct UnitCell {
//     Vector2d coordinates; // coordinates of the cell in world coordinates
//     int occupancy;
//     float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
// } UnitCell;

// typedef struct LineSegment2d {
//     Vector2d start;
//     Vector2d end;
// } LineSegment2d;

// typedef struct CoordinateSpace {
//     Basis2d basis; // basis vectors representing the direction and length of one step to the right and down respectively
//     DynamicArray lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
//     DynamicArray lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
//     DynamicArray cells; // the cells or field units within the coordinate space (in linear form) of field units
//     float rows, columns; // number of rows and columns in the coordinate space
// } CoordinateSpace;

// // typedef struct WorldSpace {
// //     Vector2d position; // position of the cell in world coordinates
// //     float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
// // } WorldSpace;

// typedef struct Field
// {
//     Rectangloid shape;  // shape
//     ColourRgba lineColour; // colour of the field lines (if applicable)
//     CoordinateSpace coordinateSpace; // the coordinate space of the field, containing the basis vectors and line segments for drawing the field (if applicable)   
//     DynamicArray items; // objects that will be mapped to & interacting with the field
// } Field;

// //----------------------------------------------------------------------------------
// // Global Variables Declaration (shared by several modules)
// //----------------------------------------------------------------------------------

// //----------------------------------------------------------------------------------
// // Module Functions Declaration
// //----------------------------------------------------------------------------------
// Field CreateField(Rectangloid object, int rows, int columns, ColourRgba lineColour, DynamicArray *items);
// Field CalculateFieldLines(Field field);
// Field InitialiseFieldCells(Field field);
// Vector2d GetCellIndicesFromCoordinates(Vector2d origin_coordinates, Vector2d input_coordinates, Basis2d basis);
// Field UpdateFieldCellValues(Field field);
// //void Field_Rect_GetCollisionObjects(Rectangloid rect);
// #endif