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
#define MAX_CELL_OCCUPANCY 12


//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct Cell
{
    Vector2d coords; // coordinates of the cell in world coordinates
    int object_ids[MAX_CELL_OCCUPANCY]; // array of object ids that are currently occupying this cell (if any)
    int occupancy;
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Cell;

typedef struct LineSegment2d
{
    Vector2d start;
    Vector2d end;
} LineSegment2d;

// A bare-bones coordinate space with no associated object or physicality. Use for describing a logical grid space.
typedef struct CoordSpace2d
{
    Vector2d coords_origin;
    // Vector2d coords_end;         // This can simply be calculated from the resolution
    Vector2d resolution_ixj;        // The dimensions of the coordinate space in terms of how many units it has in the i and j directions, which we can use to calculate the number of lines and cells needed to fill the space
    Basis2d basis;                  // basis vectors representing the direction and length of one step to the right and down respectively
    DynamicArray cells;            // the cells or field units within the coordinate space (in linear form) of field units
    float unitArea, stepsU, stepsV; // rows, columns; // number of rows and columns in the coordinate space
} CoordSpace2d;

typedef struct CoordSpace2d_Grid
{
    CoordSpace2d coord_space;
    ColourRgba colour_fill;
    ColourRgba colour_line;
    NewtonObject2d object;
    //DynamicArray cells;            // the cells or field units within the coordinate space (in linear form) of field units
    // DynamicArray lineSegments_u; // array of line segments representing the "horizontal" lines of the field (if applicable)
    // DynamicArray lineSegments_v; // array of line segments representing the "vertical" lines of the field (if applicable)
} CoordSpace2d_Grid;



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
CoordSpace2d_Grid NewCoordSpace2d_Grid(Vector2d origin, Vector2d resolution_ixj, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line);
CoordSpace2d NewCoordSpace2d(Vector2d origin, Vector2d resolution_ixj, Basis2d basis);
Cell* GetCellFromCoords(CoordSpace2d *space, Vector2d coords);
int GetIndexFromCoords(CoordSpace2d *space, Vector2d space_coords);
// CoordinateSpace2d CreateCoordinateSpace(Rectangloid object, int rows, int columns, ColourRgba lineColour);

// Vector2d GetCellIndicesFromCoordinates(Vector2d input_coordinates, Basis2d basis);
//  void UpdateUnitCellValues(CoordinateSpace2d *coordinate_space);
// void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif