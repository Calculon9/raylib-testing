/**********************************************************************************************
*
DRAG INTERACTION MODULE
*
**********************************************************************************************/
#ifndef DRAG_INTERACTION_H
#define DRAG_INTERACTION_H

#include "input/pointer_input.h"

#define INPUT_DRAG_THRESHOLD_PIXELS 5.0f
#define INPUT_CLICK_MAX_HOLD_TICKS 20

typedef enum
{
    INPUT_ROUTE_IGNORED = 0,
    INPUT_ROUTE_BLOCKED,
    INPUT_ROUTE_HANDLED,
    INPUT_ROUTE_CAPTURED
} InputRouteResult;

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
    DRAG_TARGET_WORLD_CONTAINER = 3,
    DRAG_TARGET_VERTEX_HANDLE = 4
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
DragInteractionState *DragInteraction_BeginContextFrame(DragContextId id, const InputFrame *input);
void DragInteraction_EndContextFrame(DragContextId id, const InputFrame *input);
void DragInteraction_Reset(DragInteractionState *state);
void DragInteraction_ResetContext(DragContextId id);
void DragInteraction_BeginFrame(DragInteractionState *state, const InputFrame *input);
void DragInteraction_EndFrame(DragInteractionState *state, const InputFrame *input);
void DragInteraction_UpdateButtonDown(DragInteractionState *state, Vector2d pointer_pos);
void DragInteraction_UpdateButtonUp(DragInteractionState *state);
void DragInteraction_BeginCapture(DragInteractionState *state, DragTargetKind kind, void *target, Vector2d anchor);
void DragInteraction_ClearCapture(DragInteractionState *state);
bool DragInteraction_IsClick(const DragInteractionState *state, int max_hold_ticks, float max_travel_pixels);
bool DragInteraction_IsDragActive(const DragInteractionState *state, float min_travel_pixels);
Vector2d DragInteraction_GetPointerDelta(const DragInteractionState *state);
void DragInteraction_ChangeAnchor(DragInteractionState *state, Vector2d new_anchor);

#endif
