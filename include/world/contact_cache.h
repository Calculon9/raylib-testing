/**********************************************************************************************
 *
 * CONTACT CACHE AND SOLVER (pure)
 *
 * The collision pipeline is split in two. DETECT (engine glue) runs SAT once per pair, fires every
 * gameplay callback, and writes ONE cached contact per pair, container wall or solid cell into this
 * cache. SOLVE (this header) then iterates over the cached contacts without any geometry tests.
 *
 * CONCEPTS (explained once, here)
 *  - Sequential impulses: each contact is visited in turn and given the impulse that removes the
 *    closing speed along its normal. Bodies share their working velocity (SolverBody), so an impulse
 *    on one contact is seen by the next. Repeating the sweep K times lets weight travel K contacts
 *    per frame instead of one.
 *  - Accumulated, clamped impulses: lambda_n is the TOTAL normal impulse a contact has delivered this
 *    frame. Each iteration adds a delta, but the total is clamped at >= 0 (a contact can push, never
 *    pull), so an over-correction in one iteration can be taken back in the next. Friction is clamped
 *    to the Coulomb cone |lambda_t| <= mu * lambda_n using that same accumulated value.
 *  - Warm starting: a resting stack needs the same supporting impulse every frame. Starting each
 *    frame's iteration from last frame's lambda (matched by ContactKey_*) means the iterations only
 *    correct the change. Without it a stack of 8+ bodies does not converge at K = 4 (design 6.4).
 *  - NGS position phase (non-linear Gauss-Seidel): overlap is removed by moving bodies (a per-body
 *    position delta dp), NOT by adding velocity, so separation injects no kinetic energy. The
 *    separation of a contact is tracked by projection, sep = -depth0 + n . (dp_b - dp_a), so SAT never
 *    has to be re-run during the iterations.
 *  - Restitution threshold: bounce is only requested for closing speeds above a threshold, so resting
 *    contacts (which see about g*dt of closing speed each frame) stay inelastic.
 *
 *  - Static sleepers: a body that is asleep when the solve starts is given zero inverse mass and inertia
 *    (world/sleep_wake.h), exactly like slot 0. An awake body resting on it is supported as by the floor,
 *    takes all of the position correction, and its weight never reaches the sleeper, so a sleeper can
 *    neither be woken by a resting neighbour nor drift. Sleepers wake on EVENTS instead (impact, new
 *    contact, support lost, neighbour waking; see sleep_wake.h and contact_persist.h).
 *
 * Bodies and contacts are arrays of structs. Slot 0 of the body array is a permanent immovable body
 * (zero inverse mass and inertia) used as the partner of container and solid-cell contacts, so the
 * solver loops need no branch for static contacts.
 *
 * THREADING: single threaded, like cell_occupancy.h. MEMORY: TryAllocateBytes only (never
 * AllocateBytes, which exit()s); every failure is non-fatal and reported through flags and stats.
 * The cache logs nothing itself; the caller does. PRECONDITION: TryAllocateBytes returns ZEROED
 * memory (the warm table relies on that to start empty).
 *
 **********************************************************************************************/
#ifndef CONTACT_CACHE_H
#define CONTACT_CACHE_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "entities/entity_id.h"
#include "memory/cmemory.h"
#include "world/collision_pair_key.h"

#define CONTACT_BODY_STATIC 0                    // Body slot 0: the permanent immovable body.
#define CONTACT_CACHE_MAX_CONTACTS (1 << 18)     // Hard ceiling (about 25 MB); contacts beyond it are dropped.
#define CONTACT_CACHE_MIN_CAPACITY 256
#define CONTACT_FLAG_A_SLEEPING 1u               // SolverContact.flags: body A was asleep at detection.
#define CONTACT_FLAG_B_SLEEPING 2u               // body B was asleep at detection.
#define CONTACT_FLAG_NEW 4u                      // The pair had no persistence record before this frame (a NEW contact).
#define BODY_STAT_STATIC 1u                      // BodyStats.flags: body sleeps this frame and acts as a static body in the solve.
#define BODY_STAT_WAKE_LOUD 2u                   // Wake pass: body is marked to wake, and the wake propagates upward.
#define BODY_STAT_WAKE_QUIET 4u                  // Wake pass: body is marked to wake, nothing resting on it is released.
#define BODY_STAT_WOKE_NOW 8u                    // Wake pass: body was woken this frame (its warm-start impulses are cleared).

typedef enum ContactKind
{
    CONTACT_KIND_PAIR = 0,
    CONTACT_KIND_CONTAINER = 1,
    CONTACT_KIND_SOLID_CELL = 2
} ContactKind;

typedef struct SolverBody // 32 B; two per cache line
{
    float vx, vy, w;         // Working velocities (copied from the entity at first touch).
    float inv_mass, inv_inertia;
    float dpx, dpy;          // Accumulated position correction since detection.
    int32_t object_index;    // Index in world->objects; -1 for slot 0.
} SolverBody;

typedef struct SolverContact // 64 B = one cache line (hot: read every iteration)
{
    int32_t body_a, body_b;  // SolverBody slots (b may be CONTACT_BODY_STATIC).
    float nx, ny;            // Unit normal from A to B.
    float rax, ray, rbx, rby; // Contact arms (contact point minus anchor), cached at detection.
    float inv_k_n, inv_k_t;  // 1 / effective mass along the normal / tangent (0 = unusable).
    float bias;              // Restitution separating-speed target (>= 0).
    float lambda_n, lambda_t; // Accumulated normal / tangent impulse (only the solver changes these).
    float depth0;            // SAT depth at detection (> 0).
    float friction;          // Combined Coulomb coefficient mu.
    uint32_t flags;          // CONTACT_FLAG_*_SLEEPING.
} SolverContact;

