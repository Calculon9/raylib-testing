/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef COORDINATE_SPACE_H
#define COORDINATE_SPACE_H
#include "common/common.h"
#include "math/cvectors.h"
#include "colour/colour.h"
#include "physics/newtonoid.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define MAX_CELL_OCCUPANCY 12

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef struct Cell
{
    Vector2d local_origin;              // Cell origin in the owning CoordSpace local frame
    Vector2d local_center;              // Cell center in the owning CoordSpace local frame
    int object_ids[MAX_CELL_OCCUPANCY]; // array of object ids that are currently occupying this cell (if any)
    int occupancy;
    float value; // value representing the properties of the field at this cell (e.g., occupied, empty, etc.)
} Cell;

typedef struct LineSegment2d
{
    Vector2d start;
    Vector2d end;
} LineSegment2d;

// A bare-bones coordinate space with no associated object or physicality. Use for describing a logical child grid space.
// coords_origin is this child space origin in its own local frame. Parent-space placement is stored externally
// (for example, a world center in universe space) and mapped via transform helpers.
// 2. THE SIMULATION DATA CONTAINER (Belongs in world.h / grid.h)
// This is the domain object that represents your physical game worlds.
typedef struct CoordSpace2d {
    CoordSystem2d system;       // Math representation! Brings basis, tracking, and boundaries
    Vector2d resolution_ixj;    // Logical dimensions in local i/j units
    DArray cells;               // The grid data buffer (linear form)
    float unitArea;
    float stepsU; 
    float stepsV; 
} CoordSpace2d;

// 3. THE VISUAL MANAGER LAYER
typedef struct CoordSpace2d_Grid {
    CoordSpace2d coord_space;
    ColourRgba colour_fill;
    ColourRgba colour_line;
    Newtonoid2d object;
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
Vector2d CalcCoordSpaceHalfExtent(const CoordSpace2d *space);
// Returns child-space origin from a center coordinate expressed in the same frame as space->basis.
Vector2d CalcCoordSpaceOriginFromCenter(const CoordSpace2d *space, Vector2d center);
Matrix2x2 CalcCoordSpaceBoundsFromCenter(const CoordSpace2d *space, Vector2d center);
Cell *GetCellFromCoords(CoordSpace2d *space, Vector2d coords);
int GetIndexFromCoords(CoordSpace2d *space, Vector2d space_coords);
void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d coord_space_basis, Vector2d out_vertices[4]); // Returns the 4 vertices of the AABB of the object in world coordinates
Matrix2x2 CalcSpaceExtents_2d(CoordSpace2d *space);
Matrix2x2 CalcSpaceAABB(CoordSpace2d *space);
bool VectorIsInSpace_2d(Vector2d vector, CoordSpace2d *space);
// Matrix2x2 GetObjectFootprint_AsBox(Basis2d coord_space_basis, Surface2d object_surface);
//  CoordinateSpace2d CreateCoordinateSpace(Rectangloid object, int rows, int columns, ColourRgba lineColour);

// Vector2d GetCellIndicesFromCoordinates(Vector2d input_coordinates, Basis2d basis);
//  void UpdateUnitCellValues(CoordinateSpace2d *coordinate_space);
// void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif