/**********************************************************************************************
 *
 * CELL OCCUPANCY (pure sparse store)
 *
 * Per-cell entity lists used to live inside every Cell (24 ids + a count), which made the dense
 * grid 148 bytes per cell and forced a full-grid reset every tick. This store keeps lists ONLY for
 * cells that currently hold at least one entity, so reset and iteration scale with the number of
 * occupied cells rather than the grid area.
 *
 * DESIGN
 *  - A "bump pool" of fixed-size blocks {cell, count, slot, ids[24]}. A block is created the first
 *    time a cell is touched in a frame and is never freed individually: Remove only lowers `count`,
 *    and Reset simply rewinds `block_count` to 0 (the whole pool is reused next frame, so the
 *    steady state allocates nothing). Block creation order is the iteration order.
 *  - An open-addressing hash table (linear probing, power-of-two capacity, load factor <= 0.5)
 *    maps dense cell index -> block index. Each entry carries its key so a probe never touches the
 *    108-byte blocks until the key matches. `block_plus_one == 0` marks an empty entry, which is
 *    why cell index 0 is a perfectly legal key. Entries are never deleted individually, so no
 *    tombstones are needed; Reset clears exactly the entries in use via block.slot (no probing).
 *
 * PRECONDITION: TryAllocateBytes returns ZEROED memory (calloc semantics). The table relies on
 * that to start with every entry empty. Use only TryAllocateBytes here (never AllocateBytes,
 * which exit()s): allocation failure is non-fatal and reported through
 * CellOccupancy_TakeAllocFailureReport. The store logs nothing itself; the caller does.
 *
 * SNAPSHOT RULE: any pointer returned by Get/BlockAt is invalidated by Add, Reserve, PrepareAdd
 * and Reset (pool growth reallocates). Never hold one across such a call; copy the ids you need.
 *
 * THREADING: not thread safe. The job system currently runs jobs serially on one thread.
 *
 * Pure: depends only on EntityId and the allocator, so it can be unit-tested with stand-ins.
 *
 **********************************************************************************************/
#ifndef CELL_OCCUPANCY_H
#define CELL_OCCUPANCY_H

#include <stdbool.h>
#include <stddef.h>
#include "entities/entity_id.h"
#include "memory/cmemory.h"

// Maximum entities one cell can list. Further occupants are refused (CELL_OCC_FULL).
#define MAX_CELL_OCCUPANCY 24
// Initial pool size in blocks (6.9 KB pool + 2 KB table); doubles on demand.
#define CELL_OCCUPANCY_INITIAL_BLOCKS 64

// One occupied cell: its dense index plus the entity list. Referenced by block index, never by
// retained pointer, because pool growth moves the blocks.
typedef struct OccupancyBlock
{
    int cell;                         // Dense cell index this block describes.
    int count;                        // Live entries in ids (0 = emptied this frame; skipped by iteration).
    int slot;                         // Hash-table slot holding this block, so Reset clears it without probing.
    EntityId ids[MAX_CELL_OCCUPANCY]; // Insertion order; removal compacts stably.
} OccupancyBlock;

// One hash-table entry. The key is stored here so probing avoids touching the blocks.
typedef struct OccupancySlot
{
    int cell;           // Key (dense cell index).
    int block_plus_one; // 0 = empty entry, otherwise (block index + 1).
} OccupancySlot;

typedef struct CellOccupancy
{
    OccupancyBlock *blocks;
    int block_count;           // Blocks in use this frame.
    int block_capacity;
    OccupancySlot *table;
    int table_capacity;        // Power of two, >= 2 * block_capacity.
    int cell_limit;            // Dense cell count at the last Reset; keys must be below it.
    bool alloc_failure_reported; // Throttle: an allocation failure is flagged once per tick.
    bool alloc_failure_pending;  // Set by a failed allocation, consumed by TakeAllocFailureReport.
} CellOccupancy;

typedef enum OccupancyAddResult
{
    CELL_OCC_OK = 0,
    CELL_OCC_FULL,         // Cell already holds MAX_CELL_OCCUPANCY entries.
    CELL_OCC_OUT_OF_RANGE, // cell < 0 or >= cell_limit.
    CELL_OCC_ALLOC_FAILED  // Pool/table growth failed; the store is unchanged.
} OccupancyAddResult;

// Read-only view of one cell's list. ids is valid only until the next Add/Reserve/PrepareAdd/Reset.
typedef struct OccupancyList
{
    const EntityId *ids;
    int count;
} OccupancyList;