typedef struct BodyStats // 12 B per body, parallel to bodies[] (kept out of the hot 32 B SolverBody)
{
    EntityId id;             // Entity of this slot (set by ContactCache_SetBodyId; INVALID for slot 0).
    float max_depth0;        // Largest detected depth over the body's contacts (sleep-inhibit guard).
    uint32_t flags;          // BODY_STAT_* bits.
} BodyStats;

typedef struct ContactMeta // 24 B (cold: sort key, warm key, markers, metrics)
{
    EntityId id_a, id_b;     // id_b = INVALID_ENTITY_ID for static contacts.
    uint32_t aux;            // Container: wall id 0..3. Solid cell: cell index.
    uint8_t kind;            // ContactKind
    uint8_t pad[3];
    float px, py;            // Contact point (height-sort key).
} ContactMeta;

typedef struct WarmEntry // 24 B, key 0 = empty
{
    uint64_t key;
    float lambda_n, lambda_t, nx, ny;
} WarmEntry;

typedef struct ContactSolverConfig
{
    int velocity_iterations;       // K
    int position_iterations;       // Kp
    float position_beta;           // Fraction of the remaining overlap removed per position iteration.
    float linear_slop;             // Overlap tolerated so resting contacts persist.
    float max_correction;          // Largest single pair/solid push per position iteration.
    float restitution_threshold;   // Closing speed above which restitution is requested.
    float sleep_inhibit_depth;     // A body overlapped deeper than this cannot start to sleep.
    float wake_impact_speed_min;   // A sleeper is hit awake by an awake body closing faster than this (floor of wake_v_thr).
    float wake_kinematic_speed_min; // A position-locked / rotor body wakes a sleeper it touches above this closing speed.
    float wake_embed_depth;        // A sleeper contact deeper than this means the world changed under it: wake.
    float persist_grace;           // Seconds a touching pair may go unseen before it counts as gone (support lost).
    float support_down_cos;        // A body counts as "below" a sleeper when its direction lies within this cosine of straight down.
} ContactSolverConfig;

typedef struct ContactSolverStats // per frame; read by the profiler line and tests
{
    int contacts, pair_contacts, static_contacts, dropped;
    int warm_hits, warm_lookups, warm_dropped;
    int impulse_contacts;          // Contacts whose final lambda_n > 0.
    float depth_sum;               // Over pair contacts at emit.
    int bodies;
} ContactSolverStats;

typedef struct ContactInput
{
    EntityId id_a, id_b;
    int body_a, body_b;
    ContactKind kind;
    uint32_t aux;
    float nx, ny, px, py;
    float rax, ray, rbx, rby;
    float depth, restitution, friction;
    uint32_t flags;                // CONTACT_FLAG_*_SLEEPING
} ContactInput;

typedef struct ContactCache
{
    SolverContact *contacts;
    ContactMeta *meta;
    uint32_t *order, *order_tmp;   // Solve order (sorted indices) and radix scratch.
    int count, capacity;
    int container_end;             // Container contacts are emitted first: indices below this may be CONTAINER.
    SolverBody *bodies;
    int body_count, body_capacity;
    BodyStats *body_stats;         // Same capacity as bodies.
    int32_t *body_of_object;       // Object index -> body slot (0 = untouched).
    int body_of_object_capacity;
    WarmEntry *warm;
    int warm_capacity, warm_log2, warm_count; // Power-of-two table, load <= 0.5.
    float prev_dt;
    float dt, gravity, v_thr;      // Set by BeginFrame; v_thr = effective restitution threshold.
    float wake_v_thr;              // Set by BeginFrame; closing speed above which a hit wakes a sleeper (independent of v_thr).
    int last_count;
    // Deallocate() free()s the pointer it is given, so the 64-byte-aligned interior pointer is never passed to it.
    void *contacts_raw;
    size_t contacts_raw_bytes;
    size_t meta_bytes, order_bytes, bodies_bytes, body_stats_bytes, map_bytes, warm_bytes;
    bool alloc_failed;             // Set on a failed growth; consumed by TakeAllocFailureReport.
    ContactSolverConfig config;
    ContactSolverStats stats;
} ContactCache;

//----------------------------------------------------------------------------------
// Configuration
//----------------------------------------------------------------------------------

// The shipped settings (design 6.5): K = Kp = 4, beta 0.8, slop 0.01, restitution threshold 1.0.
static inline ContactSolverConfig ContactSolverConfig_Default(void)
{
    ContactSolverConfig cfg = {0};
    cfg.velocity_iterations = 4;
    cfg.position_iterations = 4;
    cfg.position_beta = 0.8f;
    cfg.linear_slop = 0.01f;
    cfg.max_correction = 0.2f;
    cfg.restitution_threshold = 1.0f;
    cfg.sleep_inhibit_depth = 0.05f;
    cfg.wake_impact_speed_min = 1.0f;
    cfg.wake_kinematic_speed_min = 0.1f;
    cfg.wake_embed_depth = 0.1f;
    cfg.persist_grace = 0.06f;
    cfg.support_down_cos = 0.3f;
    return cfg;
}

//----------------------------------------------------------------------------------
// Keys (64-bit, never zero)
//   pair      : (0<<60) | CollisionPairKey(a,b)           slot-index pair, already collision-free
//   container : (1<<60) | (slot_a << 2) | wall_id         wall 0 = left, 1 = right, 2 = top, 3 = bottom
//   solid cell: (2<<60) | (cell_index << 15) | slot_a
// Keys depend on slot indices only, so they survive the generation change of a recycled slot; a stale
// hit is harmless because it only warm starts and lambda is re-clamped.
//----------------------------------------------------------------------------------

