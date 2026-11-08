/**********************************************************************************************
 *
 * CONTACT PERSISTENCE SET (pure)
 *
 * Remembers WHICH PAIRS TOUCHED on recent frames, including sleeper-sleeper pairs that detection skips,
 * so the wake rules (world/sleep_wake.h) can answer two questions without any geometry:
 *   - "is this contact NEW?"      -> Touch() reports whether a record already existed.
 *   - "has this contact GONE?"    -> Sweep() delivers records that stopped being refreshed.
 *
 * CONCEPTS (explained once, here)
 *  - Touch / Carry: an emitted contact TOUCHES its record (insert or refresh). A pair whose bodies both
 *    sleep is not run through SAT, so the broad phase CARRIES its record instead (refresh only, never
 *    insert). Sleepers cannot move, so a pair that touched before still touches; the carry only confirms
 *    both bodies are still enumerated, so a deleted or teleported sleeper lets its records expire.
 *  - Grace: a record counts as gone only after `grace` seconds without a touch or carry (physics time, so
 *    it is frame-rate independent). Resting overlap flickers in and out for a frame or two; without the
 *    grace each flicker would read as "gone" then "new".
 *  - NoteGone: callers that remove or move a body say so by id; the next Sweep delivers every untouched
 *    record naming that id immediately instead of waiting out the grace.
 *  - Canonical pair: the two ids are stored lower raw EntityId first (the stored normal is flipped to
 *    match), so callers may pass the ids in either order. Full EntityIds (with generation) are compared,
 *    so a recycled slot is a different pair.
 *  - No self-referential pointers: Universe moves World2d by value, so the inline gone-id array is only
 *    ever addressed through ContactPersist_GoneList(), which recomputes the address from the struct.
 *
 * THREADING: single threaded. MEMORY: TryAllocateBytes only (never AllocateBytes, which exit()s); every
 * failure is non-fatal and reported through alloc_failed. PRECONDITION: TryAllocateBytes returns ZEROED
 * memory (an all-zero record is an empty slot).
 *
 **********************************************************************************************/
#ifndef CONTACT_PERSIST_H
#define CONTACT_PERSIST_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "entities/entity_id.h"
#include "memory/cmemory.h"

#define CONTACT_PERSIST_MIN_CAPACITY 1024         // Table slots (power of two); load is kept at or below 0.5.
#define CONTACT_PERSIST_MAX_RECORDS (1 << 18)     // Hard ceiling on live records; further pairs are not tracked.
#define CONTACT_PERSIST_GONE_INLINE 32            // Gone ids held inside the struct before a heap list is needed.
#define CONTACT_PERSIST_DEFAULT_GRACE 0.06f       // Seconds a pair may go unseen before it counts as gone.

#define PERSIST_FLAG_USED 1u                      // Slot holds a live record (0 = empty, TOMB = deleted).
#define PERSIST_FLAG_TOMB 2u                      // Deleted slot: probes continue past it, inserts may reuse it.
#define PERSIST_FLAG_PENDING_GONE 4u              // Expired but not yet delivered (output buffer was too small).
#define PERSIST_FLAG_LAST_CARRIED 8u              // The last refresh was a Carry, not a Touch.
#define PERSIST_FLAG_EXPIRING 16u                 // Sweep scratch bit (never set outside Sweep).
#define PERSIST_FLAG_NOTED 32u                    // Sweep scratch bit: expiring because NoteGone named an end.

#define GONE_NOTED 1u                             // GonePair.flags: delivered because NoteGone named an end.
#define GONE_LAST_CARRIED 2u                      // GonePair.flags: the record's last refresh was a Carry.

typedef struct PersistRecord // 24 B
{
    EntityId id_a, id_b;     // Canonical order: lower raw EntityId first.
    uint32_t last_stamp;     // Frame stamp when last touched or carried.
    uint32_t carry_stamp;    // Frame stamp of the last carry ("carried this frame" == carry_stamp == stamp).
    float unseen_time;       // Seconds since last_stamp (physics time).
    int8_t dir_x, dir_y;     // Contact normal A->B, quantised (n * 127).
    uint8_t flags;           // PERSIST_FLAG_*
    uint8_t pad;
} PersistRecord;

