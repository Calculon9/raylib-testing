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
    GROWTH_REFUSED_CAP
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

// Plan growth so lattice cell (lx,ly) is covered. GROWTH_NOT_NEEDED when already inside (plan = identity).
// GROWTH_REFUSED_CAP when either new dimension would exceed max_dim. Uses long long internally.
static inline GrowthResult WorldGrowth_Plan(int columns, int rows, int origin_x, int origin_y,
                                            int lx, int ly, int max_dim, GridGrowthPlan *out_plan)
{
    GridGrowthPlan p;
    p.old_columns = columns;
    p.old_rows = rows;
    p.old_origin_x = origin_x;
    p.old_origin_y = origin_y;

    // New extent = union of the old rectangle and the 1x1 target cell, in absolute lattice space.
    long long min_x = origin_x < lx ? origin_x : lx;
    long long min_y = origin_y < ly ? origin_y : ly;
    long long old_max_x = (long long)origin_x + columns;
    long long old_max_y = (long long)origin_y + rows;
    long long max_x = old_max_x > (long long)lx + 1 ? old_max_x : (long long)lx + 1;
    long long max_y = old_max_y > (long long)ly + 1 ? old_max_y : (long long)ly + 1;
    long long new_cols = max_x - min_x;
    long long new_rows = max_y - min_y;

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

// Old dense index -> new dense index after the plan (row/column re-based by shift_*).
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
// warp_basis and ABSENT flags (a zero basis would be invalid); each old cell is then copied to its
// shifted slot via Cell_CopyAuthored, the single list of authored fields. local_origin is NOT set here;
// the caller derives it (Space_RecomputeCellOrigins) once the new origin and dimensions are live.
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
        for (int c = 0; c < plan->old_columns; c++)
        {
            Cell *to = &dst[(r + plan->shift_rows) * plan->new_columns + (c + plan->shift_cols)];
            Cell_CopyAuthored(to, &src[r * plan->old_columns + c]);
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