static inline uint64_t ContactKey_Pair(EntityId a, EntityId b)
{
    int key = CollisionPairKey(a, b);
    return key > 0 ? (uint64_t)key : 0u;
}

static inline uint64_t ContactKey_Container(EntityId e, int wall)
{
    uint32_t slot = EntityId_GetSlotIndex(e) + 1u;
    if (slot > ENTITY_ID_INDEX_MASK || wall < 0 || wall > 3)
        return 0u;
    return (1ull << 60) | ((uint64_t)slot << 2) | (uint64_t)wall;
}

static inline uint64_t ContactKey_SolidCell(EntityId e, int cell_index)
{
    uint32_t slot = EntityId_GetSlotIndex(e) + 1u;
    if (slot > ENTITY_ID_INDEX_MASK || cell_index < 0)
        return 0u;
    return (2ull << 60) | ((uint64_t)cell_index << 15) | (uint64_t)slot;
}

//----------------------------------------------------------------------------------
// Memory
//----------------------------------------------------------------------------------

static inline int ContactCache_NextPow2(int value, int minimum)
{
    int v = minimum;
    while (v < value && v < (1 << 30))
        v <<= 1;
    return v;
}

// Release the contact-side arrays (contacts, meta, order, order_tmp) via their raw pointers.
static inline void ContactCache_FreeContactArrays(ContactCache *c)
{
    if (c->contacts_raw)
        Deallocate(&c->contacts_raw, c->contacts_raw_bytes);
    if (c->meta)
        Deallocate((void **)&c->meta, c->meta_bytes);
    if (c->order)
        Deallocate((void **)&c->order, c->order_bytes);
    if (c->order_tmp)
        Deallocate((void **)&c->order_tmp, c->order_bytes);
    c->contacts = NULL;
    c->contacts_raw = NULL;
    c->meta = NULL;
    c->order = NULL;
    c->order_tmp = NULL;
}

// Grow the contact-side arrays to `capacity`, copying existing contents. On any failure the old
// buffers stay intact, alloc_failed is set and false is returned.
static inline bool ContactCache_GrowContacts(ContactCache *c, int capacity)
{
    if (capacity <= c->capacity)
        return true;
    size_t contacts_bytes = (size_t)capacity * sizeof(SolverContact) + 63u;
    size_t meta_bytes = (size_t)capacity * sizeof(ContactMeta);
    size_t order_bytes = (size_t)capacity * sizeof(uint32_t);
    void *raw = TryAllocateBytes(contacts_bytes);
    ContactMeta *meta = (ContactMeta *)TryAllocateBytes(meta_bytes);
    uint32_t *order = (uint32_t *)TryAllocateBytes(order_bytes);
    uint32_t *order_tmp = (uint32_t *)TryAllocateBytes(order_bytes);
    if (!raw || !meta || !order || !order_tmp)
    {
        if (raw)
            Deallocate(&raw, contacts_bytes);
        if (meta)
            Deallocate((void **)&meta, meta_bytes);
        if (order)
            Deallocate((void **)&order, order_bytes);
        if (order_tmp)
            Deallocate((void **)&order_tmp, order_bytes);
        c->alloc_failed = true;
        return false;
    }
    // Align the interior pointer to a cache line so one contact is exactly one line.
    SolverContact *contacts = (SolverContact *)(((uintptr_t)raw + 63u) & ~(uintptr_t)63u);
    if (c->count > 0)
    {
        memcpy(contacts, c->contacts, (size_t)c->count * sizeof(SolverContact));
        memcpy(meta, c->meta, (size_t)c->count * sizeof(ContactMeta));
    }
    ContactCache_FreeContactArrays(c);
    c->contacts_raw = raw;
    c->contacts_raw_bytes = contacts_bytes;
    c->contacts = contacts;
    c->meta = meta;
    c->meta_bytes = meta_bytes;
    c->order = order;
    c->order_tmp = order_tmp;
    c->order_bytes = order_bytes;
    c->capacity = capacity;
    return true;
}

// Grow the body arrays to `body_capacity` and the object->body map to `map_capacity`. All-or-nothing.
static inline bool ContactCache_GrowBodies(ContactCache *c, int body_capacity, int map_capacity)
{
    bool grow_bodies = body_capacity > c->body_capacity;
    bool grow_map = map_capacity > c->body_of_object_capacity;
    if (!grow_bodies && !grow_map)
        return true;

    size_t bodies_bytes = (size_t)body_capacity * sizeof(SolverBody);
    size_t stats_bytes = (size_t)body_capacity * sizeof(BodyStats);
    size_t map_bytes = (size_t)map_capacity * sizeof(int32_t);
    SolverBody *bodies = grow_bodies ? (SolverBody *)TryAllocateBytes(bodies_bytes) : NULL;
    BodyStats *stats = grow_bodies ? (BodyStats *)TryAllocateBytes(stats_bytes) : NULL;
    int32_t *map = grow_map ? (int32_t *)TryAllocateBytes(map_bytes) : NULL;
    if ((grow_bodies && (!bodies || !stats)) || (grow_map && !map))
    {
        if (bodies)
            Deallocate((void **)&bodies, bodies_bytes);
        if (stats)
            Deallocate((void **)&stats, stats_bytes);
        if (map)
            Deallocate((void **)&map, map_bytes);
        c->alloc_failed = true;
        return false;
    }
    if (grow_bodies)
    {
        // Slot 0 (and any used slots) carry over; the rest is already zeroed.
        if (c->bodies && c->body_count > 0)
        {
            memcpy(bodies, c->bodies, (size_t)c->body_count * sizeof(SolverBody));
            memcpy(stats, c->body_stats, (size_t)c->body_count * sizeof(BodyStats));
        }
        if (c->bodies)
            Deallocate((void **)&c->bodies, c->bodies_bytes);
        if (c->body_stats)
            Deallocate((void **)&c->body_stats, c->body_stats_bytes);
        c->bodies = bodies;
        c->bodies_bytes = bodies_bytes;
        c->body_stats = stats;
        c->body_stats_bytes = stats_bytes;
        c->body_capacity = body_capacity;
    }
    if (grow_map)
    {
        // The map is rebuilt from the used bodies (cheap and keeps it consistent).
        for (int i = 1; i < c->body_count && c->bodies; i++)
        {
            int32_t obj = c->bodies[i].object_index;
            if (obj >= 0 && obj < map_capacity)
                map[obj] = i;
        }
        if (c->body_of_object)
            Deallocate((void **)&c->body_of_object, c->map_bytes);
        c->body_of_object = map;
        c->map_bytes = map_bytes;
        c->body_of_object_capacity = map_capacity;
    }
    return true;
}

