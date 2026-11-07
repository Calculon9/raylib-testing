/**********************************************************************************************
 *
 * WORLD GROWTH (pure helpers)
 *
 * A nested world's grid is a dense row-major Cell[]. Painting a cell just outside it grows that
 * array. These helpers hold the pure grid logic (no engine state, no allocation) so it can be
 * unit-tested in isolation. Include this AFTER math/coordinate_space.h (it needs Cell,
 * CELL_FLAG_PRESENT, Vector2d, IDENTITY_BASIS_2D and Cell_CopyAuthored).
 *
 * LATTICE COORDINATES: cell (lx, ly) spans local space [lx, lx+1] x [ly, ly+1]. They are ABSOLUTE:
 * growing the grid on the left/top moves Space2d.grid_origin (by whole integers) instead of moving
 * the cells, so a lattice coordinate names the same physical cell before and after any growth.
 * Only the dense index changes. Anything stored across frames (commands, selection) therefore keeps
 * lattice coordinates and resolves to an index at the point of use.
 *
 **********************************************************************************************/
#ifndef WORLD_GROWTH_H
#define WORLD_GROWTH_H
#include <stdbool.h>
#include <math.h>

// R4: columns and rows are each capped at this after growth.
#define MAX_WORLD_GRID_DIMENSION 512

// R3 overlap tolerance: universe units of penetration depth along each separating axis. Touching, or
// overlapping by no more than this, is NOT an overlap (so abutting worlds are allowed).
#define WORLD_GROWTH_OVERLAP_EPS 0.001f

// Largest |lattice| value accepted anywhere (guards float->int casts and int overflow).
#define WORLD_GROWTH_MAX_LATTICE 1000000

// What growth does to one grid. Pure data; all lattice coordinates are absolute.
typedef struct GridGrowthPlan
{
    int old_columns, old_rows, old_origin_x, old_origin_y;
    int new_columns, new_rows, new_origin_x, new_origin_y;
    int shift_cols, shift_rows; // Old cell (0,0) lands at new (shift_cols, shift_rows) = old_origin - new_origin.
} GridGrowthPlan;

typedef enum GrowthResult
{
    GROWTH_NOT_NEEDED,
    GROWTH_OK,
    GROWTH_REFUSED_CAP,
    GROWTH_INVALID_RECT // PlanToRect only: the target rectangle is empty or inverted.
} GrowthResult;

// Lattice cell containing a local point. Mirrors GetIndexFromCoords: column = floorf(x - origin) is
// computed first and the integer origin added back, so the answer always agrees with the index
// function. Returns false for NaN/inf or |value| > WORLD_GROWTH_MAX_LATTICE (guards the float->int cast).
static inline bool WorldGrowth_LatticeFromLocal(float origin_x, float origin_y, float x, float y, int *out_lx, int *out_ly)
{
    if (!isfinite(x) || !isfinite(y) || !isfinite(origin_x) || !isfinite(origin_y))
        return false;
    if (fabsf(x) > (float)WORLD_GROWTH_MAX_LATTICE || fabsf(y) > (float)WORLD_GROWTH_MAX_LATTICE ||
        fabsf(origin_x) > (float)WORLD_GROWTH_MAX_LATTICE || fabsf(origin_y) > (float)WORLD_GROWTH_MAX_LATTICE)
        return false;
    *out_lx = (int)floorf(x - origin_x) + (int)origin_x;
    *out_ly = (int)floorf(y - origin_y) + (int)origin_y;
    return true;
}

// Lattice cell -> dense row-major index, or -1 if outside [origin, origin + size). 64-bit arithmetic
// so extreme lattice values cannot overflow int.
static inline int WorldGrowth_IndexFromLattice(int columns, int rows, int origin_x, int origin_y, int lx, int ly)
{
    long long col = (long long)lx - (long long)origin_x;
    long long row = (long long)ly - (long long)origin_y;
    if (col < 0 || row < 0 || col >= columns || row >= rows)
        return -1;
    return (int)(row * columns + col);
}

// True when any of the 4 edge neighbours (not diagonals) of lattice cell (lx,ly) is inside the grid
// and PRESENT. This is rule R2.
static inline bool WorldGrowth_TouchesPresentCell(const Cell *cells, int columns, int rows,
                                                  int origin_x, int origin_y, int lx, int ly)
{
    if (!cells)
        return false;
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};
    for (int n = 0; n < 4; n++)
    {
        int idx = WorldGrowth_IndexFromLattice(columns, rows, origin_x, origin_y, lx + dx[n], ly + dy[n]);
        if (idx >= 0 && (cells[idx].flags & CELL_FLAG_PRESENT))
            return true;
    }
    return false;
}

