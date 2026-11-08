/**********************************************************************************************
 *
 * WORLD CONTACTS (detect/solve glue)
 *
 * Connects the engine to the pure contact solver in world/contact_cache.h. DETECT code (the pair,
 * container and solid-cell detectors) calls the Emit* functions to cache contacts; SolveWorldContacts
 * runs the solver over them and is the ONLY place that writes the results back to entities.
 *
 **********************************************************************************************/
#include "world/world_internal.h"
#include "world/container_correction.h"
#include "world/contact_point.h"

// Physics step used by the solver: the same clamped delta as PhysicsUpdateJob and the old container pass.
float WorldPhysicsDeltaTime(void)
{
    return fminf(frame_counter.delta_time, MAX_PHYSICS_DELTA_TIME);
}

// Find the body slot of an entity, gathering its working state on first touch. Returns -1 when the
// entity is not inside the object array or the cache cannot take another body.
static int AcquireEntityBody(World2d *world, const Newtonoid2d *entity)
{
    const Newtonoid2d *base = (const Newtonoid2d *)world->objects.items;
    ptrdiff_t index = entity - base;
    if (index < 0 || index >= (ptrdiff_t)world->objects.count)
        return -1;
    int slot = ContactCache_AcquireBody(&world->contact_cache, (int)index, entity->velocity.x, entity->velocity.y,
                                        entity->angular_velocity, entity->inverse_mass, entity->inverse_inertia);
    // The wake pass maps body slots back to entities through this id.
    ContactCache_SetBodyId(&world->contact_cache, slot, entity->id);
    return slot;
}

// Cache one pair contact from a SAT result. The pair is put in canonical order (lower entity slot first,
// normal negated if swapped) so warm-start keys and the tangent sign do not depend on which body the
// occupancy store listed first.
void EmitPairContact(World2d *world, Newtonoid2d *a, Newtonoid2d *b, const CollisionResult_SAT *result)
{
    Vector2d normal = result->collision_normal;
    if (EntityId_GetSlotIndex(a->id) > EntityId_GetSlotIndex(b->id))
    {
        Newtonoid2d *swap = a;
        a = b;
        b = swap;
        normal = VectorScale_2d(normal, -1.0f);
    }

    int body_a = AcquireEntityBody(world, a);
    int body_b = AcquireEntityBody(world, b);
    if (body_a < 0 || body_b < 0)
    {
        world->contact_cache.stats.dropped++;
        return;
    }

    Vector2d point = result->contact_point;
    ContactInput in = {0};
    // Touch the persistence record: a pair with no record yet is a NEW contact (wake event B for a sleeper).
    if (!ContactPersist_Touch(&world->contact_persist, a->id, b->id, normal.x, normal.y))
        in.flags |= CONTACT_FLAG_NEW;
    in.id_a = a->id;
    in.id_b = b->id;
    in.body_a = body_a;
    in.body_b = body_b;
    in.kind = CONTACT_KIND_PAIR;
    in.nx = normal.x;
    in.ny = normal.y;
    in.px = point.x;
    in.py = point.y;
    in.rax = point.x - a->anchor_position.x;
    in.ray = point.y - a->anchor_position.y;
    in.rbx = point.x - b->anchor_position.x;
    in.rby = point.y - b->anchor_position.y;
    in.depth = result->penetration_depth;
    in.restitution = CalcCollisionRestitution(a, b);
    in.friction = CalcCollisionFriction(a, b);
    in.flags |= ((a->status_flags & ENTITY_STATUS_FLAG_SLEEPING) ? CONTACT_FLAG_A_SLEEPING : 0u) |
                ((b->status_flags & ENTITY_STATUS_FLAG_SLEEPING) ? CONTACT_FLAG_B_SLEEPING : 0u);
    ContactCache_Add(&world->contact_cache, &in);
}

// Cache one static contact for an entity against the container wall or a solid cell. The partner is the
// solver's immovable slot 0, so the loops need no static branch. Normal points from the entity into the wall.
void EmitStaticContact(World2d *world, Newtonoid2d *entity, ContactKind kind, uint32_t aux, Vector2d normal_to_wall,
                       Vector2d contact_point, float depth)
{
    int body = AcquireEntityBody(world, entity);
    if (body < 0)
    {
        world->contact_cache.stats.dropped++;
        return;
    }
    const Newtonoid2d *container = &world->grid_space.object;
    ContactInput in = {0};
    in.id_a = entity->id;
    in.id_b = INVALID_ENTITY_ID;
    in.body_a = body;
    in.body_b = CONTACT_BODY_STATIC;
    in.kind = kind;
    in.aux = aux;
    in.nx = normal_to_wall.x;
    in.ny = normal_to_wall.y;
    in.px = contact_point.x;
    in.py = contact_point.y;
    in.rax = contact_point.x - entity->anchor_position.x;
    in.ray = contact_point.y - entity->anchor_position.y;
    in.depth = depth;
    in.restitution = CalcCollisionRestitution(entity, container);
    in.friction = CalcCollisionFriction(entity, container);
    in.flags = (entity->status_flags & ENTITY_STATUS_FLAG_SLEEPING) ? CONTACT_FLAG_A_SLEEPING : 0u;
    ContactCache_Add(&world->contact_cache, &in);
}