// Grow to at least the requested sizes (powers of two). On failure everything stays as it was,
// alloc_failed is set and false is returned. Never exits.
static inline bool ContactCache_Reserve(ContactCache *c, int min_contacts, int min_bodies, int object_count)
{
    if (min_contacts > CONTACT_CACHE_MAX_CONTACTS)
        min_contacts = CONTACT_CACHE_MAX_CONTACTS;
    bool ok = true;
    if (min_contacts > c->capacity)
        ok = ContactCache_GrowContacts(c, ContactCache_NextPow2(min_contacts, CONTACT_CACHE_MIN_CAPACITY)) && ok;
    int body_cap = min_bodies > c->body_capacity ? ContactCache_NextPow2(min_bodies, CONTACT_CACHE_MIN_CAPACITY) : c->body_capacity;
    int map_cap = object_count > c->body_of_object_capacity ? ContactCache_NextPow2(object_count, CONTACT_CACHE_MIN_CAPACITY) : c->body_of_object_capacity;
    ok = ContactCache_GrowBodies(c, body_cap, map_cap) && ok;
    return ok;
}

// Release every buffer and clear the cache.
static inline void ContactCache_Free(ContactCache *c)
{
    ContactCache_FreeContactArrays(c);
    if (c->bodies)
        Deallocate((void **)&c->bodies, c->bodies_bytes);
    if (c->body_stats)
        Deallocate((void **)&c->body_stats, c->body_stats_bytes);
    if (c->body_of_object)
        Deallocate((void **)&c->body_of_object, c->map_bytes);
    if (c->warm)
        Deallocate((void **)&c->warm, c->warm_bytes);
    memset(c, 0, sizeof(*c));
}

// True once if an allocation failed since the last call, so the caller logs one line per episode.
static inline bool ContactCache_TakeAllocFailureReport(ContactCache *c)
{
    bool failed = c->alloc_failed;
    c->alloc_failed = false;
    return failed;
}

// Start a frame: O(used) reset, restitution threshold, slot 0 re-initialised. Safe when the buffers
// are NULL after a failed initial Reserve (nothing is written; later Add/AcquireBody calls fail and
// the frame runs without contacts). A retry Reserve is attempted here.
static inline void ContactCache_BeginFrame(ContactCache *c, int object_count, float dt, float gravity)
{
    c->last_count = c->count;
    // Clear the object->body map by walking only the bodies used last frame.
    if (c->body_of_object && c->bodies)
    {
        for (int i = 1; i < c->body_count; i++)
        {
            int32_t obj = c->bodies[i].object_index;
            if (obj >= 0 && obj < c->body_of_object_capacity)
                c->body_of_object[obj] = 0;
        }
    }
    c->count = 0;
    c->container_end = 0;
    c->body_count = 0;
    c->dt = dt;
    c->gravity = gravity;
    c->v_thr = fmaxf(c->config.restitution_threshold, 2.0f * fabsf(gravity) * dt);
    c->wake_v_thr = fmaxf(c->config.wake_impact_speed_min, 2.0f * fabsf(gravity) * dt);
    memset(&c->stats, 0, sizeof(c->stats));

    // Size for last frame's load plus headroom; failure leaves the old buffers and the frame continues.
    int want_contacts = (int)((float)c->last_count * 1.25f) + 256;
    int want_bodies = object_count + 1; // At most one body per object, plus the static slot.
    ContactCache_Reserve(c, want_contacts, want_bodies, object_count);

    if (c->bodies && c->body_stats && c->body_capacity > 0)
    {
        memset(&c->bodies[0], 0, sizeof(SolverBody));
        c->bodies[0].object_index = -1;
        memset(&c->body_stats[0], 0, sizeof(BodyStats));
        c->body_count = 1;
    }
}

// Return the body slot for an entity index, appending it on first touch. Returns -1 when the index
// is outside the map (markers may grow the object array mid-detect), the buffers are missing, or the
// array cannot grow.
static inline int ContactCache_AcquireBody(ContactCache *c, int object_index, float vx, float vy, float w,
                                           float inv_mass, float inv_inertia)
{
    if (!c->bodies || !c->body_of_object || (unsigned)object_index >= (unsigned)c->body_of_object_capacity)
        return -1;
    int slot = c->body_of_object[object_index];
    if (slot > 0)
        return slot;
    if (c->body_count >= c->body_capacity)
    {
        int want = c->body_capacity * 2;
        if (!ContactCache_GrowBodies(c, want > 0 ? want : CONTACT_CACHE_MIN_CAPACITY, c->body_of_object_capacity))
            return -1;
    }
    slot = c->body_count++;
    SolverBody *b = &c->bodies[slot];
    memset(b, 0, sizeof(*b));
    b->vx = vx;
    b->vy = vy;
    b->w = w;
    b->inv_mass = inv_mass;
    b->inv_inertia = inv_inertia;
    b->object_index = object_index;
    memset(&c->body_stats[slot], 0, sizeof(BodyStats));
    c->body_of_object[object_index] = slot;
    return slot;
}