// Plan a resize of the grid to the absolute lattice rectangle [min_x,max_x) x [min_y,max_y). This is the single
// planning routine: growth passes a rectangle that contains the old grid, a trim passes a smaller one. For a
// shrink shift_cols/shift_rows are NEGATIVE (old cell (0,0) lies left of / above the new grid and is dropped).
// GROWTH_NOT_NEEDED when the rectangle equals the current grid (plan = identity), GROWTH_REFUSED_CAP when
// either new dimension would exceed max_dim, GROWTH_INVALID_RECT when the rectangle is empty or inverted.
// Uses long long internally.
static inline GrowthResult WorldGrowth_PlanToRect(int columns, int rows, int origin_x, int origin_y,
                                                  long long min_x, long long min_y, long long max_x, long long max_y,
                                                  int max_dim, GridGrowthPlan *out_plan)
{
    GridGrowthPlan p;
    p.old_columns = columns;
    p.old_rows = rows;
    p.old_origin_x = origin_x;
    p.old_origin_y = origin_y;

    long long new_cols = max_x - min_x;
    long long new_rows = max_y - min_y;
    if (new_cols <= 0 || new_rows <= 0)
    {
        if (out_plan)
            *out_plan = p; // Old fields only; caller must not apply an invalid plan.
        return GROWTH_INVALID_RECT;
    }

    bool unchanged = (new_cols == columns && new_rows == rows && min_x == origin_x && min_y == origin_y);
    if (!unchanged && (new_cols > max_dim || new_rows > max_dim))
    {
        if (out_plan)
            *out_plan = p; // Identity-ish data; caller must not apply a refused plan.
        return GROWTH_REFUSED_CAP;
    }

    p.new_columns = (int)new_cols;
    p.new_rows = (int)new_rows;
    p.new_origin_x = (int)min_x;
    p.new_origin_y = (int)min_y;
    p.shift_cols = origin_x - p.new_origin_x;
    p.shift_rows = origin_y - p.new_origin_y;
    if (out_plan)
        *out_plan = p;
    return unchanged ? GROWTH_NOT_NEEDED : GROWTH_OK;
}

// Plan growth so lattice cell (lx,ly) is covered. GROWTH_NOT_NEEDED when already inside (plan = identity).
// GROWTH_REFUSED_CAP when either new dimension would exceed max_dim. The target rectangle is the union of the
// old grid and the 1x1 target cell, handed to WorldGrowth_PlanToRect.
static inline GrowthResult WorldGrowth_Plan(int columns, int rows, int origin_x, int origin_y,
                                            int lx, int ly, int max_dim, GridGrowthPlan *out_plan)
{
    long long min_x = origin_x < lx ? origin_x : lx;
    long long min_y = origin_y < ly ? origin_y : ly;
    long long old_max_x = (long long)origin_x + columns;
    long long old_max_y = (long long)origin_y + rows;
    long long max_x = old_max_x > (long long)lx + 1 ? old_max_x : (long long)lx + 1;
    long long max_y = old_max_y > (long long)ly + 1 ? old_max_y : (long long)ly + 1;
    return WorldGrowth_PlanToRect(columns, rows, origin_x, origin_y, min_x, min_y, max_x, max_y, max_dim, out_plan);
}

// True when the grid cell (lx,ly) is PRESENT, treating the lattice cell (skip_lx,skip_ly) as already absent.
// Used so a delete can be evaluated BEFORE the presence bit is cleared.
static inline bool WorldGrowth_PresentExcluding(const Cell *cells, int columns, int origin_x, int origin_y,
                                                int lx, int ly, int skip_lx, int skip_ly, int row, int col)
{
    if (lx == skip_lx && ly == skip_ly)
        return false;
    return (cells[(long long)row * columns + col].flags & CELL_FLAG_PRESENT) != 0;
}

// Cheap trim detection (B2): true only when the deleted lattice cell lies on the outer row or column of the
// grid. An interior deletion can never move a tight bounding box, so callers skip the scan entirely.
static inline bool WorldGrowth_DeleteMayShrink(int columns, int rows, int origin_x, int origin_y, int lx, int ly)
{
    long long col = (long long)lx - origin_x;
    long long row = (long long)ly - origin_y;
    if (col < 0 || row < 0 || col >= columns || row >= rows)
        return false;
    return col == 0 || col == columns - 1 || row == 0 || row == rows - 1;
}

