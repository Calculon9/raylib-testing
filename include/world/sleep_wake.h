/**********************************************************************************************
 *
 * STATIC SLEEPERS AND EVENT WAKING (pure)
 *
 * A sleeping body is IMMOVABLE in the solver and wakes on EVENTS, never on how hard it is leant on.
 * Before the solve every body that is still asleep is given zero inverse mass and inertia and zero working
 * velocity (like slot 0 in contact_cache.h), and the effective masses of its contacts are recomputed. An
 * awake body resting on a sleeper is therefore supported exactly as by the floor: it takes all the position
 * correction and its weight never reaches the sleeper. Because a zero-inverse-mass body cannot be moved by
 * the velocity or the NGS position phase, sleepers cannot drift either.
 *
 * WAKE EVENTS (evaluated once per frame, after detection and before the solve)
 *   A. IMPACT          an awake body closes on the sleeper faster than c->wake_v_thr.        LOUD
 *   B. NEW CONTACT     the pair had no persistence record before this frame.                 QUIET
 *   C. SUPPORT LOST    a persistence record whose other end lay below the sleeper has gone.  LOUD
 *   D. NEIGHBOUR WAKES a body below the sleeper was loudly woken last frame and the pair
 *                      touched on the frame before (read from the RECORDS, not from this
 *                      frame's contacts: the woken body may already have moved away).       LOUD
 *   E. KINEMATIC TOUCH an awake position-locked body (inverse mass 0) moves into it.         QUIET (guard)
 *   F. EMBEDDED        a sleeper contact deeper than wake_embed_depth.                       QUIET (guard)
 *
 * LOUD vs QUIET: a loud wake means the body may move, so whatever rests on it must be released: the entity is
 * flagged WOKEN and rule D carries the wave up ONE LAYER PER FRAME (a tall column unzips over a few frames).
 * A quiet wake only makes the body dynamic so it can be nudged or corrected; it releases nothing.
 *
 * Position-locked sleepers (inverse mass and inertia both 0) can never change state, so no rule wakes them.
 * Rotors (inverse mass 0, inertia > 0) wake quietly on A, B and E only.
 *
 * Support side: gravity-relative "below". The direction from the sleeper to the other body must lie within
 * support_down_cos of straight down. With zero gravity every contact counts.
 *
 * The decision logic is here so the engine and the tests run the same code; the engine supplies only the
 * SleepWakeHost callbacks (entity lookups and the wake itself).
 *
 * THREADING: single threaded. MEMORY: TryAllocateBytes only; a failed list growth is non-fatal.
 *
 **********************************************************************************************/
#ifndef SLEEP_WAKE_H
#define SLEEP_WAKE_H

#include "world/contact_cache.h"
#include "world/contact_persist.h"

typedef enum SleeperKind
{
    SLEEPER_NONE = 0,    // Awake, missing or not resolvable.
    SLEEPER_DYNAMIC = 1, // Asleep and able to move.
    SLEEPER_ROTOR = 2,   // Asleep, inverse mass 0 but can rotate.
    SLEEPER_LOCKED = 3   // Asleep, inverse mass and inertia 0: waking it changes nothing.
} SleeperKind;

typedef struct SleepWakeHost // The only window onto entities; the engine fills it with world_contacts.c glue.
{
    void *user;
    int (*sleeper_kind)(void *user, EntityId id);            // SleeperKind of the entity right now.
    bool (*wake)(void *user, EntityId id, bool loud);        // WakeUp (loud) or WakeUpQuiet; true if it was asleep.
} SleepWakeHost;

typedef struct SleepWakeScratch // Growable lists owned by the world and reused every frame.
{
    EntityId *woken;                    // Every body woken by the last Resolve (loud or quiet), sorted afterwards.
    int woken_count, woken_capacity;
    EntityId *prev;                     // Bodies loudly woken since the last Resolve (the caller fills this first).
    int prev_count, prev_capacity;
    size_t bytes;
    bool alloc_failed;
} SleepWakeScratch;

//----------------------------------------------------------------------------------
// Lists
//----------------------------------------------------------------------------------