typedef struct GonePair // One expired or noted-gone pair, delivered to the support-lost rule.
{
    EntityId id_a, id_b;     // Canonical order (lower raw EntityId first).
    int8_t dir_x, dir_y;     // Last contact normal A->B (quantised n * 127).
    uint8_t flags;           // GONE_*
} GonePair;

typedef struct AgeingPair // An untouched record that has not yet expired (0 < unseen_time < grace).
{
    EntityId id_a, id_b;     // Canonical order.
    int8_t dir_y;            // Stored normal y part (A->B, quantised), for the support-side test.
} AgeingPair;

typedef struct ContactPersist
{
    PersistRecord *records;
    int capacity, count, tombstones, log2_capacity;
    uint32_t stamp;                                 // Frame counter (advanced by BeginFrame).
    float dt, grace;                                // Physics step of this frame; grace seconds.
    EntityId gone_inline[CONTACT_PERSIST_GONE_INLINE];
    EntityId *gone_heap;                            // NULL = the inline array is in use; else holds ALL entries.
    int gone_count, gone_capacity;                  // gone_capacity is only meaningful while gone_heap != NULL.
    GonePair *out;                                  // Sweep output buffer (heap only).
    int out_capacity, pending;                      // pending = expired records not yet delivered.
    AgeingPair *ageing;                             // Records ageing in the last Sweep (sleep-onset hold); rebuilt every Sweep.
    int ageing_count, ageing_capacity;
    size_t bytes;                                   // Total heap bytes owned (for reporting).
    bool alloc_failed, disabled_this_frame;         // disabled_this_frame: inserts are refused until next frame.
} ContactPersist;

//----------------------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------------------

static inline bool ContactPersist_RecordLive(const PersistRecord *r)
{
    return (r->flags & PERSIST_FLAG_USED) != 0u;
}

static inline bool ContactPersist_IsCarriedThisFrame(const ContactPersist *p, const PersistRecord *r)
{
    return r->carry_stamp == p->stamp && (r->flags & PERSIST_FLAG_LAST_CARRIED) != 0u;
}

// Quantise a unit-normal component to int8 with saturation.
static inline int8_t ContactPersist_Quantise(float v)
{
    float q = v * 127.0f;
    if (!(q == q))
        return 0;
    return (int8_t)(q > 127.0f ? 127 : (q < -127.0f ? -127 : (int)(q >= 0.0f ? q + 0.5f : q - 0.5f)));
}

// Put the ids in canonical order, flipping the normal when they are swapped.
static inline void ContactPersist_Canon(EntityId *a, EntityId *b, float *nx, float *ny)
{
    if ((uint32_t)*a > (uint32_t)*b)
    {
        EntityId t = *a;
        *a = *b;
        *b = t;
        if (nx)
            *nx = -*nx;
        if (ny)
            *ny = -*ny;
    }
}

// Address of the live gone-id list: the heap list once it exists, else the inline array.
static inline EntityId *ContactPersist_GoneList(ContactPersist *p)
{
    return p->gone_heap ? p->gone_heap : p->gone_inline;
}

// Fibonacci hash of the two FULL ids. The slot-index key used elsewhere keeps its larger index in the low
// bits, which would cluster records in the first buckets; mixing both ids and taking the HIGH bits does not.
static inline uint32_t ContactPersist_Hash(EntityId a, EntityId b, int log2_capacity)
{
    uint64_t key = ((uint64_t)(uint32_t)a << 32) | (uint64_t)(uint32_t)b;
    key ^= key >> 31;
    return (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> (64 - log2_capacity));
}

// Index of the record for a CANONICAL pair, or -1. Probing stops at the first empty slot.
static inline int ContactPersist_Find(const ContactPersist *p, EntityId a, EntityId b)
{
    if (!p->records || p->capacity <= 0)
        return -1;
    uint32_t mask = (uint32_t)p->capacity - 1u;
    uint32_t h = ContactPersist_Hash(a, b, p->log2_capacity);
    for (int probe = 0; probe < p->capacity; probe++)
    {
        const PersistRecord *r = &p->records[h];
        if (r->flags == 0u)
            return -1;
        if ((r->flags & PERSIST_FLAG_USED) && r->id_a == a && r->id_b == b)
            return (int)h;
        h = (h + 1u) & mask;
    }
    return -1;
}