// Tight bounding rectangle [*x0,*x1) x [*y0,*y1) (absolute lattice) of the PRESENT cells, as it will be once
// lattice cell (del_lx,del_ly) has been deleted. The grid is assumed already tight (the invariant this feature
// maintains), so only the edges the deleted cell lies on can move: for each such edge the scan walks inward
// while the whole row/column is absent. Rows are trimmed first (full width), then columns (over the surviving
// rows), which gives the unique tight box. Returns false when no present cell would remain (the caller must
// refuse the delete). Cost: O(length of the lines scanned), never O(cells) for a delete that cannot shrink.
static inline bool WorldGrowth_TightPresentRect(const Cell *cells, int columns, int rows, int origin_x, int origin_y,
                                                int del_lx, int del_ly, int *x0, int *y0, int *x1, int *y1)
{
    int c0 = 0, c1 = columns, r0 = 0, r1 = rows; // Surviving window in dense column/row units, [c0,c1) x [r0,r1).
    int del_col = del_lx - origin_x;
    int del_row = del_ly - origin_y;

    if (cells && WorldGrowth_DeleteMayShrink(columns, rows, origin_x, origin_y, del_lx, del_ly))
    {
        bool top = del_row == 0, bottom = del_row == rows - 1;
        bool left = del_col == 0, right = del_col == columns - 1;

        // Trim whole absent rows from the top then the bottom.
        for (; top && r0 < r1; r0++)
        {
            bool any = false;
            for (int c = 0; c < columns && !any; c++)
                any = WorldGrowth_PresentExcluding(cells, columns, origin_x, origin_y, c + origin_x, r0 + origin_y,
                                                   del_lx, del_ly, r0, c);
            if (any)
                break;
        }
        for (; bottom && r1 > r0; r1--)
        {
            bool any = false;
            for (int c = 0; c < columns && !any; c++)
                any = WorldGrowth_PresentExcluding(cells, columns, origin_x, origin_y, c + origin_x, r1 - 1 + origin_y,
                                                   del_lx, del_ly, r1 - 1, c);
            if (any)
                break;
        }
        if (r0 >= r1)
            return false; // Every row was absent: no present cell remains.

        // Trim whole absent columns from the left then the right, looking only at the surviving rows.
        for (; left && c0 < c1; c0++)
        {
            bool any = false;
            for (int r = r0; r < r1 && !any; r++)
                any = WorldGrowth_PresentExcluding(cells, columns, origin_x, origin_y, c0 + origin_x, r + origin_y,
                                                   del_lx, del_ly, r, c0);
            if (any)
                break;
        }
        for (; right && c1 > c0; c1--)
        {
            bool any = false;
            for (int r = r0; r < r1 && !any; r++)
                any = WorldGrowth_PresentExcluding(cells, columns, origin_x, origin_y, c1 - 1 + origin_x, r + origin_y,
                                                   del_lx, del_ly, r, c1 - 1);
            if (any)
                break;
        }
        if (c0 >= c1)
            return false;
    }

    *x0 = origin_x + c0;
    *x1 = origin_x + c1;
    *y0 = origin_y + r0;
    *y1 = origin_y + r1;
    return true;
}

// Grow the rectangle [*x0,*x1) x [*y0,*y1) so it also contains lattice cell (lx,ly). Used by a trim to keep the
// lattice cell holding an entity's centre inside the new grid (B4).
static inline void WorldGrowth_RectInclude(int *x0, int *y0, int *x1, int *y1, int lx, int ly)
{
    if (lx < *x0) *x0 = lx;
    if (ly < *y0) *y0 = ly;
    if (lx + 1 > *x1) *x1 = lx + 1;
    if (ly + 1 > *y1) *y1 = ly + 1;
}

// Old dense index -> new dense index after the plan (row/column re-based by shift_*). For a shrink the
// result is only meaningful when the old cell survives (see WorldGrowth_CopyCells).
static inline int WorldGrowth_MapIndex(const GridGrowthPlan *plan, int old_index)
{
    int r = old_index / plan->old_columns;
    int c = old_index % plan->old_columns;
    return (r + plan->shift_rows) * plan->new_columns + (c + plan->shift_cols);
}