static inline bool SleepWake_ListPush(SleepWakeScratch *s, EntityId **list, int *count, int *capacity, EntityId id)
{
    if (*count >= *capacity)
    {
        int new_capacity = *capacity > 0 ? *capacity * 2 : 64;
        EntityId *grown = (EntityId *)TryAllocateBytes((size_t)new_capacity * sizeof(EntityId));
        if (!grown)
        {
            s->alloc_failed = true;
            return false;
        }
        if (*list)
        {
            memcpy(grown, *list, (size_t)*count * sizeof(EntityId));
            Deallocate((void **)list, (size_t)*capacity * sizeof(EntityId));
            s->bytes -= (size_t)*capacity * sizeof(EntityId);
        }
        *list = grown;
        *capacity = new_capacity;
        s->bytes += (size_t)new_capacity * sizeof(EntityId);
    }
    (*list)[(*count)++] = id;
    return true;
}

// Forget last frame's lists (keeps the buffers). Call before filling `prev`.
static inline void SleepWake_BeginFrame(SleepWakeScratch *s)
{
    s->woken_count = 0;
    s->prev_count = 0;
}

// Add a body that was loudly woken since the last Resolve (WOKEN flag read by the caller).
static inline void SleepWake_AddPrevWoken(SleepWakeScratch *s, EntityId id)
{
    SleepWake_ListPush(s, &s->prev, &s->prev_count, &s->prev_capacity, id);
}

static inline void SleepWake_Free(SleepWakeScratch *s)
{
    if (s->woken)
        Deallocate((void **)&s->woken, (size_t)s->woken_capacity * sizeof(EntityId));
    if (s->prev)
        Deallocate((void **)&s->prev, (size_t)s->prev_capacity * sizeof(EntityId));
    memset(s, 0, sizeof(*s));
}

// True once if a list could not grow since the last call.
static inline bool SleepWake_TakeAllocFailureReport(SleepWakeScratch *s)
{
    bool failed = s->alloc_failed;
    s->alloc_failed = false;
    return failed;
}

// True when the body was woken by the last Resolve (binary search of the sorted list).
static inline bool SleepWake_WasWoken(const SleepWakeScratch *s, EntityId id)
{
    return s->woken_count > 0 && ContactPersist_IdListed(s->woken, s->woken_count, id);
}

//----------------------------------------------------------------------------------
// Support side
//----------------------------------------------------------------------------------

// True when a body in direction `dir_y` (the y component of the unit direction FROM THE SLEEPER TO THE OTHER
// BODY) lies below the sleeper with respect to gravity: gravity > 0 pulls toward +y, so "below" is +y.
// With zero gravity there is no below and every contact counts.
static inline bool SleepWake_IsSupportSide(float dir_y, float gravity, float down_cos)
{
    if (gravity == 0.0f)
        return true;
    float g_sign = gravity > 0.0f ? 1.0f : -1.0f;
    return g_sign * dir_y > down_cos;
}

// y component of the direction from `sleeper` to the other end of a canonical record whose stored normal
// (quantised, y part `dir_y_ab`) points from id_a to id_b: as stored when the sleeper is A, negated when B.
static inline float SleepWake_DirFromSleeper(int8_t dir_y_ab, EntityId id_a, EntityId sleeper)
{
    float y = (float)dir_y_ab / 127.0f;
    return sleeper == id_a ? y : -y;
}

//----------------------------------------------------------------------------------
// Wake evaluation
//----------------------------------------------------------------------------------

// Wake one body through the host and remember it. Returns true if it was asleep.
static inline bool SleepWake_Commit(const SleepWakeHost *host, SleepWakeScratch *s, EntityId id, bool loud)
{
    if (id == INVALID_ENTITY_ID || !host->wake(host->user, id, loud))
        return false;
    SleepWake_ListPush(s, &s->woken, &s->woken_count, &s->woken_capacity, id);
    return true;
}

