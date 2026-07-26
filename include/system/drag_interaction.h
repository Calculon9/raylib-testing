/**********************************************************************************************
*
DRAG INTERACTION MODULE
*
**********************************************************************************************/
#ifndef DRAG_INTERACTION_H
#define DRAG_INTERACTION_H

#include "common/common.h"
#include "system/utility_system.h"

typedef enum
{
    DRAG_CONTEXT_UI = 0,
    DRAG_CONTEXT_GAME = 1,
    DRAG_CONTEXT_COUNT
} DragContextId;

typedef enum
{
    DRAG_TARGET_NONE = 0,
    DRAG_TARGET_UI_ELEMENT = 1,
    DRAG_TARGET_WORLD_ENTITY = 2,
    DRAG_TARGET_WORLD_CONTAINER = 3
} DragTargetKind;

typedef struct
{
    PointerState pointer_state;
    DragTargetKind target_kind;
    void *target;
    Vector2d target_anchor;
    bool has_capture;
} DragInteractionState;

DragInteractionState *DragInteraction_GetContext(DragContextId id);
void DragInteraction_Reset(DragInteractionState *state);
void DragInteraction_ResetContext(DragContextId id);
void DragInteraction_UpdateButtonDown(DragInteractionState *state, Vector2d pointer_pos);
void DragInteraction_UpdateButtonUp(DragInteractionState *state);
void DragInteraction_BeginCapture(DragInteractionState *state, DragTargetKind kind, void *target, Vector2d anchor);
void DragInteraction_ClearCapture(DragInteractionState *state);
bool DragInteraction_IsClick(const DragInteractionState *state, int max_hold_ticks, float max_travel_pixels);
bool DragInteraction_IsDragActive(const DragInteractionState *state, float min_travel_pixels);
Vector2d DragInteraction_GetPointerDelta(const DragInteractionState *state);

#endif