// Longest probe sequence needed to find any live record (a quality metric for the hash; used by tests).
static inline int ContactPersist_MaxProbe(const ContactPersist *p)
{
    int worst = 0;
    if (!p->records)
        return 0;
    uint32_t mask = (uint32_t)p->capacity - 1u;
    for (int i = 0; i < p->capacity; i++)
    {
        const PersistRecord *r = &p->records[i];
        if (!(r->flags & PERSIST_FLAG_USED))
            continue;
        uint32_t h = ContactPersist_Hash(r->id_a, r->id_b, p->log2_capacity);
        int probes = 1;
        while ((int)h != i)
        {
            h = (h + 1u) & mask;
            probes++;
        }
        if (probes > worst)
            worst = probes;
    }
    return worst;
}

// Place a record (known absent) in the first empty or deleted slot of its probe sequence.
static inline void ContactPersist_PlaceRaw(PersistRecord *table, int capacity, int log2_capacity, const PersistRecord *rec,
                                           int *tombstones)
{
    uint32_t mask = (uint32_t)capacity - 1u;
    uint32_t h = ContactPersist_Hash(rec->id_a, rec->id_b, log2_capacity);
    while (table[h].flags & PERSIST_FLAG_USED)
        h = (h + 1u) & mask;
    if (tombstones && (table[h].flags & PERSIST_FLAG_TOMB))
        (*tombstones)--;
    table[h] = *rec;
}

// Rebuild the table at `new_capacity` (power of two), dropping tombstones. On failure nothing changes.
static inline bool ContactPersist_Rebuild(ContactPersist *p, int new_capacity)
{
    size_t bytes = (size_t)new_capacity * sizeof(PersistRecord);
    PersistRecord *table = (PersistRecord *)TryAllocateBytes(bytes);
    if (!table)
    {
        p->alloc_failed = true;
        return false;
    }
    int log2 = 0;
    while ((1 << log2) < new_capacity)
        log2++;
    for (int i = 0; i < p->capacity && p->records; i++)
        if (p->records[i].flags & PERSIST_FLAG_USED)
            ContactPersist_PlaceRaw(table, new_capacity, log2, &p->records[i], NULL);
    if (p->records)
    {
        size_t old_bytes = (size_t)p->capacity * sizeof(PersistRecord);
        Deallocate((void **)&p->records, old_bytes);
        p->bytes -= old_bytes;
    }
    p->records = table;
    p->capacity = new_capacity;
    p->log2_capacity = log2;
    p->tombstones = 0;
    p->bytes += bytes;
    return true;
}

//----------------------------------------------------------------------------------
// Lifecycle
//----------------------------------------------------------------------------------

// Make room for at least `min_records` live records at load <= 0.5. False (nothing changed) on failure.
static inline bool ContactPersist_Reserve(ContactPersist *p, int min_records)
{
    if (min_records > CONTACT_PERSIST_MAX_RECORDS)
        min_records = CONTACT_PERSIST_MAX_RECORDS;
    int want = CONTACT_PERSIST_MIN_CAPACITY;
    while (want < min_records * 2 && want < (1 << 30))
        want <<= 1;
    if (p->records && want <= p->capacity)
        return true;
    return ContactPersist_Rebuild(p, want);
}

// Start a frame: advance the stamp and store the step. Does NOT touch the gone list; only Sweep consumes it,
// so a NoteGone made at any point since the previous sweep is honoured by the next one.
static inline void ContactPersist_BeginFrame(ContactPersist *p, float dt)
{
    p->stamp++;
    p->dt = dt;
    if (!(p->grace > 0.0f))
        p->grace = CONTACT_PERSIST_DEFAULT_GRACE;
    p->disabled_this_frame = false;
}