// Record which entity owns a body slot (the wake pass maps slots back to entities through this). Idempotent.
static inline void ContactCache_SetBodyId(ContactCache *c, int slot, EntityId id)
{
    if (c->body_stats && slot > 0 && slot < c->body_count)
        c->body_stats[slot].id = id;
}

//----------------------------------------------------------------------------------
// Warm table (open addressing, power-of-two capacity, linear probing, key 0 = empty)
//----------------------------------------------------------------------------------

static inline uint32_t ContactWarm_Hash(uint64_t key, int log2_capacity)
{
    return (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> (64 - log2_capacity));
}

// Find last frame's entry for a key, or NULL. Lookups stop at the first empty slot.
static inline const WarmEntry *ContactWarm_Find(const ContactCache *c, uint64_t key)
{
    if (!c->warm || c->warm_count <= 0 || key == 0u)
        return NULL;
    uint32_t mask = (uint32_t)c->warm_capacity - 1u;
    uint32_t h = ContactWarm_Hash(key, c->warm_log2);
    for (int probe = 0; probe < c->warm_capacity; probe++)
    {
        const WarmEntry *e = &c->warm[h];
        if (e->key == key)
            return e;
        if (e->key == 0u)
            return NULL;
        h = (h + 1u) & mask;
    }
    return NULL;
}

//----------------------------------------------------------------------------------
// Adding contacts
//----------------------------------------------------------------------------------

static inline bool ContactCache_Finite(float v)
{
    return isfinite(v) != 0;
}

// Effective mass along the normal (k_n) and tangent (k_t) of a contact between bodies a and b, from their
// CURRENT inverse masses and inertias. Shared by Add and by the static-sleeper pass (sleep_wake.h), which
// recomputes it after zeroing the sleepers' inverse mass so both use one formula.
static inline void ContactCache_EffectiveMass(const SolverBody *a, const SolverBody *b, float rax, float ray,
                                              float rbx, float rby, float nx, float ny, float *k_n, float *k_t)
{
    float tx = -ny, ty = nx;
    float ra_n = rax * ny - ray * nx;
    float rb_n = rbx * ny - rby * nx;
    float ra_t = rax * ty - ray * tx;
    float rb_t = rbx * ty - rby * tx;
    float inv_mass_sum = a->inv_mass + b->inv_mass;
    *k_n = inv_mass_sum + ra_n * ra_n * a->inv_inertia + rb_n * rb_n * b->inv_inertia;
    *k_t = inv_mass_sum + ra_t * ra_t * a->inv_inertia + rb_t * rb_t * b->inv_inertia;
}