// Mark the sleeping side of every sleeper contact with the events A, B, E and F (contact-based rules).
static inline void SleepWake_MarkContactEvents(ContactCache *c)
{
    for (int i = 0; i < c->count; i++)
    {
        const SolverContact *k = &c->contacts[i];
        bool a_sleeps = (k->flags & CONTACT_FLAG_A_SLEEPING) != 0u;
        bool b_sleeps = (k->flags & CONTACT_FLAG_B_SLEEPING) != 0u;
        if (a_sleeps == b_sleeps)
            continue; // Both awake (nothing to wake) or both asleep (a sensor pair; no awake side).
        int sleeper = a_sleeps ? k->body_a : k->body_b;
        int other = a_sleeps ? k->body_b : k->body_a;
        if (sleeper <= 0)
            continue;
        const SolverBody *sb = &c->bodies[sleeper];
        if (sb->inv_mass == 0.0f && sb->inv_inertia == 0.0f)
            continue; // Position-locked: nothing a wake could change.
        bool rotor = sb->inv_mass == 0.0f;
        uint32_t mark = 0u;

        if (other > 0)
        {
            // Closing speed of the awake body toward the sleeper (the sleeper's own velocity is zero).
            float closing, sliding;
            ContactSolver_RelativeSpeeds(&c->bodies[k->body_a], &c->bodies[k->body_b], k, &closing, &sliding);
            float approach = a_sleeps ? -closing : closing;
            if (approach > c->wake_v_thr)
                mark |= rotor ? BODY_STAT_WAKE_QUIET : BODY_STAT_WAKE_LOUD; // A. IMPACT
            if (k->flags & CONTACT_FLAG_NEW)
                mark |= BODY_STAT_WAKE_QUIET;                                  // B. NEW CONTACT
            if (c->bodies[other].inv_mass == 0.0f && approach > c->config.wake_kinematic_speed_min)
                mark |= BODY_STAT_WAKE_QUIET;                                  // E. KINEMATIC TOUCH
        }
        if (!rotor && k->depth0 > c->config.wake_embed_depth)
            mark |= BODY_STAT_WAKE_QUIET;                                      // F. EMBEDDED (pair or static contact)
        c->body_stats[sleeper].flags |= mark;
    }
}

// Decide and apply all wakes of this frame. The caller has filled s->prev (bodies loudly woken since the last
// pass) and run ContactPersist_Sweep's records through `gone`. Order:
//   1. C. SUPPORT LOST from the delivered gone records, then D. NEIGHBOUR WAKES from the persistence records.
//      Both are woken at once; neither depends on contact order, and a body woken here propagates NEXT frame
//      (it is not in s->prev), so nothing cascades within the pass.
//   2. A, B, E, F from the contacts, marked on body_stats and then committed in slot order (loud wins).
// Every body woken is appended to s->woken (sorted before return). Returns the number woken.
static inline int SleepWake_Resolve(ContactCache *c, ContactPersist *p, const GonePair *gone, int gone_count,
                                    const SleepWakeHost *host, SleepWakeScratch *s)
{
    s->woken_count = 0;

    // C. SUPPORT LOST: wake an asleep end that rested on the other end of a pair that has gone.
    for (int i = 0; i < gone_count; i++)
    {
        const GonePair *g = &gone[i];
        EntityId ends[2] = {g->id_a, g->id_b};
        for (int e = 0; e < 2; e++)
        {
            if (host->sleeper_kind(host->user, ends[e]) != SLEEPER_DYNAMIC)
                continue;
            if (SleepWake_IsSupportSide(SleepWake_DirFromSleeper(g->dir_y, g->id_a, ends[e]), c->gravity,
                                        c->config.support_down_cos))
                SleepWake_Commit(host, s, ends[e], true);
        }
    }

    // D. NEIGHBOUR WAKES: a body loudly woken last frame releases the sleepers that rested on it, if the pair
    // touched or was carried on the frame before this one. Read from the records so the wave does not depend
    // on fresh SAT contacts (the woken body has usually moved by now).
    if (s->prev_count > 0 && p->records && p->stamp > 0u)
    {
        if (s->prev_count > 1)
            qsort(s->prev, (size_t)s->prev_count, sizeof(EntityId), ContactPersist_CompareIds);
        for (int i = 0; i < p->capacity; i++)
        {
            const PersistRecord *r = &p->records[i];
            if (!ContactPersist_RecordLive(r) || r->last_stamp + 1u < p->stamp)
                continue;
            EntityId ends[2] = {r->id_a, r->id_b};
            for (int e = 0; e < 2; e++)
            {
                EntityId woken_end = ends[1 - e], sleeper = ends[e];
                if (!ContactPersist_IdListed(s->prev, s->prev_count, woken_end) ||
                    host->sleeper_kind(host->user, sleeper) != SLEEPER_DYNAMIC)
                    continue;
                if (SleepWake_IsSupportSide(SleepWake_DirFromSleeper(r->dir_y, r->id_a, sleeper), c->gravity,
                                            c->config.support_down_cos))
                    SleepWake_Commit(host, s, sleeper, true);
            }
        }
    }

    // A, B, E, F from this frame's contacts.
    if (c->bodies && c->body_stats)
    {
        SleepWake_MarkContactEvents(c);
        for (int slot = 1; slot < c->body_count; slot++)
        {
            uint32_t flags = c->body_stats[slot].flags;
            if (flags & BODY_STAT_WAKE_LOUD)
                SleepWake_Commit(host, s, c->body_stats[slot].id, true);
            else if (flags & BODY_STAT_WAKE_QUIET)
                SleepWake_Commit(host, s, c->body_stats[slot].id, false);
        }
    }

    if (s->woken_count > 1)
        qsort(s->woken, (size_t)s->woken_count, sizeof(EntityId), ContactPersist_CompareIds);
    return s->woken_count;
}