// Release every buffer and clear the set. The struct may be copied by value before this (see header note).
static inline void ContactPersist_Free(ContactPersist *p)
{
    if (p->records)
        Deallocate((void **)&p->records, (size_t)p->capacity * sizeof(PersistRecord));
    if (p->gone_heap)
        Deallocate((void **)&p->gone_heap, (size_t)p->gone_capacity * sizeof(EntityId));
    if (p->out)
        Deallocate((void **)&p->out, (size_t)p->out_capacity * sizeof(GonePair));
    if (p->ageing)
        Deallocate((void **)&p->ageing, (size_t)p->ageing_capacity * sizeof(AgeingPair));
    memset(p, 0, sizeof(*p));
}

// True once if an allocation failed since the last call, so the caller logs one line per episode.
static inline bool ContactPersist_TakeAllocFailureReport(ContactPersist *p)
{
    bool failed = p->alloc_failed;
    p->alloc_failed = false;
    return failed;
}

//----------------------------------------------------------------------------------
// Touch / Carry / NoteGone
//----------------------------------------------------------------------------------

// An emitted contact refreshes (or inserts) its record. Returns true when the record already existed, so a
// FALSE return means "this contact is NEW". Returns true (never NEW) when the pair cannot be tracked: invalid
// ids, allocation failure or a full table, so a failure can never cause a spurious wake.
static inline bool ContactPersist_Touch(ContactPersist *p, EntityId a, EntityId b, float nx, float ny)
{
    if (a == INVALID_ENTITY_ID || b == INVALID_ENTITY_ID || a == b)
        return true;
    ContactPersist_Canon(&a, &b, &nx, &ny);
    int idx = ContactPersist_Find(p, a, b);
    if (idx >= 0)
    {
        PersistRecord *r = &p->records[idx];
        r->last_stamp = p->stamp;
        r->unseen_time = 0.0f;
        r->dir_x = ContactPersist_Quantise(nx);
        r->dir_y = ContactPersist_Quantise(ny);
        r->flags &= (uint8_t)~(PERSIST_FLAG_PENDING_GONE | PERSIST_FLAG_LAST_CARRIED);
        return true;
    }
    if (p->disabled_this_frame)
        return true;
    if (!p->records && !ContactPersist_Reserve(p, 0))
    {
        p->disabled_this_frame = true;
        return true;
    }
    if (p->count + 1 > p->capacity / 2)
    {
        // Grow while under the ceiling; at the ceiling (or on allocation failure) stop inserting this frame.
        // Tombstones are reclaimed by a same-size rebuild first.
        bool ok = false;
        if (p->count + 1 <= CONTACT_PERSIST_MAX_RECORDS)
            ok = ContactPersist_Rebuild(p, p->capacity * 2);
        if (!ok)
        {
            p->disabled_this_frame = true;
            return true;
        }
    }
    PersistRecord rec = {0};
    rec.id_a = a;
    rec.id_b = b;
    rec.last_stamp = p->stamp;
    rec.dir_x = ContactPersist_Quantise(nx);
    rec.dir_y = ContactPersist_Quantise(ny);
    rec.flags = PERSIST_FLAG_USED;
    ContactPersist_PlaceRaw(p->records, p->capacity, p->log2_capacity, &rec, &p->tombstones);
    p->count++;
    return false;
}

// The broad phase confirms both bodies of an existing record are still enumerated while both sleep.
// Refreshes only (never inserts). Idempotent within a frame (a pair spans up to four cells).
static inline bool ContactPersist_Carry(ContactPersist *p, EntityId a, EntityId b)
{
    if (a == INVALID_ENTITY_ID || b == INVALID_ENTITY_ID || a == b)
        return false;
    ContactPersist_Canon(&a, &b, NULL, NULL);
    int idx = ContactPersist_Find(p, a, b);
    if (idx < 0)
        return false;
    PersistRecord *r = &p->records[idx];
    if (r->carry_stamp == p->stamp && (r->flags & PERSIST_FLAG_LAST_CARRIED))
        return true;
    r->last_stamp = p->stamp;
    r->carry_stamp = p->stamp;
    r->unseen_time = 0.0f;
    r->flags = (uint8_t)((r->flags | PERSIST_FLAG_LAST_CARRIED) & ~PERSIST_FLAG_PENDING_GONE);
    return true;
}