// Compute K_n, K_t and the restitution target from the bodies' CURRENT velocities, look up the warm
// impulse, and append the contact. Returns false and counts a drop on any rejection (invalid input,
// two immovable bodies, full cache). Warm start applies to sleeper contacts too: a sleeper is made
// static before the solve (sleep_wake.h), so the stored impulse only ever reaches the awake side.
static inline bool ContactCache_Add(ContactCache *c, const ContactInput *in)
{
    if (!c->contacts || !c->meta || !c->bodies || c->count >= c->capacity)
    {
        if (c->count >= c->capacity && c->capacity < CONTACT_CACHE_MAX_CONTACTS && c->contacts)
            ContactCache_GrowContacts(c, c->capacity * 2); // In-frame doubling; failure falls through to a drop.
        if (!c->contacts || c->count >= c->capacity)
        {
            c->stats.dropped++;
            return false;
        }
    }
    if (in->body_a <= 0 || in->body_a >= c->body_count || in->body_b < 0 || in->body_b >= c->body_count ||
        in->body_a == in->body_b)
    {
        c->stats.dropped++;
        return false;
    }
    float values[] = {in->nx, in->ny, in->px, in->py, in->rax, in->ray, in->rbx, in->rby,
                      in->depth, in->restitution, in->friction};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        if (!ContactCache_Finite(values[i]))
        {
            c->stats.dropped++;
            return false;
        }
    }
    float nlen = sqrtf(in->nx * in->nx + in->ny * in->ny);
    if (nlen <= 1e-4f || in->depth < 0.0f)
    {
        c->stats.dropped++;
        return false;
    }
    float nx = in->nx / nlen, ny = in->ny / nlen;

    const SolverBody *a = &c->bodies[in->body_a];
    const SolverBody *b = &c->bodies[in->body_b];
    float k_n, k_t;
    ContactCache_EffectiveMass(a, b, in->rax, in->ray, in->rbx, in->rby, nx, ny, &k_n, &k_t);
    if (!(k_n > 1e-8f)) // Two immovable bodies (or NaN): nothing to solve, skipped silently as before.
        return false;

    SolverContact *k = &c->contacts[c->count];
    ContactMeta *m = &c->meta[c->count];
    memset(k, 0, sizeof(*k));
    k->body_a = in->body_a;
    k->body_b = in->body_b;
    k->nx = nx;
    k->ny = ny;
    k->rax = in->rax;
    k->ray = in->ray;
    k->rbx = in->rbx;
    k->rby = in->rby;
    k->inv_k_n = 1.0f / k_n;
    // A position-locked pair can have K_t == 0 (design review: NIT 4): 0 disables friction on the contact.
    k->inv_k_t = (k_t > 1e-8f) ? 1.0f / k_t : 0.0f;
    k->depth0 = in->depth;
    k->friction = in->friction;
    k->flags = in->flags;

    // Closing speed BEFORE any impulse (warm start included); positive = approaching.
    float vax = a->vx - a->w * in->ray, vay = a->vy + a->w * in->rax;
    float vbx = b->vx - b->w * in->rby, vby = b->vy + b->w * in->rbx;
    float vc0 = (vax - vbx) * nx + (vay - vby) * ny;
    k->bias = (vc0 > c->v_thr) ? in->restitution * vc0 : 0.0f;

    memset(m, 0, sizeof(*m));
    m->id_a = in->id_a;
    m->id_b = in->id_b;
    m->aux = in->aux;
    m->kind = (uint8_t)in->kind;
    m->px = in->px;
    m->py = in->py;

    // Warm start lookup. Skipped on the first frame (prev_dt <= 0).
    uint64_t warm_key = 0u;
    if (in->kind == CONTACT_KIND_PAIR)
        warm_key = ContactKey_Pair(in->id_a, in->id_b);
    else if (in->kind == CONTACT_KIND_CONTAINER)
        warm_key = ContactKey_Container(in->id_a, (int)in->aux);
    else
        warm_key = ContactKey_SolidCell(in->id_a, (int)in->aux);
    if (c->prev_dt > 0.0f && c->warm && warm_key != 0u)
    {
        c->stats.warm_lookups++;
        const WarmEntry *w = ContactWarm_Find(c, warm_key);
        if (w)
        {
            c->stats.warm_hits++;
            // Impulse per step scales with dt; a rotated normal cannot reuse the old direction.
            float ratio = c->dt / c->prev_dt;
            ratio = ratio < 0.0f ? 0.0f : (ratio > 2.0f ? 2.0f : ratio);
            float align = fmaxf(0.0f, nx * w->nx + ny * w->ny);
            float scale = ratio * align;
            float ln = w->lambda_n * scale;
            float lt = w->lambda_t * scale;
            float limit = in->friction * ln;
            lt = fmaxf(-limit, fminf(lt, limit));
            if (ContactCache_Finite(ln) && ContactCache_Finite(lt) && ln >= 0.0f)
            {
                k->lambda_n = ln;
                k->lambda_t = lt;
            }
        }
    }

    if (in->kind == CONTACT_KIND_CONTAINER)
        c->container_end = c->count + 1;
    if (in->kind == CONTACT_KIND_PAIR)
    {
        c->stats.pair_contacts++;
        c->stats.depth_sum += in->depth;
    }
    else
        c->stats.static_contacts++;
    c->count++;
    c->stats.contacts = c->count;
    return true;
}

//----------------------------------------------------------------------------------
// Sorting (bottom-up solve order)
//----------------------------------------------------------------------------------

static inline uint32_t ContactCache_SortKey(float y, float y_min, float inv_range, bool descending)
{
    float t = (y - y_min) * inv_range;
    uint32_t key = 0u;
    if (t > 0.0f) // NaN fails the test and maps to 0 (y_min).
        key = t >= 1.0f ? 65535u : (uint32_t)(t * 65535.0f);
    return descending ? 65535u - key : key;
}

// Stable two-pass LSD radix sort of order[] on a 16-bit height key. Gravity > 0 puts the largest y
// (the floor) first, gravity < 0 the smallest y first; gravity == 0 leaves emission order. Ties keep
// emission order. y_min/y_max bound the key range (the grid's vertical extent).
static inline void ContactCache_Sort(ContactCache *c, float y_min, float y_max)
{
    int n = c->count;
    if (n <= 0 || !c->order || !c->order_tmp)
        return;
    for (int i = 0; i < n; i++)
        c->order[i] = (uint32_t)i;
    float range = y_max - y_min;
    if (c->gravity == 0.0f || !(range > 0.0f))
        return;
    bool descending = c->gravity > 0.0f;
    float inv_range = 1.0f / range;

    uint32_t count_lo[256] = {0}, count_hi[256] = {0};
    for (int i = 0; i < n; i++)
    {
        uint32_t key = ContactCache_SortKey(c->meta[i].py, y_min, inv_range, descending);
        count_lo[key & 255u]++;
        count_hi[key >> 8]++;
    }
    uint32_t sum_lo = 0, sum_hi = 0;
    for (int i = 0; i < 256; i++)
    {
        uint32_t lo = count_lo[i], hi = count_hi[i];
        count_lo[i] = sum_lo;
        count_hi[i] = sum_hi;
        sum_lo += lo;
        sum_hi += hi;
    }
    for (int i = 0; i < n; i++) // Pass 1: low byte, emission order in, stable scatter to scratch.
    {
        uint32_t key = ContactCache_SortKey(c->meta[i].py, y_min, inv_range, descending);
        c->order_tmp[count_lo[key & 255u]++] = (uint32_t)i;
    }
    for (int i = 0; i < n; i++) // Pass 2: high byte over pass 1's order.
    {
        uint32_t idx = c->order_tmp[i];
        uint32_t key = ContactCache_SortKey(c->meta[idx].py, y_min, inv_range, descending);
        c->order[count_hi[key >> 8]++] = idx;
    }
}

//----------------------------------------------------------------------------------
// Solver primitives
//----------------------------------------------------------------------------------

