#ifndef STATE_MANAGER_SYSTEM_H
#define STATE_MANAGER_SYSTEM_H

#include "system/view_host_system.h"
#include "camera/camera.h"
#include "ui/ui.h"

void InitStateManagerSystem(void);
// Destroy the state-manager panel and clear its cached UI references.
void DestroyStateManagerSystem(void);
void DrawStateManagerSystem(void);
UIElement *GetStateManagerRoot(void);
void MarkStateManagerRefreshDirty(void);
void UpdateStateManagerSelectedObject(void);

// Debug-basis hooks for the panel's coordinate space (used by the diagnostics basis editor).
Frame2d *GetStateManagerSpaceFrame(void);
bool SetStateManagerSpaceBasis(Vector2d basis_u, Vector2d basis_v);
void ResetStateManagerSpaceBasis(void);

#endif