// Multiplicative (Fibonacci) hash folded with a shift so the low bits used by the power-of-two
// mask are well mixed even for sequential cell indices.
static inline unsigned CellOccupancy_Hash(int cell)
{
    unsigned h = (unsigned)cell * 2654435761u;
    return h ^ (h >> 16);
}

// Find the block index holding `cell`, or -1. Probing stops at the first empty entry; the load
// factor is <= 0.5 so one always exists and the loop terminates.
static inline int CellOccupancy_Find(const CellOccupancy *s, int cell)
{
    if (s->table_capacity == 0)
        return -1;
    unsigned mask = (unsigned)s->table_capacity - 1u;
    for (unsigned i = CellOccupancy_Hash(cell) & mask;; i = (i + 1u) & mask)
    {
        const OccupancySlot *e = &s->table[i];
        if (e->block_plus_one == 0)
            return -1;
        if (e->cell == cell)
            return e->block_plus_one - 1;
    }
}

// Release the pool and table (byte counts come from the capacities) and zero the struct.
static inline void CellOccupancy_Free(CellOccupancy *s)
{
    if (s->blocks)
        Deallocate((void **)&s->blocks, (size_t)s->block_capacity * sizeof(OccupancyBlock));
    if (s->table)
        Deallocate((void **)&s->table, (size_t)s->table_capacity * sizeof(OccupancySlot));
    *s = (CellOccupancy){0};
}

// Record an allocation failure; flags it for the caller at most once per tick (until Reset).
static inline void CellOccupancy_NoteAllocFailure(CellOccupancy *s)
{
    if (!s->alloc_failure_reported)
    {
        s->alloc_failure_reported = true;
        s->alloc_failure_pending = true;
    }
}

// Returns true once per tick if an allocation failed since the last report. The caller logs it.
static inline bool CellOccupancy_TakeAllocFailureReport(CellOccupancy *s)
{
    bool pending = s->alloc_failure_pending;
    s->alloc_failure_pending = false;
    return pending;
}

// Ensure room for at least min_blocks blocks. Grows pool and table ATOMICALLY: both new buffers
// are allocated first; if either fails nothing is committed and the old store stays intact.
// Existing blocks are copied and re-hashed (rewriting every block.slot). Never shrinks.
static inline bool CellOccupancy_Reserve(CellOccupancy *s, int min_blocks)
{
    if (min_blocks <= s->block_capacity)
        return true;

    // Double from the current capacity (or the initial size) until the request fits.
    long long new_blocks = s->block_capacity > 0 ? s->block_capacity : CELL_OCCUPANCY_INITIAL_BLOCKS;
    while (new_blocks < min_blocks)
        new_blocks *= 2;
    long long new_table = 2;
    while (new_table < new_blocks * 2)
        new_table *= 2;
    if (new_table > 0x40000000LL)
    {
        CellOccupancy_NoteAllocFailure(s);
        return false;
    }

    OccupancyBlock *blocks = (OccupancyBlock *)TryAllocateBytes((size_t)new_blocks * sizeof(OccupancyBlock));
    OccupancySlot *table = (OccupancySlot *)TryAllocateBytes((size_t)new_table * sizeof(OccupancySlot));
    if (!blocks || !table)
    {
        // Free whichever succeeded; the existing store is untouched.
        if (blocks)
            Deallocate((void **)&blocks, (size_t)new_blocks * sizeof(OccupancyBlock));
        if (table)
            Deallocate((void **)&table, (size_t)new_table * sizeof(OccupancySlot));
        CellOccupancy_NoteAllocFailure(s);
        return false;
    }

    // Copy the live blocks (including emptied ones: they still own a table entry) and re-hash.
    unsigned mask = (unsigned)new_table - 1u;
    for (int b = 0; b < s->block_count; b++)
    {
        blocks[b] = s->blocks[b];
        unsigned i = CellOccupancy_Hash(blocks[b].cell) & mask;
        while (table[i].block_plus_one != 0)
            i = (i + 1u) & mask;
        table[i].cell = blocks[b].cell;
        table[i].block_plus_one = b + 1;
        blocks[b].slot = (int)i;
    }

    // Swap in the new buffers, then release the old ones using their OLD capacities.
    OccupancyBlock *old_blocks = s->blocks;
    OccupancySlot *old_table = s->table;
    size_t old_block_bytes = (size_t)s->block_capacity * sizeof(OccupancyBlock);
    size_t old_table_bytes = (size_t)s->table_capacity * sizeof(OccupancySlot);
    s->blocks = blocks;
    s->table = table;
    s->block_capacity = (int)new_blocks;
    s->table_capacity = (int)new_table;
    if (old_blocks)
        Deallocate((void **)&old_blocks, old_block_bytes);
    if (old_table)
        Deallocate((void **)&old_table, old_table_bytes);
    return true;
}

