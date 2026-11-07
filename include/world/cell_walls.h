/**********************************************************************************************
 *
 * ABSENT-CELL WALLS (pure helpers)
 *
 * A world's shape is the set of PRESENT cells. A cell whose PRESENT flag is clear has been
 * given back to the root world, so it behaves as an implicit SOLID tile. These helpers hold
 * the pure grid logic (no engine state) so it can be unit-tested in isolation. Include this
 * AFTER math/coordinate_space.h (it needs Cell, CELL_FLAG_PRESENT and CELL_FLAG_SOLID).
 *
 **********************************************************************************************/
#ifndef CELL_WALLS_H
#define CELL_WALLS_H

#include <stdbool.h>

// True when the cell at (col, row) blocks movement: outside the grid, absent (PRESENT clear),
// or present and flagged SOLID. `cells` is a dense row-major array of columns * rows cells.
// Flags are only meaningful for a present cell, so SOLID is read after PRESENT passes.
static inline bool CellWalls_IsBlocked(const Cell *cells, int columns, int rows, int col, int row)
{
    // Outside the grid there is no cell, so the container acts as the wall.
    if (col < 0 || row < 0 || col >= columns || row >= rows)
        return true;

    int flags = (int)cells[row * columns + col].flags;
    if (!(flags & CELL_FLAG_PRESENT))
        return true; // Absent cells are walls.
    return (flags & CELL_FLAG_SOLID) != 0;
}

// Find the nearest cell that is present and not SOLID to (from_col, from_row), using an
// expanding square ring search bounded by the grid size. Within the first ring that holds a
// candidate, the smallest squared distance wins; ties go to the first candidate in row-major
// order (top row first, then left to right), so the result is deterministic. Returns false
// when the grid has no usable cell (or has no layout).
static inline bool CellWalls_FindNearestOpen(const Cell *cells, int columns, int rows,
                                             int from_col, int from_row,
                                             int *out_col, int *out_row)
{
    if (!cells || columns < 1 || rows < 1)
        return false;

    // Ring `r` is the square perimeter at Chebyshev distance r; r never needs to exceed the
    // furthest grid edge from the start cell.
    int max_ring = columns > rows ? columns : rows;
    for (int ring = 0; ring <= max_ring; ring++)
    {
        bool found = false;
        long best_dist = 0;
        int best_col = 0, best_row = 0;

        for (int row = from_row - ring; row <= from_row + ring; row++)
        {
            if (row < 0 || row >= rows)
                continue;

            // Top and bottom rows of the ring are full spans; the middle rows only have the
            // two end columns.
            bool full_row = (row == from_row - ring || row == from_row + ring);
            int step = full_row ? 1 : (ring > 0 ? 2 * ring : 1);
            for (int col = from_col - ring; col <= from_col + ring; col += step)
            {
                if (col < 0 || col >= columns)
                    continue;
                if (CellWalls_IsBlocked(cells, columns, rows, col, row))
                    continue;

                long dx = col - from_col;
                long dy = row - from_row;
                long dist = dx * dx + dy * dy;
                // Strict '<' keeps the first (row-major) candidate on a tie.
                if (!found || dist < best_dist)
                {
                    found = true;
                    best_dist = dist;
                    best_col = col;
                    best_row = row;
                }
            }
        }

        if (found)
        {
            *out_col = best_col;
            *out_row = best_row;
            return true;
        }
    }
    return false;
}

#endif // CELL_WALLS_H
