/**********************************************************************************************
*
*   OBJECT GIZMOS MODULE
*
*   Toggleable inspection overlays drawn onto world objects: orientation axes, collision hull,
*   and axis-aligned bounding box. These are first-class, editor-grade entity annotations owned
*   by the world layer (consumed by world_renderer.c), NOT developer diagnostics. State lives
*   here; the debug-overlay facade delegates its DEBUG_OBJECT_* cases to this module so existing
*   command/UI/hotkey entry points keep working unchanged.
*
**********************************************************************************************/
#ifndef OBJECT_GIZMOS_H
#define OBJECT_GIZMOS_H

#include <stdbool.h>

// Identifies a single object-inspection gizmo.
typedef enum GizmoId
{
    GIZMO_AXES = 0, // Per-object orientation axes.
    GIZMO_HULL,     // Per-object collision hull outline.
    GIZMO_AABB,     // Per-object axis-aligned bounding box.
    GIZMO_COUNT
} GizmoId;

// Flip a gizmo's enabled state. Unknown ids are ignored.
void ToggleGizmo(GizmoId gizmo_id);

// Return non-zero when the gizmo is enabled, 0 otherwise (including unknown ids).
int IsGizmoEnabled(GizmoId gizmo_id);

#endif // !OBJECT_GIZMOS_H