// Say that a body was removed or moved: the next Sweep delivers every untouched record that names it. If the
// list cannot grow the id is dropped (the grace period delivers the same wake a few frames later).
static inline void ContactPersist_NoteGone(ContactPersist *p, EntityId id)
{
    if (id == INVALID_ENTITY_ID)
        return;
    EntityId *list = ContactPersist_GoneList(p);
    if (p->gone_count > 0 && list[p->gone_count - 1] == id)
        return;
    int capacity = p->gone_heap ? p->gone_capacity : CONTACT_PERSIST_GONE_INLINE;
    if (p->gone_count >= capacity)
    {
        int new_capacity = capacity * 2;
        EntityId *grown = (EntityId *)TryAllocateBytes((size_t)new_capacity * sizeof(EntityId));
        if (!grown)
        {
            p->alloc_failed = true;
            return;
        }
        memcpy(grown, list, (size_t)p->gone_count * sizeof(EntityId));
        if (p->gone_heap)
            Deallocate((void **)&p->gone_heap, (size_t)p->gone_capacity * sizeof(EntityId));
        p->gone_heap = grown;
        p->gone_capacity = new_capacity;
        list = grown;
    }
    list[p->gone_count++] = id;
}

//----------------------------------------------------------------------------------
// Sweep
//----------------------------------------------------------------------------------

static inline int ContactPersist_CompareIds(const void *x, const void *y)
{
    uint32_t a = (uint32_t)*(const EntityId *)x, b = (uint32_t)*(const EntityId *)y;
    return (a > b) - (a < b);
}

static inline bool ContactPersist_IdListed(const EntityId *sorted, int n, EntityId id)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (sorted[mid] == id)
            return true;
        if ((uint32_t)sorted[mid] < (uint32_t)id)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return false;
}

// Move one expiring record into the output buffer and delete it from the table.
static inline void ContactPersist_Deliver(ContactPersist *p, PersistRecord *r, GonePair *out)
{
    out->id_a = r->id_a;
    out->id_b = r->id_b;
    out->dir_x = r->dir_x;
    out->dir_y = r->dir_y;
    out->flags = (uint8_t)(((r->flags & PERSIST_FLAG_NOTED) ? GONE_NOTED : 0u) |
                           ((r->flags & PERSIST_FLAG_LAST_CARRIED) ? GONE_LAST_CARRIED : 0u));
    r->flags = PERSIST_FLAG_TOMB;
    p->count--;
    p->tombstones++;
}

// Remember one ageing record for the sleep-onset hold. False if the list could not grow (non-fatal).
static inline bool ContactPersist_PushAgeing(ContactPersist *p, const PersistRecord *r)
{
    if (p->ageing_count >= p->ageing_capacity)
    {
        int new_capacity = p->ageing_capacity > 0 ? p->ageing_capacity * 2 : 64;
        AgeingPair *grown = (AgeingPair *)TryAllocateBytes((size_t)new_capacity * sizeof(AgeingPair));
        if (!grown)
        {
            p->alloc_failed = true;
            return false;
        }
        if (p->ageing)
        {
            memcpy(grown, p->ageing, (size_t)p->ageing_count * sizeof(AgeingPair));
            Deallocate((void **)&p->ageing, (size_t)p->ageing_capacity * sizeof(AgeingPair));
            p->bytes -= (size_t)p->ageing_capacity * sizeof(AgeingPair);
        }
        p->ageing = grown;
        p->ageing_capacity = new_capacity;
        p->bytes += (size_t)new_capacity * sizeof(AgeingPair);
    }
    p->ageing[p->ageing_count++] = (AgeingPair){r->id_a, r->id_b, r->dir_y};
    return true;
}

// Put a delivered (deleted) pair back as a fresh record, as if it had just been touched. Used when a grace
// expiry is not a real loss. False if it could not be stored (full table or allocation failure), in which
// case the caller must treat the pair as gone.
static inline bool ContactPersist_Rearm(ContactPersist *p, const GonePair *g)
{
    ContactPersist_Touch(p, g->id_a, g->id_b, (float)g->dir_x / 127.0f, (float)g->dir_y / 127.0f);
    return ContactPersist_Find(p, g->id_a, g->id_b) >= 0;
}

