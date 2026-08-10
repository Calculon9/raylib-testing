#include "input/drag_interaction.h"

static DragInteractionState g_drag_contexts[DRAG_CONTEXT_COUNT] = {0};

DragInteractionState *DragInteraction_GetContext(DragContextId id)
{
    if (id < 0 || id >= DRAG_CONTEXT_COUNT)
    {
        return NULL;
    }

    return &g_drag_contexts[id];
}

void DragInteraction_Reset(DragInteractionState *state)
{
    if (!state)
    {
        return;
    }

    ResetPointerState(&state->pointer_state);

    state->target_kind = DRAG_TARGET_NONE;
    state->target = NULL;
    state->target_anchor = ZERO_VECTOR_2D;
    state->has_capture = false;
}

void DragInteraction_ResetContext(DragContextId id)
{
    DragInteractionState *state = DragInteraction_GetContext(id);
    DragInteraction_Reset(state);
}

void DragInteraction_BeginFrame(DragInteractionState *state, const InputFrame *input)
{
    if (!state || !input)
    {
        return;
    }

    if (input->left_pressed)
    {
        DragInteraction_Reset(state);
    }

    if (input->left_down)
    {
        DragInteraction_UpdateButtonDown(state, input->pointer_position);
    }
}

void DragInteraction_EndFrame(DragInteractionState *state, const InputFrame *input)
{
    if (!state || !input || !input->left_released)
    {
        return;
    }

    DragInteraction_UpdateButtonUp(state);
}

void DragInteraction_UpdateButtonDown(DragInteractionState *state, Vector2d pointer_pos)
{
    if (!state)
    {
        return;
    }

    UpdatePointerState(POINTER_BUTTON_LEFT, &state->pointer_state, pointer_pos);
}

void DragInteraction_UpdateButtonUp(DragInteractionState *state)
{
    DragInteraction_Reset(state);
}

void DragInteraction_BeginCapture(DragInteractionState *state, DragTargetKind kind, void *target, Vector2d anchor)
{
    if (!state)
    {
        return;
    }

    state->target_kind = kind;
    state->target = target;
    state->target_anchor = anchor;
    state->has_capture = (target != NULL && kind != DRAG_TARGET_NONE);
}

void DragInteraction_ClearCapture(DragInteractionState *state)
{
    if (!state)
    {
        return;
    }

    state->target_kind = DRAG_TARGET_NONE;
    state->target = NULL;
    state->target_anchor = ZERO_VECTOR_2D;
    state->has_capture = false;
}

bool DragInteraction_IsClick(const DragInteractionState *state, int max_hold_ticks, float max_travel_pixels)
{
    if (!state)
    {
        return false;
    }

    return Pointer_IsClick(state->pointer_state, max_hold_ticks, max_travel_pixels);
}

bool DragInteraction_IsDragActive(const DragInteractionState *state, float min_travel_pixels)
{
    if (!state)
    {
        return false;
    }

    return Pointer_IsDrag(state->pointer_state, min_travel_pixels);
}

Vector2d DragInteraction_GetPointerDelta(const DragInteractionState *state)
{
    if (!state)
    {
        return ZERO_VECTOR_2D;
    }

    return Pointer_GetTravelDelta(state->pointer_state);
}