// Apply a normal impulse magnitude `dn` (pushes A along -n, B along +n) and a tangent impulse `dt_`
// (+t on A, -t on B). Slot 0 has zero inverse mass and inertia, so writes to it multiply by zero.
static inline void ContactSolver_ApplyImpulse(SolverBody *a, SolverBody *b, const SolverContact *k, float dn, float dtan)
{
    float nx = k->nx, ny = k->ny;
    float tx = -ny, ty = nx;
    float px = nx * dn - tx * dtan; // Impulse on B.
    float py = ny * dn - ty * dtan;
    a->vx -= a->inv_mass * px;
    a->vy -= a->inv_mass * py;
    a->w -= a->inv_inertia * (k->rax * py - k->ray * px);
    b->vx += b->inv_mass * px;
    b->vy += b->inv_mass * py;
    b->w += b->inv_inertia * (k->rbx * py - k->rby * px);
}

// Closing speed along n and sliding speed along t of A relative to B at the contact point.
static inline void ContactSolver_RelativeSpeeds(const SolverBody *a, const SolverBody *b, const SolverContact *k,
                                                float *closing, float *sliding)
{
    float rvx = (a->vx - a->w * k->ray) - (b->vx - b->w * k->rby);
    float rvy = (a->vy + a->w * k->rax) - (b->vy + b->w * k->rbx);
    *closing = rvx * k->nx + rvy * k->ny;
    *sliding = -rvx * k->ny + rvy * k->nx;
}

// Apply last frame's accumulated impulses so the iterations only correct the change.
static inline void ContactSolver_WarmStart(ContactCache *c)
{
    for (int i = 0; i < c->count; i++)
    {
        SolverContact *k = &c->contacts[i];
        if (k->lambda_n == 0.0f && k->lambda_t == 0.0f)
            continue;
        ContactSolver_ApplyImpulse(&c->bodies[k->body_a], &c->bodies[k->body_b], k, k->lambda_n, k->lambda_t);
    }
}

// One normal + friction visit of a contact (accumulated impulses, clamped).
static inline void ContactSolver_SolveContact(ContactCache *c, SolverContact *k)
{
    SolverBody *a = &c->bodies[k->body_a];
    SolverBody *b = &c->bodies[k->body_b];
    float closing, sliding;
    ContactSolver_RelativeSpeeds(a, b, k, &closing, &sliding);

    // Normal: drive the closing speed to -bias; the accumulated impulse can only push.
    float dl = (closing + k->bias) * k->inv_k_n;
    float l_new = fmaxf(k->lambda_n + dl, 0.0f);
    dl = l_new - k->lambda_n;
    k->lambda_n = l_new;
    if (dl != 0.0f)
        ContactSolver_ApplyImpulse(a, b, k, dl, 0.0f);

    // Friction: cancel sliding inside the cone |lambda_t| <= mu * lambda_n. The CLAMPED delta is applied.
    if (k->inv_k_t > 0.0f)
    {
        ContactSolver_RelativeSpeeds(a, b, k, &closing, &sliding);
        float dlt = -sliding * k->inv_k_t;
        float limit = k->friction * k->lambda_n;
        float lt_new = fmaxf(-limit, fminf(k->lambda_t + dlt, limit));
        dlt = lt_new - k->lambda_t;
        k->lambda_t = lt_new;
        if (dlt != 0.0f)
            ContactSolver_ApplyImpulse(a, b, k, 0.0f, dlt);
    }
}

// K iterations over the sorted order: forward on even iterations, reverse on odd, so the weight of a
// stack is carried both up and down.
static inline void ContactSolver_VelocityIterations(ContactCache *c)
{
    int n = c->count;
    for (int it = 0; it < c->config.velocity_iterations; it++)
    {
        if ((it & 1) == 0)
            for (int q = 0; q < n; q++)
                ContactSolver_SolveContact(c, &c->contacts[c->order[q]]);
        else
            for (int q = n - 1; q >= 0; q--)
                ContactSolver_SolveContact(c, &c->contacts[c->order[q]]);
    }
}

// Remaining overlap of a contact after the position deltas so far: -sep = depth0 - n . (dp_b - dp_a).
static inline float ContactSolver_DepthNow(const ContactCache *c, const SolverContact *k)
{
    const SolverBody *a = &c->bodies[k->body_a];
    const SolverBody *b = &c->bodies[k->body_b];
    return k->depth0 - (k->nx * (b->dpx - a->dpx) + k->ny * (b->dpy - a->dpy));
}

// Move A and B apart by `delta` along n, split by inverse mass (a zero-inverse-mass body stays put).
static inline void ContactSolver_PushApart(ContactCache *c, const SolverContact *k, float delta)
{
    SolverBody *a = &c->bodies[k->body_a];
    SolverBody *b = &c->bodies[k->body_b];
    float total = a->inv_mass + b->inv_mass;
    if (!(total > 0.0f) || delta <= 0.0f)
        return;
    float wa = a->inv_mass / total;
    float wb = 1.0f - wa;
    a->dpx -= wa * delta * k->nx;
    a->dpy -= wa * delta * k->ny;
    b->dpx += wb * delta * k->nx;
    b->dpy += wb * delta * k->ny;
}

// Container walls are immovable and the AABB must end flush with them, so the whole remaining depth
// is removed at once (no beta, no max_correction, no slop). This rule is for CONTAINER contacts only.
static inline void ContactSolver_SnapToWall(ContactCache *c, const SolverContact *k)
{
    SolverBody *a = &c->bodies[k->body_a];
    if (!(a->inv_mass > 0.0f))
        return;
    float depth = ContactSolver_DepthNow(c, k);
    if (depth > 0.0f)
    {
        a->dpx -= depth * k->nx;
        a->dpy -= depth * k->ny;
    }
}

