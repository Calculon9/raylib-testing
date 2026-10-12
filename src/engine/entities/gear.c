#include "entities/gear.h"
#include "entities/entity_internal.h"
#include "entities/entity_registry.h"

// Validate the tooth count required to generate a gear surface.
static bool ValidateGearParams(const Newtonoid2dParams *params)
{
    if (!Entity_ValidateDimensions(params))
    {
        return false;
    }

    // Gear geometry (CreateVertices_Gear) emits points_per_tooth=3 vertices per tooth, so the
    // base body's vertice_count is the TOOTH count and must match that function's accepted
    // range: at least GEAR_MIN_TEETH, at most MAX_SHAPE_VERTICES/3 (the vertex-budget cap).
    if (params->vertice_count < GEAR_MIN_TEETH ||
        params->vertice_count > MAX_SHAPE_VERTICES / GEAR_POINTS_PER_TOOTH)
    {
        LOG_WARN("Invalid gear tooth count: tooth_count = %d (expected %d..%d)\n",
                 params->vertice_count, GEAR_MIN_TEETH, MAX_SHAPE_VERTICES / GEAR_POINTS_PER_TOOTH);
        return false;
    }

    return true;
}

// Create a gear entity and leave world registration to the caller.
Newtonoid2d *GearEntity_Create(const Newtonoid2dParams *params)
{
    if (!ValidateGearParams(params))
    {
        return NULL;
    }

    Surface2d surface = {0};
    surface.surface_vectors = CreateVertices_Gear(
        params->vertice_count, params->width * 0.5f, params->height * 0.5f);
    Newtonoid2d *entity = CreateNewtonoid2d_Allocated(
        params->mass, params->anchor_position, params->velocity,
        params->acceleration, surface);
    if (!entity)
    {
        return NULL;
    }

    entity->shape_type = SHAPE_GEAR;
    EntityLifecycle_ApplyAttachedComponent(entity, ENTITY_COMPONENT_GEAR, NULL);
    Newtonoid_ConfigureRestitution(entity, params->restitution);
    Newtonoid_ConfigureFriction(entity, params->friction);
    return entity;
}

// Initialise gear component state from prefab-configurable values. Seeds the authored
// meshing identity, derives the pitch radius, and clears the runtime engagement state
// (partners/sign) that GearSystem_Update maintains. Mirrors PortalComponent_Initialise.
bool GearComponent_Initialise(GearComponent *component, const GearComponentParams *params)
{
    // Reject invalid meshing identity: a gear needs positive teeth and module to form ratios.
    if (!component || !params || params->tooth_count <= 0 || params->module <= 0.0f)
    {
        return false;
    }

    *component = (GearComponent){
        .tooth_count = params->tooth_count,
        .module = params->module,
        .is_driver = params->is_driver,
        .drive_angular_velocity = params->drive_angular_velocity,
        // pitch_radius = module * tooth_count / 2 (standard gear geometry).
        .pitch_radius = params->module * (float)params->tooth_count * 0.5f,
        // Runtime engagement state starts empty; the gear system fills it each frame.
        .rotation_sign = 1,
        .mesh_partner_count = 0,
        .mesh_partners = {0}};
    return true;
}