// Contact point of an entity against a wall or solid cell: the deepest support face's centre. `inward` points from
// the wall into the legal region. The tolerance is relative to the penetration and the entity's AABB half-diagonal.
Vector2d StaticContactPoint(const Newtonoid2d *entity, const Vector2d *vertices, size_t count, Vector2d inward, float depth)
{
    float extent = 0.5f * sqrtf(entity->bounds_size.x * entity->bounds_size.x + entity->bounds_size.y * entity->bounds_size.y);
    return FindDeepestSupportInterval(vertices, count, inward, ContactPoint_FaceTolerance(depth, extent), NULL, NULL);
}

// True for the physical, world-owned bodies the container acts on (the PhysicsUpdateJob gate).
static bool IsContainerBody(const World2d *world, const Newtonoid2d *entity)
{
    return (entity->status_flags & ENTITY_STATUS_FLAG_ALIVE) && !(entity->roles & ENTITY_ROLE_EFFECT) &&
           entity->parent_id == world->grid_space.object.id;
}

// DETECT the container walls. The legal region is the grid rectangle in space-local units; each penetrated
// axis (left/right, top/bottom) becomes one static contact carrying that axis' depth. No NO_CONTACT_RESPONSE
// test, as before; role masks must match in both directions. Vertices are transformed only for the few
// entities actually penetrating a wall.
void DetectContainerContacts(World2d *world)
{
    const Newtonoid2d *container = &world->grid_space.object;
    const Space2d *space = &world->grid_space.space;
    Vector2d grid_min = space->grid_origin;
    Vector2d grid_max = {space->grid_origin.x + (float)space->columns, space->grid_origin.y + (float)space->rows};

    Newtonoid2d *entity = NULL;
    LArray_ForEach(&world->objects, Newtonoid2d *, entity)
    {
        if (!IsContainerBody(world, entity))
            continue;
        if (!(entity->collision_role_mask & container->roles) || !(container->collision_role_mask & entity->roles))
            continue;

        ContainerCorrection correction = ContainerCorrection_Compute(entity->bounds_origin, entity->bounds_size,
                                                                     entity->anchor_position, grid_min, grid_max);
        if (!(correction.depth_x > 0.0f) && !(correction.depth_y > 0.0f))
            continue;

        Vector2d vertices[MAX_SHAPE_VERTICES] = {0};
        int count = (int)entity->surface.surface_vectors.count;
        if (count < 1)
            continue;
        if (count > MAX_SHAPE_VERTICES)
            count = MAX_SHAPE_VERTICES;
        Newtonoid_TransformVertices(entity, vertices, MAX_SHAPE_VERTICES);

        for (int axis = 0; axis < 2; axis++)
        {
            float depth = axis == 0 ? correction.depth_x : correction.depth_y;
            if (!(depth > 0.0f))
                continue;
            Vector2d inward = axis == 0 ? correction.normal_x : correction.normal_y;
            // Wall ids: 0 = left, 1 = right, 2 = top, 3 = bottom (warm-start key).
            int wall = axis == 0 ? (inward.x > 0.0f ? 0 : 1) : (inward.y > 0.0f ? 2 : 3);
            Vector2d point = StaticContactPoint(entity, vertices, (size_t)count, inward, depth);
            EmitStaticContact(world, entity, CONTACT_KIND_CONTAINER, (uint32_t)wall,
                              VectorScale_2d(inward, -1.0f), point, depth);
        }
    }
}