// Advance unseen time for untouched records, then deliver every record that expired (unseen for `grace`) or
// that NoteGone named while untouched. Returns the pointer to p->out and writes the count. A record is deleted
// only after its GonePair is written; records that do not fit (output growth failed) stay PENDING and are
// delivered FIRST next frame, so nothing is ever dropped. The gone-id list is consumed here.
static inline const GonePair *ContactPersist_Sweep(ContactPersist *p, int *out_count)
{
    *out_count = 0;
    p->ageing_count = 0;
    if (!p->records)
    {
        p->gone_count = 0;
        return NULL;
    }
    EntityId *gone = ContactPersist_GoneList(p);
    int gone_n = p->gone_count;
    if (gone_n > 1)
        qsort(gone, (size_t)gone_n, sizeof(EntityId), ContactPersist_CompareIds);

    // Pass 1: age untouched records and mark the expiring ones. Records still inside their grace are also
    // listed in p->ageing (non-fatal: if the list cannot grow it is emptied and left empty for this frame, so
    // the sleep-onset hold is skipped rather than applied partially).
    int expiring = 0;
    bool ageing_ok = true;
    for (int i = 0; i < p->capacity; i++)
    {
        PersistRecord *r = &p->records[i];
        if (!(r->flags & PERSIST_FLAG_USED))
            continue;
        r->flags &= (uint8_t)~(PERSIST_FLAG_EXPIRING | PERSIST_FLAG_NOTED);
        bool untouched = r->last_stamp != p->stamp;
        if (untouched && p->dt > 0.0f)
            r->unseen_time += p->dt;
        bool noted = untouched && gone_n > 0 &&
                     (ContactPersist_IdListed(gone, gone_n, r->id_a) || ContactPersist_IdListed(gone, gone_n, r->id_b));
        bool expired = untouched && r->unseen_time >= p->grace;
        if ((r->flags & PERSIST_FLAG_PENDING_GONE) || noted || expired)
        {
            r->flags |= PERSIST_FLAG_EXPIRING;
            if (noted)
                r->flags |= PERSIST_FLAG_NOTED;
            expiring++;
        }
        else if (untouched && r->unseen_time > 0.0f && ageing_ok)
            ageing_ok = ContactPersist_PushAgeing(p, r);
    }
    if (!ageing_ok)
        p->ageing_count = 0;
    p->gone_count = 0;

    // Make the output buffer big enough for every expiring record; on failure deliver what fits.
    if (expiring > p->out_capacity)
    {
        int want = p->out_capacity > 0 ? p->out_capacity : 64;
        while (want < expiring)
            want *= 2;
        GonePair *buffer = (GonePair *)TryAllocateBytes((size_t)want * sizeof(GonePair));
        if (buffer)
        {
            if (p->out)
                Deallocate((void **)&p->out, (size_t)p->out_capacity * sizeof(GonePair));
            p->bytes += (size_t)want * sizeof(GonePair);
            p->bytes -= (size_t)p->out_capacity * sizeof(GonePair);
            p->out = buffer;
            p->out_capacity = want;
        }
        else
            p->alloc_failed = true;
    }

    // Pass 2: deliver previously pending records first, then the new ones, until the buffer is full.
    int delivered = 0;
    for (int stage = 0; stage < 2; stage++)
        for (int i = 0; i < p->capacity; i++)
        {
            PersistRecord *r = &p->records[i];
            if (!(r->flags & PERSIST_FLAG_USED) || !(r->flags & PERSIST_FLAG_EXPIRING))
                continue;
            bool was_pending = (r->flags & PERSIST_FLAG_PENDING_GONE) != 0u;
            if ((stage == 0) != was_pending)
                continue;
            if (delivered < p->out_capacity)
                ContactPersist_Deliver(p, r, &p->out[delivered++]);
            else
                r->flags = (uint8_t)((r->flags & ~(PERSIST_FLAG_EXPIRING | PERSIST_FLAG_NOTED)) | PERSIST_FLAG_PENDING_GONE);
        }
    p->pending = expiring - delivered;

    // Reclaim tombstones once they pile up (a same-size rebuild; failure just leaves them in place).
    if (p->tombstones > p->capacity / 8)
        ContactPersist_Rebuild(p, p->capacity);

    *out_count = delivered;
    return p->out;
}

#endif // CONTACT_PERSIST_H