// O(blocks in use): clears exactly the table entries used (via block.slot), rewinds the pool and
// stamps the valid cell range. Safe on an empty store whose Reserve failed (NULL buffers): it
// still stamps cell_limit and never dereferences NULL.
static inline void CellOccupancy_Reset(CellOccupancy *s, int cell_limit)
{
    if (s->table && s->blocks)
    {
        for (int b = 0; b < s->block_count; b++)
            s->table[s->blocks[b].slot] = (OccupancySlot){0};
    }
    s->block_count = 0;
    s->cell_limit = cell_limit;
    s->alloc_failure_reported = false;
    s->alloc_failure_pending = false;
}

// What Add(cell) would return, without adding. On OK, space for a new block has already been
// reserved, so an immediately following Add for the same cell cannot fail. Spawn uses this to
// reject cleanly before registering anything.
static inline OccupancyAddResult CellOccupancy_PrepareAdd(CellOccupancy *s, int cell)
{
    if (cell < 0 || cell >= s->cell_limit)
        return CELL_OCC_OUT_OF_RANGE;
    int b = CellOccupancy_Find(s, cell);
    if (b >= 0)
        return s->blocks[b].count >= MAX_CELL_OCCUPANCY ? CELL_OCC_FULL : CELL_OCC_OK;
    if (s->block_count >= s->block_capacity && !CellOccupancy_Reserve(s, s->block_count + 1))
        return CELL_OCC_ALLOC_FAILED;
    return CELL_OCC_OK;
}

// Append id to the cell's list (creating the cell's block on first touch this frame).
static inline OccupancyAddResult CellOccupancy_Add(CellOccupancy *s, int cell, EntityId id)
{
    OccupancyAddResult r = CellOccupancy_PrepareAdd(s, cell);
    if (r != CELL_OCC_OK)
        return r;

    int b = CellOccupancy_Find(s, cell);
    if (b < 0)
    {
        // First touch: claim the first empty entry on the probe path and bump-allocate a block.
        unsigned mask = (unsigned)s->table_capacity - 1u;
        unsigned i = CellOccupancy_Hash(cell) & mask;
        while (s->table[i].block_plus_one != 0)
            i = (i + 1u) & mask;
        b = s->block_count++;
        s->table[i].cell = cell;
        s->table[i].block_plus_one = b + 1;
        s->blocks[b].cell = cell;
        s->blocks[b].count = 0;
        s->blocks[b].slot = (int)i;
    }
    s->blocks[b].ids[s->blocks[b].count++] = id;
    return CELL_OCC_OK;
}

// Remove EVERY occurrence of id from the cell's list with stable (ordered) compaction. Returns
// the number removed (0 if the cell has no block or the id is absent). An emptied block stays in
// the pool with count 0 and is skipped by iteration.
static inline int CellOccupancy_Remove(CellOccupancy *s, int cell, EntityId id)
{
    int b = CellOccupancy_Find(s, cell);
    if (b < 0)
        return 0;
    OccupancyBlock *blk = &s->blocks[b];
    int write = 0;
    for (int read = 0; read < blk->count; read++)
    {
        if (blk->ids[read] != id)
            blk->ids[write++] = blk->ids[read];
    }
    int removed = blk->count - write;
    blk->count = write;
    return removed;
}

// Read-only view of a cell's list; {NULL, 0} when the cell has no block. See the snapshot rule.
static inline OccupancyList CellOccupancy_Get(const CellOccupancy *s, int cell)
{
    int b = CellOccupancy_Find(s, cell);
    if (b < 0)
        return (OccupancyList){NULL, 0};
    return (OccupancyList){s->blocks[b].ids, s->blocks[b].count};
}

// Number of entities listed in a cell (0 if none).
static inline int CellOccupancy_Count(const CellOccupancy *s, int cell)
{
    int b = CellOccupancy_Find(s, cell);
    return b < 0 ? 0 : s->blocks[b].count;
}

// Blocks in use this frame, for indexed iteration (some may have count 0).
static inline int CellOccupancy_BlockCount(const CellOccupancy *s)
{
    return s->block_count;
}

// Block at an iteration index in [0, BlockCount). Invalidated by Add/Reserve/PrepareAdd/Reset.
static inline const OccupancyBlock *CellOccupancy_BlockAt(const CellOccupancy *s, int block_index)
{
    return &s->blocks[block_index];
}

#endif // CELL_OCCUPANCY_H