// Safety net after the solve: move any entity whose CENTRE escaped the grid back inside, shifting anchor
// and bounds together. The container contacts already removed wall penetration (final static sweep); this
// only covers an axis with no detected contact and oversized bodies. No impulse is applied.
void ClampEntitiesToContainer(World2d *world)
{
    const Newtonoid2d *container = &world->grid_space.object;
    const Space2d *space = &world->grid_space.space;
    Vector2d grid_min = space->grid_origin;
    Vector2d grid_max = {space->grid_origin.x + (float)space->columns, space->grid_origin.y + (float)space->rows};

    Newtonoid2d *entity = NULL;
    LArray_ForEach(&world->objects, Newtonoid2d *, entity)
    {
        if (!IsContainerBody(world, entity))
            continue;
        if (!(entity->collision_role_mask & container->roles) || !(container->collision_role_mask & entity->roles))
            continue;
        Vector2d shift = ContainerCorrection_ClampCentre(entity->anchor_position, grid_min, grid_max);
        if (shift.x != 0.0f || shift.y != 0.0f)
        {
            // A moved body must not stay a static sleeper: wake it and release what rested on it.
            WakeUp(entity);
            ContactPersist_NoteGone(&world->contact_persist, entity->id);
            entity->anchor_position = VectorSum_2d(entity->anchor_position, shift);
            entity->bounds_origin = VectorSum_2d(entity->bounds_origin, shift);
        }
    }
}

// Write the solved state back to the entities (the only entity writes of the solve phase). Bodies are
// reloaded by object index each time: collision markers may have grown the object array during detect.
// A body that is still asleep is a static body in the solve (see SolveWorldContacts): it is never written
// back, so it keeps the zero velocity and the position it went to sleep with.
static void ApplySolverResults(World2d *world)
{
    ContactCache *cache = &world->contact_cache;
    Newtonoid2d *entities = (Newtonoid2d *)world->objects.items;
    int object_count = (int)world->objects.count;

    for (int slot = 1; slot < cache->body_count; slot++)
    {
        const SolverBody *body = &cache->bodies[slot];
        if (body->object_index < 0 || body->object_index >= object_count)
            continue;
        if (!isfinite(body->vx) || !isfinite(body->vy) || !isfinite(body->w) ||
            !isfinite(body->dpx) || !isfinite(body->dpy))
        {
            LOG_WARN_LIMIT(1, 1.0f, "Contact solver produced a non-finite body state; entity %d keeps its old state.\n",
                           entities[body->object_index].id);
            continue;
        }

        Newtonoid2d *entity = &entities[body->object_index];
        if (entity->status_flags & ENTITY_STATUS_FLAG_SLEEPING)
            continue; // Static sleeper: its state is final (velocity zero, no position delta by construction).

        entity->velocity = (Vector2d){body->vx, body->vy};
        entity->angular_velocity = body->w;
        entity->momentum = VectorScale_2d(entity->velocity, entity->mass);

        // Positional correction accumulated by the solver; anchor and bounds origin always move together.
        if (body->dpx != 0.0f || body->dpy != 0.0f)
        {
            Vector2d shift = {body->dpx, body->dpy};
            entity->anchor_position = VectorSum_2d(entity->anchor_position, shift);
            entity->bounds_origin = VectorSum_2d(entity->bounds_origin, shift);
        }

        // A body overlapped deeper than the inhibit depth must not start sleeping (sleep would freeze
        // the overlap, because sleeping pairs are skipped by detection). Only the velocity solver
        // makes resting bodies quiet enough to sleep.
        if (cache->body_stats[slot].max_depth0 > cache->config.sleep_inhibit_depth)
            entity->sleep_timer = 0.0f;

        Newtonoid_SyncOrientationToVelocity(entity);
    }
}

// Wake-pass host callback: classify an entity that is asleep right now (see SleeperKind).
static int WakeHost_SleeperKind(void *user, EntityId id)
{
    const Newtonoid2d *entity = (const Newtonoid2d *)GetEntityByID((World2d *)user, id);
    if (!entity || !(entity->status_flags & ENTITY_STATUS_FLAG_SLEEPING))
        return SLEEPER_NONE;
    if (entity->inverse_mass == 0.0f)
        return entity->inverse_inertia == 0.0f ? SLEEPER_LOCKED : SLEEPER_ROTOR;
    return SLEEPER_DYNAMIC;
}

// Wake-pass host callback: wake a sleeping entity (loud flags it WOKEN so the wake propagates). False if it
// was not asleep or no longer resolves.
static bool WakeHost_Wake(void *user, EntityId id, bool loud)
{
    Newtonoid2d *entity = (Newtonoid2d *)GetEntityByID((World2d *)user, id);
    if (!entity || !(entity->status_flags & ENTITY_STATUS_FLAG_SLEEPING))
        return false;
    if (loud)
        WakeUp(entity);
    else
        WakeUpQuiet(entity);
    return true;
}