// A woken body starts its first dynamic solve with no stored impulse: whatever the warm table held was built
// while the body was static (or against a body that was static), so it would be a one-frame transient. Call
// after any late detection, so the late contacts are cleared too.
static inline void SleepWake_ClearWarmOfWoken(ContactCache *c, const SleepWakeScratch *s)
{
    if (s->woken_count <= 0 || !c->body_stats)
        return;
    for (int slot = 1; slot < c->body_count; slot++)
        if (SleepWake_WasWoken(s, c->body_stats[slot].id))
            c->body_stats[slot].flags |= BODY_STAT_WOKE_NOW;
    for (int i = 0; i < c->count; i++)
    {
        SolverContact *k = &c->contacts[i];
        if ((c->body_stats[k->body_a].flags | c->body_stats[k->body_b].flags) & BODY_STAT_WOKE_NOW)
        {
            k->lambda_n = 0.0f;
            k->lambda_t = 0.0f;
        }
    }
}

//----------------------------------------------------------------------------------
// Static treatment
//----------------------------------------------------------------------------------

// Make every body flagged BODY_STAT_STATIC immovable: zero inverse mass and inertia, zero working velocity.
static inline void ContactCache_MakeSleepersStatic(ContactCache *c)
{
    if (!c->bodies || !c->body_stats)
        return;
    for (int slot = 1; slot < c->body_count; slot++)
    {
        if (!(c->body_stats[slot].flags & BODY_STAT_STATIC))
            continue;
        SolverBody *b = &c->bodies[slot];
        b->inv_mass = 0.0f;
        b->inv_inertia = 0.0f;
        b->vx = b->vy = b->w = 0.0f;
    }
}

// Recompute K_n / K_t of every contact that touches a static sleeper. A contact whose two bodies are both
// immovable (a sleeper against the container or a solid cell) gets inverse K = 0: the solver then computes a
// zero impulse for it without any branch in the hot loop.
static inline void ContactCache_RefreshEffectiveMass(ContactCache *c)
{
    if (!c->bodies || !c->body_stats)
        return;
    for (int i = 0; i < c->count; i++)
    {
        SolverContact *k = &c->contacts[i];
        if (!((c->body_stats[k->body_a].flags | c->body_stats[k->body_b].flags) & BODY_STAT_STATIC))
            continue;
        float k_n, k_t;
        ContactCache_EffectiveMass(&c->bodies[k->body_a], &c->bodies[k->body_b], k->rax, k->ray, k->rbx, k->rby,
                                   k->nx, k->ny, &k_n, &k_t);
        k->inv_k_n = k_n > 1e-8f ? 1.0f / k_n : 0.0f;
        k->inv_k_t = k_t > 1e-8f ? 1.0f / k_t : 0.0f;
    }
}

#endif // SLEEP_WAKE_H
