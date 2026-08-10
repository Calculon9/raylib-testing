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
#include "physics/newtonoid.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------
#define MAX_CELL_OCCUPANCY 12

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef enum FrameGeometryType
{
    GRID_GEOMETRY_REGULAR,
    GRID_GEOMETRY_SHEARED_Y,
    GRID_GEOMETRY_SHEARED_X,
    GRID_GEOMETRY_ISOMETRIC,
    GRID_GEOMETRY_PERSPECTIVE,
    GRID_GEOMETRY_RADIAL
} FrameGeometryType;

typedef struct CoordinateSpacePreset
{
    FrameGeometryType geometry;
    Vector2d resolution;
    Basis2d basis;
} CoordinateSpacePreset;

#define COORDINATE_SPACE_DEFAULT_RESOLUTION ((Vector2d){7.0f, 5.0f})

typedef struct Cell
{
    Vector2d local_origin;              // Cell origin in the owning CoordSpace local frame
    EntityId object_ids[MAX_CELL_OCCUPANCY]; // entity IDs currently occupying this cell (if any)
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

//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_REGULAR;
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_Y;
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_X;
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_ISOMETRIC;
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_PERSPECTIVE;
extern const CoordinateSpacePreset COORDINATE_SPACE_PRESET_RADIAL;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
GridSpace2d NewGridSpace2d_FromPreset(Vector2d origin_in_parent, CoordinateSpacePreset preset, ColourRgba colour_fill, ColourRgba colour_line);
GridSpace2d NewGridSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line);
Space2d NewSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis);
void RebuildSpaceCells(Space2d *space);
Cell *GetCellFromCoords(Space2d *space, Vector2d local_coords);
int GetIndexFromCoords(Space2d *space, Vector2d local_coords);
void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Basis2d space_basis, Vector2d out_vertices[4]); // Returns the 4 vertices of the AABB of the object in world coordinates
Frame2d CreateFrame2d(Basis2d basis, Vector2d origin_in_parent, Vector2d local_resolution);

#endif