// NGS position phase for one visit: pair and solid-cell contacts (an ordinary static partner) are
// rate-limited soft corrections; container contacts snap (see above).
static inline void ContactSolver_PositionContact(ContactCache *c, const SolverContact *k, uint8_t kind)
{
    if (kind == CONTACT_KIND_CONTAINER)
    {
        ContactSolver_SnapToWall(c, k);
        return;
    }
    float violation = ContactSolver_DepthNow(c, k) - c->config.linear_slop; // -(sep + slop)
    if (violation > 0.0f)
        ContactSolver_PushApart(c, k, fminf(c->config.position_beta * violation, c->config.max_correction));
}

// Kp position iterations (same direction rule as the velocity phase) followed by one final sweep over
// the container contacts: the last forward/reverse pass may have pushed a floor-row body back into a
// wall, and the invariant "no AABB outside the grid" is owned by this sweep. Velocities are untouched.
static inline void ContactSolver_PositionIterations(ContactCache *c)
{
    int n = c->count;
    for (int it = 0; it < c->config.position_iterations; it++)
    {
        if ((it & 1) == 0)
            for (int q = 0; q < n; q++)
            {
                uint32_t i = c->order[q];
                ContactSolver_PositionContact(c, &c->contacts[i], c->meta[i].kind);
            }
        else
            for (int q = n - 1; q >= 0; q--)
            {
                uint32_t i = c->order[q];
                ContactSolver_PositionContact(c, &c->contacts[i], c->meta[i].kind);
            }
    }
    for (int i = 0; i < c->container_end; i++)
        if (c->meta[i].kind == CONTACT_KIND_CONTAINER)
            ContactSolver_SnapToWall(c, &c->contacts[i]);
}

//----------------------------------------------------------------------------------
// Per-body results
//----------------------------------------------------------------------------------

// One pass over the contacts: per body the largest detected depth (sleep-inhibit guard). The per-frame
// flags and depth are cleared first; the id is NOT, so it survives to the end of the frame. Slot 0 is not written.
static inline void ContactSolver_AccumulateBodyStats(ContactCache *c)
{
    for (int s = 1; s < c->body_count; s++)
    {
        c->body_stats[s].max_depth0 = 0.0f;
        c->body_stats[s].flags = 0u;
    }
    for (int i = 0; i < c->count; i++)
    {
        const SolverContact *k = &c->contacts[i];
        int slots[2] = {k->body_a, k->body_b};
        for (int side = 0; side < 2; side++)
        {
            if (slots[side] <= 0)
                continue;
            BodyStats *s = &c->body_stats[slots[side]];
            if (k->depth0 > s->max_depth0)
                s->max_depth0 = k->depth0;
        }
    }
}

// Rebuild the warm table from the final contacts (lambda_n, lambda_t, n). The table grows by a
// non-fatal allocation; if growth fails the old table is reused and entries beyond load 0.5 are
// dropped (stats.warm_dropped), so the next frame starts those contacts cold. The first entry for a
// key wins.
static inline void ContactCache_StoreWarm(ContactCache *c)
{
    int needed = ContactCache_NextPow2(c->count * 2, 64);
    if (needed > c->warm_capacity)
    {
        size_t bytes = (size_t)needed * sizeof(WarmEntry);
        WarmEntry *table = (WarmEntry *)TryAllocateBytes(bytes);
        if (table)
        {
            if (c->warm)
                Deallocate((void **)&c->warm, c->warm_bytes);
            c->warm = table;
            c->warm_bytes = bytes;
            c->warm_capacity = needed;
            int log2 = 0;
            while ((1 << log2) < needed)
                log2++;
            c->warm_log2 = log2;
        }
        else
            c->alloc_failed = true;
    }
    c->warm_count = 0;
    int impulse_contacts = 0;
    for (int i = 0; i < c->count; i++)
        if (c->contacts[i].lambda_n > 0.0f)
            impulse_contacts++;
    c->stats.impulse_contacts = impulse_contacts;
    if (!c->warm)
    {
        c->stats.warm_dropped = c->count;
        c->prev_dt = 0.0f;
        return;
    }
    memset(c->warm, 0, c->warm_bytes);
    uint32_t mask = (uint32_t)c->warm_capacity - 1u;
    int limit = c->warm_capacity / 2;
    for (int i = 0; i < c->count; i++)
    {
        const SolverContact *k = &c->contacts[i];
        const ContactMeta *m = &c->meta[i];
        uint64_t key = 0u;
        if (m->kind == CONTACT_KIND_PAIR)
            key = ContactKey_Pair(m->id_a, m->id_b);
        else if (m->kind == CONTACT_KIND_CONTAINER)
            key = ContactKey_Container(m->id_a, (int)m->aux);
        else
            key = ContactKey_SolidCell(m->id_a, (int)m->aux);
        if (key == 0u || (k->lambda_n == 0.0f && k->lambda_t == 0.0f))
            continue; // Nothing worth remembering.
        if (c->warm_count >= limit)
        {
            c->stats.warm_dropped++;
            continue;
        }
        uint32_t h = ContactWarm_Hash(key, c->warm_log2);
        bool duplicate = false;
        while (c->warm[h].key != 0u)
        {
            if (c->warm[h].key == key)
            {
                duplicate = true;
                break;
            }
            h = (h + 1u) & mask;
        }
        if (duplicate)
            continue;
        c->warm[h] = (WarmEntry){key, k->lambda_n, k->lambda_t, k->nx, k->ny};
        c->warm_count++;
    }
    c->prev_dt = c->dt;
}

#endif // CONTACT_CACHE_H
