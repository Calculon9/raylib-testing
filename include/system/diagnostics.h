/**********************************************************************************************
*
*   DIAGNOSTICS MODULE
*
*   Genuine developer diagnostics extracted from the debug-overlay facade: the full-screen
*   debug dashboard and the viewport-space basis editor (plus the shared basis-target
*   machinery they both use). State lives here; debug_overlay_system remains a thin toggle
*   facade and delegates its DEBUG_DASHBOARD case and its dashboard/basis-editor entry points
*   to this module, so existing command/UI/hotkey entry points keep working unchanged.
*
*   This module is a CONSUMER of the debug-overlay facade: it calls IsDebugEnabled() to render
*   the dashboard's ON/OFF rows. It never calls back into facade toggling.
*
**********************************************************************************************/
#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>

// --- Dashboard toggle/query (DEBUG_DASHBOARD delegates here) ---

// Flip the debug dashboard's enabled state.
void Diagnostics_ToggleDashboard(void);

// Return true when the debug dashboard is enabled, false otherwise.
bool Diagnostics_IsDashboardEnabled(void);

// --- Dashboard rendering + snapshot ---

// Draw the full-screen debug dashboard. No-op responsibility stays with the caller: the
// facade only calls this when the dashboard is enabled.
void Diagnostics_DrawDashboard(void);

// Recompute the universe diagnostics snapshot for the current frame. When the dashboard is
// disabled the snapshot is simply invalidated (matches the previous early-out behaviour).
void Diagnostics_UpdateSnapshot(void);

// --- Basis editor ---

// Process basis-editor hotkeys (F5 toggle, TAB/U/V/I/J/K/L/O/P/BACKSPACE). Returns true when
// the active basis target changed this frame, so the caller can refresh dependent systems.
bool Diagnostics_HandleBasisEditorHotkeys(void);

// Return non-zero when the active basis-editor target is universe space, zero otherwise.
// The facade uses this to route the post-edit refresh: universe-space edits refresh the camera
// (UpdateCameraFull), any other target refreshes the viewport + UI (RefreshViewportForBasisEdit).
// Exposed as an int (a boolean 0/1 predicate) so DebugBasisTargetId stays private to
// diagnostics.c; the implementation is `return basis_editor_target == DEBUG_BASIS_TARGET_UNIVERSE_SPACE;`.
int Diagnostics_ActiveBasisTargetIsUniverseSpace(void);

#endif // !DIAGNOSTICS_H
