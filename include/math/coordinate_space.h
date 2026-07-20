/**********************************************************************************************
*
CIRCLOID MODULE
*
**********************************************************************************************/
#ifndef COORDINATE_SPACE_H
#define COORDINATE_SPACE_H
#include "common/common.h"
#include "math/cvectors.h"
#include "math/affine_space_ops.h"
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
// THE SIMULATION DATA CONTAINER (Belongs in world.h / grid.h)
// This is the domain object that represents your physical game worlds.
typedef struct Space2d {
    Frame2d frame;       // Math representation! Brings basis, tracking, and boundaries
    int rows;
    int columns;
    DArray cells;               // The grid data buffer (linear form)
    float unitArea;
} Space2d;

// THE VISUAL MANAGER LAYER
typedef struct GridSpace2d {
    Space2d space;
    ColourRgba colour_fill;
    ColourRgba colour_line;
    Newtonoid2d object;
} GridSpace2d;

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
GridSpace2d NewGridSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line);
Space2d NewSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis);
Cell *GetCellFromCoords(Space2d *space, Vector2d local_coords);
int GetIndexFromCoords(Space2d *space, Vector2d local_coords);
void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d space_basis, Vector2d out_vertices[4]); // Returns the 4 vertices of the AABB of the object in world coordinates
Frame2d CreateFrame2d(Basis2d basis, Vector2d origin_in_parent, Vector2d local_resolution);
//Vector2d CalcSpaceHalfExtent(const Space2d *space);
//Vector2d CalcSpaceOriginFromCenter(const Space2d *space, Vector2d center);
//Matrix2x2 CalcSpaceExtents_2d(Space2d *space);
//Matrix2x2 CalcSpaceBoundsFromCenter(const Space2d *space, Vector2d center);
// Matrix2x2 CalcSpaceExtents_2d(Space2d *space);
// Matrix2x2 CalcSpaceAABB(Space2d *space);
// bool VectorIsInSpace_2d(Vector2d vector, Space2d *space);
// Matrix2x2 GetObjectFootprint_AsBox(Basis2d space_basis, Surface2d object_surface);
//  CoordinateSpace2d CreateCoordinateSpace(Rectangloid object, int rows, int columns, ColourRgba lineColour);

// Vector2d GetCellIndicesFromCoordinates(Vector2d input_coordinates, Basis2d basis);
//  void UpdateUnitCellValues(CoordinateSpace2d *coordinate_space);
// void Field_Rect_GetCollisionObjects(Rectangloid rect);
#endif