// Added strip as a lattice rectangle [x0,x1) x [y0,y1). Single-axis growth (always, by R2) gives exactly
// the new column/row; if both axes grew it falls back to the whole new rectangle (conservative).
// Nothing grew -> an empty rectangle (x0 == x1).
static inline void WorldGrowth_AddedStrip(const GridGrowthPlan *plan, int *x0, int *y0, int *x1, int *y1)
{
    bool cols_grew = plan->new_columns != plan->old_columns;
    bool rows_grew = plan->new_rows != plan->old_rows;
    int nx1 = plan->new_origin_x + plan->new_columns;
    int ny1 = plan->new_origin_y + plan->new_rows;
    int ox1 = plan->old_origin_x + plan->old_columns;
    int oy1 = plan->old_origin_y + plan->old_rows;

    if (!cols_grew && !rows_grew)
    {
        *x0 = *x1 = plan->new_origin_x;
        *y0 = *y1 = plan->new_origin_y;
    }
    else if (cols_grew && !rows_grew)
    {
        // Left growth adds [new_origin, old_origin); right growth adds [old_end, new_end).
        *y0 = plan->new_origin_y;
        *y1 = ny1;
        if (plan->new_origin_x < plan->old_origin_x)
        {
            *x0 = plan->new_origin_x;
            *x1 = plan->old_origin_x;
        }
        else
        {
            *x0 = ox1;
            *x1 = nx1;
        }
    }
    else if (rows_grew && !cols_grew)
    {
        *x0 = plan->new_origin_x;
        *x1 = nx1;
        if (plan->new_origin_y < plan->old_origin_y)
        {
            *y0 = plan->new_origin_y;
            *y1 = plan->old_origin_y;
        }
        else
        {
            *y0 = oy1;
            *y1 = ny1;
        }
    }
    else
    {
        *x0 = plan->new_origin_x;
        *y0 = plan->new_origin_y;
        *x1 = nx1;
        *y1 = ny1;
    }
}

// Fills dst (new_columns * new_rows cells, already zeroed) from src: every dst cell gets identity
// warp_basis and ABSENT flags (a zero basis would be invalid); each old cell that falls inside the new grid
// is then copied to its shifted slot via Cell_CopyAuthored, the single list of authored fields. Cell no longer stores
// a derived origin, and per-cell occupancy lives in World2d.cell_occupancy (rebuilt by RefreshWorldSpatialMap).
static inline void WorldGrowth_CopyCells(Cell *dst, const GridGrowthPlan *plan, const Cell *src)
{
    long long total = (long long)plan->new_columns * plan->new_rows;
    for (long long k = 0; k < total; k++)
    {
        dst[k].flags = CELL_FLAG_NONE;
        dst[k].warp_basis = IDENTITY_BASIS_2D;
    }
    for (int r = 0; r < plan->old_rows; r++)
    {
        // A shrink has negative shifts, so some old rows/columns land outside the new grid and are dropped.
        // For growth every old cell lands inside, so these guards never skip anything.
        int nr = r + plan->shift_rows;
        if (nr < 0 || nr >= plan->new_rows)
            continue;
        for (int c = 0; c < plan->old_columns; c++)
        {
            int nc = c + plan->shift_cols;
            if (nc < 0 || nc >= plan->new_columns)
                continue;
            Cell_CopyAuthored(&dst[nr * plan->new_columns + nc], &src[r * plan->old_columns + c]);
        }
    }
}

// Strict convex-quad overlap by separating axes over all 8 edge normals (unit length). The quads
// overlap ONLY if the projection overlap depth on EVERY axis exceeds eps; touching or shallower
// overlap is NOT overlap. eps is in universe units of penetration depth (WORLD_GROWTH_OVERLAP_EPS).
static inline bool WorldGrowth_QuadsOverlapStrict(const Vector2d a[4], const Vector2d b[4], float eps)
{
    for (int quad = 0; quad < 2; quad++)
    {
        const Vector2d *q = quad == 0 ? a : b;
        for (int e = 0; e < 4; e++)
        {
            // Edge normal, normalised so depth is a true distance.
            float ex = q[(e + 1) % 4].x - q[e].x;
            float ey = q[(e + 1) % 4].y - q[e].y;
            float len = sqrtf(ex * ex + ey * ey);
            if (len < 1e-12f)
                continue; // Degenerate edge has no axis.
            float nx = -ey / len;
            float ny = ex / len;

            float min_a = INFINITY, max_a = -INFINITY, min_b = INFINITY, max_b = -INFINITY;
            for (int i = 0; i < 4; i++)
            {
                float pa = a[i].x * nx + a[i].y * ny;
                float pb = b[i].x * nx + b[i].y * ny;
                if (pa < min_a) min_a = pa;
                if (pa > max_a) max_a = pa;
                if (pb < min_b) min_b = pb;
                if (pb > max_b) max_b = pb;
            }
            float hi = max_a < max_b ? max_a : max_b;
            float lo = min_a > min_b ? min_a : min_b;
            if (hi - lo <= eps)
                return false; // Separated (or merely touching) on this axis.
        }
    }
    return true;
}

#endif // WORLD_GROWTH_H
