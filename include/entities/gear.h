#ifndef GEAR_ENTITY_H
#define GEAR_ENTITY_H

#include "entities/entity_id.h"
#include "physics/newtonoid.h"

/**********************************************************************************************
 *
 * GEAR COMPONENT
 *
 * Game-logic state for gear entities. A gear is a toothed rotating body that MESHES with
 * neighbouring gears and propagates rotation through the resulting gear train at a ratio
 * set by the engaged gears' tooth counts. Continuous rotation itself is stored in the base
 * body's Newtonoid2d.angular_velocity; this component holds the meshing/ratio state that
 * GearSystem_Update reads to detect engagement and synchronise driven gears.
 *
 * NOTE: tooth_count is the MESHING tooth count (gear-ratio maths), which is distinct from
 * the base body's vertice_count (the number of visual facets CreateVertices_Gear draws).
 *
 **********************************************************************************************/

// Maximum number of simultaneous mesh partners tracked per gear. A fixed cap keeps the
// component representable in packed component stores (no per-gear allocation). A planetary
// hub rarely engages more than a handful of neighbours; raise if a use case needs more.
#define GEAR_MAX_MESH_PARTNERS 6

// Component data for gear entities: authored meshing identity + runtime engagement state.
typedef struct GearComponent
{
    // --- Authored meshing identity (seeded from GearComponentParams) ---
    int tooth_count;              // Number of meshing teeth; drives gear ratios (ratio = t_a / t_b).
    float module;                 // Gear module (tooth pitch). Two gears mesh only when modules match.
    bool is_driver;               // true: torque source (externally driven); false: driven by meshing.
    float drive_angular_velocity; // Driver spin rate in rad/s; seeds the base body for drivers.

    // --- Derived cache ---
    float pitch_radius;           // module * tooth_count / 2. Centre distance for meshing detection.

    // --- Runtime engagement state (maintained by GearSystem_Update) ---
    int rotation_sign;            // +1 / -1 handedness propagated along the train (meshed externals oppose).
    int mesh_partner_count;       // Number of valid entries in mesh_partners.
    EntityId mesh_partners[GEAR_MAX_MESH_PARTNERS]; // Currently engaged neighbour gears.
} GearComponent;

// Configurable gear component values supplied by a prefab or caller. Only the authored
// meshing identity is specified here; all runtime engagement state is derived at creation
// (pitch_radius) or maintained by the gear system (partners, sign).
typedef struct GearComponentParams
{
    int tooth_count;              // Meshing teeth; must be > 0.
    float module;                 // Gear module (tooth pitch); must be > 0.
    bool is_driver;               // Whether this gear is a torque source.
    float drive_angular_velocity; // Initial driver spin rate (rad/s); 0 for driven gears.
} GearComponentParams;

// Create an allocated gear entity from the supplied creation parameters.
Newtonoid2d *GearEntity_Create(const Newtonoid2dParams *params);

// Initialise runtime gear component state from prefab-configurable values. Returns false on
// invalid params (non-positive tooth_count or module). Mirrors PortalComponent_Initialise.
bool GearComponent_Initialise(GearComponent *component, const GearComponentParams *params);

#endif