// WAKE pass of the frame, between detection and the solve (static sleepers + event waking, sleep_wake.h).
// 1. Collect the bodies loudly woken since the last pass (WOKEN flag, read and cleared here).
// 2. Sweep the persistence set and wake sleepers on impact, new contact, support lost and neighbour waking.
// 3. A body woken now may have pairs with sleeping neighbours that were only CARRIED, not emitted. Detect
//    those through DetectCollisionPair (carried pairs are not marked resolved, so each pair's gameplay
//    callbacks still fire once), so the newly dynamic body is constrained in this same frame.
// Runs every tick, also with no contacts: support loss needs none.
void ResolveSleepWakes(World2d *world)
{
    ContactCache *cache = &world->contact_cache;
    ContactPersist *persist = &world->contact_persist;
    SleepWakeScratch *scratch = &world->sleep_wake_scratch;

    SleepWake_BeginFrame(scratch);
    Newtonoid2d *entity = NULL;
    LArray_ForEach(&world->objects, Newtonoid2d *, entity)
    {
        if (entity->status_flags & ENTITY_STATUS_FLAG_WOKEN)
        {
            entity->status_flags &= ~ENTITY_STATUS_FLAG_WOKEN;
            SleepWake_AddPrevWoken(scratch, entity->id);
        }
    }

    int gone_count = 0;
    const GonePair *gone = ContactPersist_Sweep(persist, &gone_count);
    SleepWakeHost host = {world, WakeHost_SleeperKind, WakeHost_Wake};
    int woken = SleepWake_Resolve(cache, persist, gone, gone_count, &host, scratch);

    if (woken > 0 && persist->records)
    {
        // Late detection. DetectCollisionPair only refreshes existing records here (Touch never inserts for a
        // carried pair), so the table is not rebuilt under this loop. A collision marker may grow the object array.
        for (int i = 0; i < persist->capacity; i++)
        {
            const PersistRecord *record = &persist->records[i];
            if (!ContactPersist_RecordLive(record) || !ContactPersist_IsCarriedThisFrame(persist, record))
                continue;
            if (SleepWake_WasWoken(scratch, record->id_a) || SleepWake_WasWoken(scratch, record->id_b))
                DetectCollisionPair(world, record->id_a, record->id_b, -1, &world->resolved_collisions,
                                    &world->scheduled_world_cmds);
        }
        SleepWake_ClearWarmOfWoken(cache, scratch);
    }

    if (persist->pending > 0)
        LOG_WARN_LIMIT(1, 1.0f, "Contact persistence: %d expired pairs are waiting for output space.\n", persist->pending);
    if (ContactPersist_TakeAllocFailureReport(persist) || SleepWake_TakeAllocFailureReport(scratch))
        LOG_ERROR("Contact persistence / wake lists allocation failed. Some wake events are delayed this tick.\n");
}

// SOLVE half of the frame. Order: sleepers made static, sort bottom-up, warm start, K velocity iterations,
// position iterations, per-body stats, write-back, and the warm table for next frame. No geometry tests
// and no gameplay callbacks happen here.
void SolveWorldContacts(World2d *world, float dt)
{
    (void)dt; // The cache holds the step set by BeginFrame; kept in the signature for the call site's clarity.
    ContactCache *cache = &world->contact_cache;

    if (cache->count > 0)
    {
        // Static sleepers: ResolveSleepWakes has already woken whatever must move, so a body that is STILL
        // asleep now is immovable for this solve (zero inverse mass and inertia, like the container). An awake
        // body resting on it is supported as by the floor and its weight never reaches the sleeper.
        const Newtonoid2d *entities = (const Newtonoid2d *)world->objects.items;
        int object_count = (int)world->objects.count;
        for (int slot = 1; slot < cache->body_count; slot++)
        {
            int index = cache->bodies[slot].object_index;
            if (index >= 0 && index < object_count && (entities[index].status_flags & ENTITY_STATUS_FLAG_SLEEPING))
                cache->body_stats[slot].flags |= BODY_STAT_STATIC;
        }
        ContactCache_MakeSleepersStatic(cache);
        ContactCache_RefreshEffectiveMass(cache);

        const Space2d *space = &world->grid_space.space;
        ContactCache_Sort(cache, space->grid_origin.y, space->grid_origin.y + (float)space->rows);
        ContactSolver_WarmStart(cache);
        ContactSolver_VelocityIterations(cache);
        ContactSolver_PositionIterations(cache);
        ContactSolver_AccumulateBodyStats(cache);
        ApplySolverResults(world);
    }
    ContactCache_StoreWarm(cache);

    if (cache->stats.dropped > 0)
        LOG_WARN_LIMIT(1, 1.0f, "Contact cache dropped %d contacts this frame (capacity or invalid input).\n", cache->stats.dropped);
    if (cache->stats.warm_dropped > 0)
        LOG_WARN_LIMIT(1, 1.0f, "Warm-start table overflow: %d contacts start cold next frame.\n", cache->stats.warm_dropped);
    if (ContactCache_TakeAllocFailureReport(cache))
        LOG_ERROR("Contact cache allocation failed. Some contacts are dropped or start cold this tick.\n");
}
