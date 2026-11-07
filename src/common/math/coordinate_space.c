/**********************************************************************************************
 *
    INCLUDES/DEFINITIONS
 *
 **********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "physics/newtonoid.h"
#include "math/coordinate_space.h"
#include "world/world_growth.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
const CoordinateSpacePreset COORDINATE_SPACE_PRESET_REGULAR = {
   GRID_GEOMETRY_REGULAR,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_Y = {
   GRID_GEOMETRY_SHEARED_Y,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.5f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_SHEARED_X = {
   GRID_GEOMETRY_SHEARED_X,
   {7.0f, 5.0f},
   {{1.0f, 0.5f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_ISOMETRIC = {
   GRID_GEOMETRY_ISOMETRIC,
   {7.0f, 5.0f},
   {{0.7071f, 0.5f}, {-0.7071f, 0.5f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_PERSPECTIVE = {
   GRID_GEOMETRY_PERSPECTIVE,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

const CoordinateSpacePreset COORDINATE_SPACE_PRESET_RADIAL = {
   GRID_GEOMETRY_RADIAL,
   {7.0f, 5.0f},
   {{1.0f, 0.0f}, {0.0f, 1.0f}}
};

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
// void CalculateLineSegmentVectors(Space2d *space);
void InitUnitCells(Space2d *coordinate_space);

Frame2d CreateFrame2d(Basis2d basis, Vector2d origin_in_parent, Vector2d local_resolution)
{
    return (Frame2d){
        .basis = basis,
        .origin_in_parent = origin_in_parent,
        .local_min = ZERO_VECTOR_2D,
        .local_max = local_resolution
    };
}

// Create UI coordinate-space geometry without allocating simulation cell data.
UISpace2d NewUISpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis)
{
   UISpace2d space = {0};
   space.frame = CreateFrame2d(basis, origin_in_parent, local_resolution);
   space.columns = (int)fmaxf(1.0f, ceilf(local_resolution.x));
   space.rows = (int)fmaxf(1.0f, ceilf(local_resolution.y));
   return space;
}

// Creates a local coordinate space. Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// The origin is in local coordinates of this space, not a parent-space mapped coordinate.
Space2d NewSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis)
{
   Space2d space = {0};
   int columns = (int)fmaxf(1.0f, ceilf(local_resolution.x));
   int rows = (int)fmaxf(1.0f, ceilf(local_resolution.y));
   Vector2d grid_size = {(float)columns, (float)rows};

   space.frame = CreateFrame2d(basis, origin_in_parent,local_resolution);
   //space.frame.local_min = origin_in_parent;
   //space.frame.local_max = VectorSum_2d(origin_in_parent, grid_size);
   space.columns = columns;
   space.rows = rows;
   space.grid_origin = ZERO_VECTOR_2D;
   space.unitArea = fabsf((basis.u.x * basis.v.y) - (basis.u.y * basis.v.x)); // Calculate the 'Area' of a single basis tile; this is the determinant of the basis matrix, which gives us the area of the parallelogram formed by the basis vectors, which is the area of each cell in the coordinate space. We can then divide the total area of the field by this cell area to get the total number of cells needed to fill the field.

   // If cellArea is 0, the basis is invalid (it's a line, not a space)
   if (space.unitArea < 0.0001f)
   {
      fprintf(stderr, "ERROR: Invalid basis vectors. The area of the basis tile is too small (close to zero). Cannot create coordinate space with these basis vectors!");
   }

   // Total units are deterministic from requested i/j resolution.
   int totalUnits = space.columns * space.rows;
   space.cells = MakeDArray(totalUnits, sizeof(Cell));

   InitUnitCells(&space);

   Vector2d basis_u = space.frame.basis.u;
   Vector2d basis_v = space.frame.basis.v;

   // char text[64]; // Buffer to hold the text
   // snprintf(text, sizeof(text), "FIELD INITIALISED:  Dimensions (%d, %d); Units (%d); Basis -> u = [%d,%d], v = [%d,%d].\n", unitW, unitH, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);
   printf("COORD.SPACE INITIALISED:  Dimensions (W:%d, H:%d); Units (%d); Basis -> u = [%0.2f,%0.2f], v = [%0.2f,%0.2f].\n", columns, rows, totalUnits, basis_u.x, basis_u.y, basis_v.x, basis_v.y);

   return space;
}

// Creates a local child coordinate space with physical attributes.
// Basis vectors in most cases (orthogonal dimensions) should be u = {1,0}, v = {0,1}.
// The origin is in the child/local frame; parent-space placement is handled externally.
GridSpace2d NewGridSpace2d(Vector2d origin_in_parent, Vector2d local_resolution, Basis2d basis, ColourRgba colour_fill, ColourRgba colour_line)
{
   GridSpace2d space_obj = {0};
   space_obj.space = NewSpace2d(origin_in_parent, local_resolution, basis);
   space_obj.colour_line = colour_line;
   space_obj.colour_fill = colour_fill;

   Vector2d grid_size = {(float)space_obj.space.columns, (float)space_obj.space.rows};
   Surface2d surface = CreateSurface_Rectangular(grid_size);

   // Calc anchor_position - this is required when creating new objects
   Vector2d anchor_position = (Vector2d){origin_in_parent.x + (grid_size.x / 2.0f), origin_in_parent.y + (grid_size.y / 2.0f)};
   space_obj.object = CreateNewtonoid2d(0.0f, anchor_position, ZERO_VECTOR_2D, ZERO_VECTOR_2D, surface);
   return space_obj;
}

void InitUnitCells(Space2d *space)
{
   DArray *cells = &(space->cells);
   size_t cells_capacity = cells->capacity;
   MemorySet(cells->items, 0, cells->elem_bytes * cells_capacity);

   int columns = space->columns;
   int rows = space->rows;

   int count = 0;
   for (int k = 0; k < columns * rows; k++)
   {
      int i = k / columns; // Row index (based on horizontal lines)
      int j = k % columns; // Column index (based on vertical lines)

      Cell cell = {0}; // Create a new cell and initialize it to zero

      // Warp basis defaults to identity so a freshly built cell is inert (no motion warp).
      // NOTE: warp_basis is authored state (entity occupancy no longer lives in the Cell at all), so the
      // identity default is set here, at the fresh-build site only.
      cell.warp_basis = IDENTITY_BASIS_2D;

      // Default: every freshly built cell is PRESENT (occupied). This is the invariant that keeps
      // existing worlds byte-identical until the user carves a cell out: InitUnitCells is the sole
      // fresh-build site, so a new world fills the whole rectangle present. PRESENT is a *set* bit,
      // so a zero-initialised cell would otherwise read as absent — hence it is set explicitly here.
      cell.flags |= CELL_FLAG_PRESENT;

      // Default tile type 0: the cell's fill is sourced from the grid's own colour_fill, so a freshly
      // built world is pixel-identical to before this field existed. Non-zero types are authored later
      // (no authoring path exists yet) and resolve through the shared tile palette. Set explicitly here,
      // at the sole fresh-build site, mirroring the self-documenting style used for warp_basis/PRESENT.
      cell.tile_type = 0;

      // Write the cell to the array
      Cell *address = (Cell *)((char *)cells->items + (k * cells->elem_bytes));
      MemoryCopy(address, &cell, cells->elem_bytes);
      count++;
   }
   cells->count = columns * rows;
   LOG_INFO("Initialised %d cells\n", count);
}

GridSpace2d NewGridSpace2d_FromPreset(Vector2d origin_in_parent, CoordinateSpacePreset preset, ColourRgba colour_fill, ColourRgba colour_line)
{
   return NewGridSpace2d(origin_in_parent, preset.resolution, preset.basis, colour_fill, colour_line);
}

// Copy the AUTHORED state of one cell: flags (incl. PRESENT/SOLID/behaviour bits), tile_type, warp_basis,
// effect and value. This is the single list of authored fields used by world growth. Deliberately excludes
// per-cell entity occupancy (transient; lives in World2d.cell_occupancy) (derived origins are computed on demand by Space_CellLocalOrigin).
// A field added to Cell later must be added here or growth will silently drop it.
void Cell_CopyAuthored(Cell *dst, const Cell *src)
{
   dst->flags = src->flags;
   dst->tile_type = src->tile_type;
   dst->warp_basis = src->warp_basis;
   dst->effect = src->effect;
   dst->value = src->value;
}

// Local-space origin of a cell: grid_origin + column*u + row*v. Derived on demand because Cell no longer stores
// it (its only consumer is a debug log). Same formula the old per-cell origin pass used. Returns the grid origin
// for a null space or an out-of-range index.
Vector2d Space_CellLocalOrigin(const Space2d *space, int cell_index)
{
   if (!space || space->columns < 1 || cell_index < 0 || cell_index >= (int)space->cells.count)
   {
      return space ? space->grid_origin : ZERO_VECTOR_2D;
   }
   int i = cell_index / space->columns; // Row
   int j = cell_index % space->columns; // Column
   return (Vector2d){space->grid_origin.x + j * space->frame.basis.u.x + i * space->frame.basis.v.x,
                     space->grid_origin.y + j * space->frame.basis.u.y + i * space->frame.basis.v.y};
}

// Rebuild basis-derived data after a basis edit: refresh unitArea only (cells no longer store a derived origin, so this is O(1)).
// DELIBERATE DEVIATION (needs user sign-off): this is done IN PLACE rather than via a snapshot + InitUnitCells +
// Cell_CopyAuthored restore. A basis edit never changes columns/rows, so there is nothing to copy; leaving the
// buffer alone preserves flags (SOLID etc.), warp_basis, effect and value (fixing the old bug where only PRESENT
// and tile_type survived). Occupancy is no longer zeroed here; Universe_SetWorldBasis calls RefreshWorldSpatialMap
// straight afterwards, which resets and rebuilds it. The root-world init caller works unchanged because
// NewSpace2d already built every cell PRESENT.
void RebuildSpaceCells(Space2d *space)
{
   if (!space)
   {
      return;
   }

   space->unitArea = fabsf((space->frame.basis.u.x * space->frame.basis.v.y) -
                           (space->frame.basis.u.y * space->frame.basis.v.x));
}

// Lattice cell containing a space-local point (uses the same maths as GetIndexFromCoords). False when the
// point is non-finite or absurdly large.
bool Space_LatticeFromLocal(const Space2d *space, Vector2d local, int *out_lx, int *out_ly)
{
   if (!space || !out_lx || !out_ly)
   {
      return false;
   }
   return WorldGrowth_LatticeFromLocal(space->grid_origin.x, space->grid_origin.y, local.x, local.y, out_lx, out_ly);
}

// Lattice cell -> dense index, or -1 if outside the grid.
int Space_IndexFromLattice(const Space2d *space, int lx, int ly)
{
   if (!space)
   {
      return -1;
   }
   return WorldGrowth_IndexFromLattice(space->columns, space->rows, (int)space->grid_origin.x, (int)space->grid_origin.y, lx, ly);
}

// Cell at a lattice coordinate, or NULL if outside the grid / no buffer. Pointer is valid until the next growth.
Cell *Space_GetCellAtLattice(Space2d *space, int lx, int ly)
{
   if (!space || !space->cells.items)
   {
      return NULL;
   }
   int index = Space_IndexFromLattice(space, lx, ly);
   if (index < 0 || index >= space->cells.count)
   {
      return NULL;
   }
   return &((Cell *)space->cells.items)[index];
}

// Apply a resize plan (growth OR trim) to a space. ORDER (so the commit cannot half-resize the space):
//   1. validate the plan against the live space;
//   2. TryAllocateBytes the fresh buffer (NULL -> log, return false, NOTHING modified);
//   3. WorldGrowth_CopyCells into the fresh buffer (cannot fail; drops old cells outside a shrunk grid);
//   4. COMMIT: swap cells.items, set count/capacity, columns, rows, grid_origin, frame.local_min/max;
//   5. free the old buffer (the caller then rebuilds the per-world occupancy store via RefreshWorldSpatialMap).
// Steps 4-6 contain no failing operation. Pointers into the old cells buffer are invalid afterwards.
bool Space_ResizeCells(Space2d *space, const struct GridGrowthPlan *plan)
{
   if (!space || !plan || !space->cells.items)
   {
      return false;
   }
   // 1. Validate: the plan must describe THIS space, be self-consistent and respect the cap. Growth and
   // shrink both pass; Space_GrowCells adds the grow-only checks.
   if (plan->old_columns != space->columns || plan->old_rows != space->rows ||
       plan->old_origin_x != (int)space->grid_origin.x || plan->old_origin_y != (int)space->grid_origin.y ||
       plan->new_columns < 1 || plan->new_rows < 1 ||
       plan->new_columns > MAX_WORLD_GRID_DIMENSION || plan->new_rows > MAX_WORLD_GRID_DIMENSION ||
       plan->shift_cols != plan->old_origin_x - plan->new_origin_x ||
       plan->shift_rows != plan->old_origin_y - plan->new_origin_y ||
       space->cells.count != space->columns * space->rows)
   {
      LOG_ERROR("Space_ResizeCells: plan does not match the space; world left untouched.\n");
      return false;
   }

   // 2. Fresh zeroed buffer; size in size_t (max 512*512 cells).
   int new_count = plan->new_columns * plan->new_rows;
   size_t new_bytes = (size_t)new_count * sizeof(Cell);
   Cell *fresh = (Cell *)TryAllocateBytes(new_bytes);
   if (!fresh)
   {
      LOG_ERROR("Space_ResizeCells: out of memory allocating %zu bytes; world left untouched.\n", new_bytes);
      return false;
   }

   // 3. Copy authored data into the shifted slots.
   WorldGrowth_CopyCells(fresh, plan, (const Cell *)space->cells.items);

   // 4. Commit. Nothing below can fail.
   void *old_items = space->cells.items;
   size_t old_bytes = (size_t)space->cells.capacity * space->cells.elem_bytes;
   space->cells.items = fresh;
   space->cells.count = new_count;
   space->cells.capacity = new_count;
   space->columns = plan->new_columns;
   space->rows = plan->new_rows;
   space->grid_origin = (Vector2d){(float)plan->new_origin_x, (float)plan->new_origin_y};
   space->frame.local_min = space->grid_origin;
   space->frame.local_max = (Vector2d){space->grid_origin.x + (float)plan->new_columns,
                                       space->grid_origin.y + (float)plan->new_rows};

   // 5. Release the old buffer.
   Deallocate(&old_items, old_bytes);
   return true;
}

// Growth entry point: a thin wrapper over Space_ResizeCells that additionally insists the plan only grows (every
// old cell keeps a slot), so a trim plan can never be applied through the growth API by mistake.
bool Space_GrowCells(Space2d *space, const struct GridGrowthPlan *plan)
{
   if (!space || !plan)
   {
      return false;
   }
   if (plan->new_columns < plan->old_columns || plan->new_rows < plan->old_rows ||
       plan->shift_cols < 0 || plan->shift_rows < 0 ||
       plan->shift_cols + plan->old_columns > plan->new_columns ||
       plan->shift_rows + plan->old_rows > plan->new_rows)
   {
      LOG_ERROR("Space_GrowCells: plan does not only grow; world left untouched.\n");
      return false;
   }
   return Space_ResizeCells(space, plan);
}

// Return whether the cell at cell_index is PRESENT (really there). NULL space / NULL buffer /
// out-of-range index all read as absent.
bool Cell_IsPresent(const Space2d *space, int cell_index)
{
   if (!space || !space->cells.items)
   {
      return false;
   }
   if (cell_index < 0 || cell_index >= (int)space->cells.count)
   {
      return false;
   }
   const Cell *cells = (const Cell *)space->cells.items;
   return (cells[cell_index].flags & CELL_FLAG_PRESENT) != 0;
}

// Mark the cell at cell_index present/absent by toggling CELL_FLAG_PRESENT in place. An out-of-range
// index is a no-op returning false. Returns whether the stored presence bit actually changed. Never
// reallocates, resizes, or changes cells.count.
bool Space_SetCellPresent(Space2d *space, int cell_index, bool present)
{
   if (!space || !space->cells.items)
   {
      return false;
   }
   if (cell_index < 0 || cell_index >= (int)space->cells.count)
   {
      return false;
   }
   Cell *cells = (Cell *)space->cells.items;
   bool was_present = (cells[cell_index].flags & CELL_FLAG_PRESENT) != 0;
   if (was_present == present)
   {
      return false; // No change.
   }
   if (present)
   {
      cells[cell_index].flags |= CELL_FLAG_PRESENT;
   }
   else
   {
      cells[cell_index].flags &= ~CELL_FLAG_PRESENT;
   }
   return true;
}

int GetIndexFromCoords(Space2d *space, Vector2d local_coords)
{
   int i = (int)floorf(local_coords.y - space->grid_origin.y);
   int j = (int)floorf(local_coords.x - space->grid_origin.x);
   int columns = space->columns;
   int rows = space->rows;

   if (i < 0 || j < 0 || i >= rows || j >= columns)
   {
      return -1;
   }

   int cell_index = (i * columns) + j;
   return cell_index;
}

// Return whether a space-local point lies inside a PRESENT cell. Uses GetIndexFromCoords (the
// single index convention) so out-of-grid points give -1 and read as false; otherwise the
// presence bit of the resolved cell decides.
bool Space_IsPointInPresentCell(const Space2d *space, Vector2d point)
{
   if (!space)
   {
      return false;
   }
   // GetIndexFromCoords takes a non-const space but only reads it.
   int cell_index = GetIndexFromCoords((Space2d *)space, point);
   if (cell_index < 0)
   {
      return false; // Out of grid.
   }
   return Cell_IsPresent(space, cell_index);
}

Cell *GetCellFromCoords(Space2d *space, Vector2d local_coords)
{
   int cell_index = GetIndexFromCoords(space, local_coords);
   if (cell_index < 0 || cell_index >= space->cells.count)
   {
      return NULL;
   }
   Cell *cells = space->cells.items;
   Cell *target_cell = &cells[cell_index];

   return target_cell;
}


// Matrix2x2 CalcSpaceAABB(Space2d *space)
// {
//    Matrix2x2 u_v_extents = CalcSpaceExtents_2d(space);
//    Matrix2x2 aabb_box = {0};
//    aabb_box.col1.x = fminf(u_v_extents.col1.x, u_v_extents.col2.x);
//    aabb_box.col2.x = fmaxf(u_v_extents.col1.x, u_v_extents.col2.x);

//    aabb_box.col1.y = fminf(u_v_extents.col1.y, u_v_extents.col2.y);
//    aabb_box.col2.y = fmaxf(u_v_extents.col1.y, u_v_extents.col2.y);

//    return aabb_box;
// }

// Compute the 4 corners of the integer cell rectangle (in absolute world-local
// units) covering an object's world-local AABB. This emits the covered-cell span
// in the SAME units GetIndexFromCoords consumes: one raw local unit = one cell.
// There is deliberately no basis (|u|/|v|) scaling here — the grid is a unit-cell
// lattice in local coordinates, and GetIndexFromCoords is the single place that
// floors a local coordinate to a cell and subtracts grid_origin once when the
// span loop walks these corners.
// The object offset parameter allows you to specify the world coordinates of the
// object's centre, which is necessary to correctly position the footprint in the
// coordinate space. Provide a zero vector if the object's surface vertices are
// already in world coordinates.
void CalcSnappedAABB_Vertices(Vector2d *object_surface_vertices, int object_surface_vertices_count, Vector2d object_offset, Vector2d out_vertices[4])
{
   if (object_surface_vertices_count == 0)
   {
      return;
   }

   Matrix2x2 box_coords = {0};

   // Must initialise with one of the provided vertices rather than all 0s because 0 could be the largest or smallest value compared to the provided vertices
   box_coords.col1 = VectorSum_2d(object_offset, object_surface_vertices[0]);
   box_coords.col2 = VectorSum_2d(object_offset, object_surface_vertices[0]);
   Vector2d vertice = {0};
   for (size_t i = 1; i < object_surface_vertices_count; i++)
   {
      vertice = VectorSum_2d(object_offset, object_surface_vertices[i]); // Convert vertices to absolute world positions to find the global AABB limits

      box_coords.col1.x = fminf(box_coords.col1.x, vertice.x);
      box_coords.col2.x = fmaxf(box_coords.col2.x, vertice.x);

      box_coords.col1.y = fminf(box_coords.col1.y, vertice.y);
      box_coords.col2.y = fmaxf(box_coords.col2.y, vertice.y);
   }

   // Convert the world-local AABB into an inclusive cell span in absolute local
   // units (one raw local unit = one cell), exactly matching GetIndexFromCoords.
   // floor selects the cell containing the minimum edge; ceil selects the first
   // boundary after the maximum edge. Rounding outward ensures partially covered
   // cells are retained as broad-phase candidates instead of being missed at an
   // edge. grid_origin is intentionally NOT subtracted here: GetIndexFromCoords
   // applies it once when the span loop walks these corners.
   float start_cell_x = floorf(box_coords.col1.x);
   float end_cell_x = ceilf(box_coords.col2.x);
   float start_cell_y = floorf(box_coords.col1.y);
   float end_cell_y = ceilf(box_coords.col2.y);

   // Create a 4-corner bounding rectangle that perfectly encapsulates
   // every single cell the object is partially or fully occupying.
   //LArray footprint_vertices = MakeLArray(4, sizeof(Vector2d));

   // Map the bounding indexes back to absolute spatial coordinates
   out_vertices[0] = (Vector2d){start_cell_x, start_cell_y}; // Bottom-Left Cell Boundary
   out_vertices[1] = (Vector2d){end_cell_x, start_cell_y};   // Bottom-Right Cell Boundary
   out_vertices[2] = (Vector2d){end_cell_x, end_cell_y};     // Top-Right Cell Boundary
   out_vertices[3] = (Vector2d){start_cell_x, end_cell_y};   // Top-Left Cell Boundary
}

// bool VectorIsInSpace_2d(Vector2d vector, Space2d *space)
// {
//    // Check if the vector is within the bounds of the coordinate space defined by its origin and the extents of its basis vectors multiplied by their respective steps
//    Matrix2x2 extents = CalcSpaceExtents_2d(space);
//    Vector2d origin = space->system.origin_in_parent;
//    float min_x = origin.x;
//    float max_x = origin.x + extents.col1.x + extents.col2.x;
//    float min_y = origin.y;
//    float max_y = origin.y + extents.col1.y + extents.col2.y;

//    return (vector.x >= min_x && vector.x < max_x && vector.y >= min_y && vector.y < max_y);
// }

// This function calculates the spatial extents of a coordinate space based on its origin and the extents of its basis vectors multiplied by their respective steps.
// It returns a 2x2 matrix where col1 is the vector from the origin to the far corner along the u direction, and col2 is the vector from the origin to the far corner along the v direction.
//    int cols = coordinate_space->columns;

//    // 1. Correct the counts
//    int numHorizontalLines = rows + 1;
//    int numVerticalLines = cols + 1;

//    coordinate_space->lineSegments_u = *NewDynamicArray(numHorizontalLines, sizeof(LineSegment2d));
//    coordinate_space->lineSegments_v = *NewDynamicArray(numVerticalLines, sizeof(LineSegment2d));

//    // 2. Generate Horizontal-ish Lines (spanning the width)
//    for (int r = 0; r < numHorizontalLines; r++)
//    {
//       LineSegment2d segment;
//       // Start at origin, step DOWN 'r' times using Basis V
//       segment.start.x = space_coords.x + r * coordinate_space->basis.v.x;
//       segment.start.y = space_coords.y + r * coordinate_space->basis.v.y;

//       // Extend across the whole width (cols) using Basis U
//       segment.end.x = segment.start.x + cols * coordinate_space->basis.u.x;
//       segment.end.y = segment.start.y + cols * coordinate_space->basis.u.y;

//       Array_Push(&coordinate_space->lineSegments_u, &segment);
//    }

//    // 3. Generate Vertical-ish Lines (spanning the height)
//    for (int c = 0; c < numVerticalLines; c++)
//    {
//       LineSegment2d segment;
//       // Start at origin, step RIGHT 'c' times using Basis U
//       segment.start.x = space_coords.x + c * coordinate_space->basis.u.x;
//       segment.start.y = space_coords.y + c * coordinate_space->basis.u.y;

//       // Extend down the whole height (rows) using Basis V
//       segment.end.x = segment.start.x + rows * coordinate_space->basis.v.x;
//       segment.end.y = segment.start.y + rows * coordinate_space->basis.v.y;

//       Array_Push(&coordinate_space->lineSegments_v, &segment);
//    }
//}

// Update the values of all cells in the field according to object interactions with them
// void UpdateCellValue(Cell *cell)
// {
//    // Positional
// }

// Update the values of all cells in the field according to object interactions with them
// void FindCellsWithCollisions(Field *field)
// {
//    // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
//    // If there are more cells than objects, iterate through the objects and check for collisions with the cell they are in
// }

// NOT DONE
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

// Update the values of all cells in the field according to object interactions with them
// Field UpdateCellValues(Field field)
// {
//    Collection *cells = &field.coordinateSpace.cells.coll;
//    Collection *objects = &field.items.coll;

//    // Goal = Adjust cell value if the cell's surface has some overlap with the surface that an object's contact vectors create

//    // If there are more cells than objects, iterate through the objects and increment the value for the all cells the object occupies
//    for (size_t i = 0; i < objects->count; i++)
//    {
//       Circloid *circloid_i = (Circloid *)((char *)objects + (i * objects->elemSize));
//       Collection *vertices = &circloid_i->object.surface.surface_vectors.coll;

//       DynamicArray *vector_indices = NewDynamicArray(vertices->count, vertices->elemSize);
//       // Get the surface vectors of the object and find the corresponding cell indices
//       for (size_t i = 0; i < vertices->count; i++)
//       {
//          // Calculate cell indices
//          Vector2d indices = GetCellIndicesFromCoordinates(field.shape.object.position, ((Vector2d*)vertices->items)[i], field.coordinateSpace.basis);
//          Array_Push(vector_indices, &indices);
//       }

//    }
//    // if (cells->count >= objects->count)
//    // {
//    // }

//    // If there are more objects than cells, iterate through the cells and check for collisions if the cell has occupancy > 1
//    // field.

//    return field;
// }

