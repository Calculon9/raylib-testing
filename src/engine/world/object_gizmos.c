/**********************************************************************************************
*
*   OBJECT GIZMOS MODULE - implementation
*
*   Owns the enabled-state of the per-object inspection overlays (axes / hull / AABB). Moved out
*   of the debug-overlay system so these entity annotations live with the world layer that draws
*   them. Defaults preserve the previous behaviour: all gizmos start disabled.
*
**********************************************************************************************/
#include "world/object_gizmos.h"

// Per-gizmo enabled flags. All default false, matching the previous debug-overlay defaults for
// object_axes/hull/aabb.
static bool axes_gizmo_enabled = false;
static bool hull_gizmo_enabled = false;
static bool aabb_gizmo_enabled = false;

// Flip a gizmo's enabled state. Unknown ids are ignored.
void ToggleGizmo(GizmoId gizmo_id)
{
    switch (gizmo_id)
    {
    case GIZMO_AXES:
        axes_gizmo_enabled = !axes_gizmo_enabled;
        break;
    case GIZMO_HULL:
        hull_gizmo_enabled = !hull_gizmo_enabled;
        break;
    case GIZMO_AABB:
        aabb_gizmo_enabled = !aabb_gizmo_enabled;
        break;
    default:
        break;
    }
}

// Return non-zero when the gizmo is enabled, 0 otherwise (including unknown ids).
int IsGizmoEnabled(GizmoId gizmo_id)
{
    switch (gizmo_id)
    {
    case GIZMO_AXES:
        return axes_gizmo_enabled;
    case GIZMO_HULL:
        return hull_gizmo_enabled;
    case GIZMO_AABB:
        return aabb_gizmo_enabled;
    default:
        return 0;
    }
}
