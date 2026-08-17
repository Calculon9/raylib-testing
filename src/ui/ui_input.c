/**********************************************************************************************
 *
 *   raylib - Advance Game template
 *
 **********************************************************************************************/
#include "raylib.h"
#include <stdint.h>
#include "math/cvectors.h"
#include "camera/camera.h"
#include "ui/ui_input.h"
#include "ui/text_region.h"
#include "system/ui_system.h"
#include "system/ui/lpanel_system.h"
#include "system/ui/utility_panel_system.h"
#include "system/ui/state_manager_system.h"
#include "system/viewport_system.h"
#include "system/utility_system.h"
#include <stdlib.h>
#include "world/world.h"
#include "system/ui/rpanel_system.h"
#include "system/ui/popup_menu.h"
#include "world/universe.h"
#include "system/command_queue.h"
#include "input/drag_interaction.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
// Legacy UI drag mirror retained for reference; DragInteractionState is authoritative.
// DragState G_DragState = {0};
// UIElement *G_DraggedElement = NULL;
// Vector2d G_DragOffset = ZERO_VECTOR_2D;
Text_64_IOState tbox_io_buffers = {0};
// MouseDownState mouse_down_state = {0};
// char tbox_input_buffer[sizeof(G_UIState.focused_element->data.textbox.text.string)] = ""; // Stores textbox input
// char tbox_temp_buffer[sizeof(G_UIState.focused_element->data.textbox.text.string)] = "";  // Stores the original text being output to a focused textbox before it was modified by the user - this buffer is used to restore it if needed

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void HandleMouseEvents(UIElement *root_element, const InputFrame *input);
void HandleLeftMouseDown(UIElement *element, Vector2d mouse_coords);
void HandleLeftMouseUp(UIElement *target, Vector2d mouse_coords);
void HandleTextBoxClick(UIElement *clicked);
void HandleBtnClick(UIElement *target);
void HandleHoverItem(UIElement *target);
static UIElement *hovered_item = NULL;
void HandleUIDragging(UIElement *e, Vector2d mouse_coords);
//void HandleFocusSwitch(UIElement *curr_focus, UIElement *prev_focus);
void HandleTextCommit(UIElement *element, Text_64_IOState *tbox_buffers);
// Vector2d CalculateDragOffset(UIElement *target, Vector2d mouse_coords);
void UpdateTextIO(void);
void RevertTextChanges(UIElement *tbox, Text_64_IOState *t_bufs);
// bool IsMouseDragged(MouseDownState mouse_down_state);
// bool IsMouseClicked(MouseDownState mouse_down_state);
void InitTextBuffers(UIElement *e, Text_64_IOState *t_bufs);
UIElement *ResolveUIRootTarget(Vector2d mouse_coords);
Vector2d ResolvePixelToLocalDragScale(UIElement *element);
bool ResolvePointerLocalCoords(UIElement *element, Vector2d mouse_pixel_coords, Vector2d *out_local_coords);
void UpdateUIFocus(UIElement *element);
void UpdateDragFocus(UIElement *element, Vector2d mouse_coords);
void ClearUIFocus(void);
// void ResetDragState(void);

static DragInteractionState *GetUIDragContext(void)
{
    return DragInteraction_GetContext(DRAG_CONTEXT_UI);
}

static void ResetTextBuffers(Text_64_IOState *tbox_buffers)
{
    if (!tbox_buffers)
    {
        return;
    }

    tbox_buffers->input_buffer.string[0] = '\0';
    tbox_buffers->temp_buffer.string[0] = '\0';
}

static void SnapshotTextBuffers(Text_64_IOState *tbox_buffers, const char *text)
{
    if (!tbox_buffers || !text)
    {
        return;
    }

    safe_strncpy(tbox_buffers->temp_buffer.string, text, MAX_TEXTBOX_CHARS);
    tbox_buffers->input_buffer.string[0] = '\0';
}

static bool HasPendingTextEdit(const Text_64_IOState *tbox_buffers)
{
    return tbox_buffers && tbox_buffers->input_buffer.string[0] != '\0';
}

static bool IsInteractiveTextbox(const UIElement *element)
{
    return element &&
           (element->type == UI_ELEMENT_TEXTBOX_IO ||
            element->type == UI_ELEMENT_TEXTBOX_SAFE_IO);
}

static void ClearTextFocus(Text_64_IOState *tbox_buffers)
{
    ResetTextBuffers(tbox_buffers);
    ClearUIFocus();
}

static UIElement *FindOwningRoot(UIElement *element)
{
    UIElement *current = element;
    while (current)
    {
        if (current->type == UI_ELEMENT_ROOT)
        {
            return current;
        }
        current = current->parent;
    }

    return NULL;
}

Vector2d ResolvePixelToLocalDragScale(UIElement *element)
{
    UIElement *root = FindOwningRoot(element);
    if (!root)
    {
        return (Vector2d){1.0f, 1.0f};
    }

    Vector2d local_size = {(float)root->data.root.space.columns, (float)root->data.root.space.rows};
    Vector2d pixel_size = root->screen_box.dimensions;

    Vector2d scale = {1.0f, 1.0f};
    if (local_size.x > 0.0f && pixel_size.x > 0.0f)
    {
        scale.x = local_size.x / pixel_size.x;
    }
    if (local_size.y > 0.0f && pixel_size.y > 0.0f)
    {
        scale.y = local_size.y / pixel_size.y;
    }

    frame_counter.total_frames % 1800 == 0 ? LOG_INFO("Drag scale for element [%s]: (%.2f, %.2f)", GetElementTypeName(element->type), scale.x, scale.y) : (void)0;

    return scale;
}

bool ResolvePointerLocalCoords(UIElement *element, Vector2d mouse_pixel_coords, Vector2d *out_local_coords)
{
    if (!element || !out_local_coords)
    {
        return false;
    }

    UIElement *root = FindOwningRoot(element);
    if (!root)
    {
        return false;
    }

    Vector2d pixels_to_local_scale = ResolvePixelToLocalDragScale(root);
    Vector2d root_local_origin = root->data.root.space.frame.origin_in_parent;
    Vector2d root_pixel_origin = root->screen_box.coords;
    Vector2d pixel_delta = VectorSum_2d(mouse_pixel_coords, (Vector2d){-root_pixel_origin.x, -root_pixel_origin.y});

    out_local_coords->x = root_local_origin.x + (pixel_delta.x * pixels_to_local_scale.x);
    out_local_coords->y = root_local_origin.y + (pixel_delta.y * pixels_to_local_scale.y);
    return true;
}
// DISPATCHER - UI activities
void ProcessUIInput(const InputFrame *input, bool cursor_in_region)
{
    if (!input)
    {
        return;
    }

    Vector2d mouse_coords = input->pointer_position;
    if (!cursor_in_region)
    {
        DragInteractionState *drag_ctx = GetUIDragContext();
        if ((input->left_down && drag_ctx->has_capture) ||
            (input->left_released && drag_ctx->pointer_state.left_button_hold_ticks > 0))
        {
            HandleMouseEvents(NULL, input);
        }
        return;
    }

    // If focus was disabled externally, clear it so stale input cannot flow through.
    if (G_UIState.focused_element && !G_UIState.focused_element->is_enabled)
    {
        RevertTextChanges(G_UIState.focused_element, &tbox_io_buffers);
        ClearTextFocus(&tbox_io_buffers);
    }

    UIElement *target = ResolveUIRootTarget(mouse_coords);
    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    HandleHoverItem(target);
    HandleMouseEvents(target, input);
    UpdateTextIO();
}

void HandleMouseEvents(UIElement *target, const InputFrame *input)
{
    if (!input)
    {
        return;
    }

    Vector2d mouse_coords = input->pointer_position;
    // -----HANDLE LEFT MOUSE-----
    // Phase A: Handle the Start/New Events
    // Update the shared UI drag context.
    if (input->left_down)
    {
        // Handle it
        HandleLeftMouseDown(target, mouse_coords);
    }
    else if (input->left_released)
    {
        HandleLeftMouseUp(target, mouse_coords);
    }
}

// HANDLER - MOUSE DOWN
void HandleLeftMouseDown(UIElement *target, Vector2d mouse_coords)
{
    DragInteractionState *drag_ctx = GetUIDragContext();

    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    // EVENT PHASE: Check if focus shifted (Only runs on the *initial* press frame)
    if (drag_ctx->pointer_state.left_button_hold_ticks == 1)
    {
        if (target != G_UIState.focused_element)
        {
            // Commit pending textbox edits when focus changes; if commit fails,
            // HandleTextCommit reverts using the snapshot buffer.
            if (G_UIState.focused_element && IsTextbox(G_UIState.focused_element) && HasPendingTextEdit(&tbox_io_buffers))
            {
                HandleTextCommit(G_UIState.focused_element, &tbox_io_buffers);
            }
            else
            {
                RevertTextChanges(G_UIState.focused_element, &tbox_io_buffers);
            }

            // Initialise new focus and new state
            UpdateUIFocus(target);
            InitTextBuffers(target, &tbox_io_buffers);
            // Do NOT return here anymore; let the code cascade into starting the drag!
        }

        // EVENT PHASE: Initialize Dragging if applicable
        if (target && !drag_ctx->has_capture) // && target->is_draggable ) //Assume everything is draggable for now
        {
            UpdateDragFocus(target, mouse_coords);
        }
    }

    // STATE PHASE: Process Ongoing Drag (Runs every frame the mouse is held down, including frame 1)
    if (drag_ctx->has_capture && drag_ctx->target_kind == DRAG_TARGET_UI_ELEMENT)
    {
        // Use threshold check to see if the user has dragged past the deadzone
        if (DragInteraction_IsDragActive(drag_ctx, INPUT_DRAG_THRESHOLD_PIXELS))
        {
            UIElement *dragged_element = (UIElement *)drag_ctx->target;
            if (dragged_element && dragged_element->is_draggable)
            {
                // CRITICAL: Always drag the lock-on target, NOT the element currently under the hover cursor!
                HandleUIDragging(dragged_element, mouse_coords);
            }
        }
    }

    // Debug logging
    if (frame_counter.total_frames % 180 == 0)
    {
        Vector2d drag_delta = DragInteraction_GetPointerDelta(drag_ctx);
        printf("MOUSE DOWN [%s] | DRAG_DELTA (%0.0f,%0.0f)\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), drag_delta.x, drag_delta.y);
    }
}

// HANDLER - MOUSE DOWN
void HandleLeftMouseUp(UIElement *target, Vector2d mouse_coords)
{
    DragInteractionState *drag_ctx = GetUIDragContext();

    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    // -----CHECK IF IT WAS A CLICK (e.g. MOUSE DOWN FOR <20 frames)-----
    if (drag_ctx->pointer_state.left_button_hold_ticks > 0) // Only need to do something if the mouse was down, i.e. there was either a drag, or click
    {
        if (DragInteraction_IsClick(drag_ctx, INPUT_CLICK_MAX_HOLD_TICKS, INPUT_DRAG_THRESHOLD_PIXELS))
        {
            Vector2d local_coords = ZERO_VECTOR_2D;
            bool has_local_coords = ResolvePointerLocalCoords(target, mouse_coords, &local_coords);

            if (has_local_coords)
            {
                printf("CLICKED [%s] | PIXEL (%.0f, %.0f) | LOCAL (%.2f, %.2f)\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), mouse_coords.x, mouse_coords.y, local_coords.x, local_coords.y);
            }
            else
            {
                printf("CLICKED [%s] | PIXEL (%.0f, %.0f)\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), mouse_coords.x, mouse_coords.y);
            }

            // Handle Interaction
            if (target && IsTextbox(target))
            {
                HandleTextBoxClick(target);
            }
            else if (target && IsBtn(target))
            {
                HandleBtnClick(target);
            }
        }

        printf("ENDED MOUSE DOWN [%s]\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE));
    }
}

void HandleBtnClick(UIElement *target)
{
    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    // -----CHECK IF IT WAS A CLICK (e.g. MOUSE DOWN FOR <20 frames)-----
    if (target && IsBtn(target))
    {
        if (target->data.button.on_click != NULL)
        {
            target->data.button.on_click(target);
        }
    }
}

void HandleHoverItem(UIElement *target)
{
    if (hovered_item != target && hovered_item &&
        hovered_item->type == UI_ELEMENT_HOVER_ITEM)
    {
        hovered_item->colour_fill = hovered_item->data.hover_item.normal_fill;
    }

    hovered_item = NULL;
    if (target && target->is_enabled && target->type == UI_ELEMENT_HOVER_ITEM)
    {
        hovered_item = target;
        target->colour_fill = target->data.hover_item.hover_fill;
        if (target->data.hover_item.on_hover)
        {
            target->data.hover_item.on_hover(target);
        }
    }
}

void HandleBtnSwitchClick(UIElement *btn)
{
    if (btn && !btn->is_enabled)
    {
        return;
    }
    ToggleElementEnabled(btn->data.button.data_bind); // Toggle the slave's enabled state
    LOG_INFO("SWITCH CLICK\n");
}

// A LArray of UIElements needs
void HandleBtnEnumerateClick(UIElement *btn)
{
    if (!btn || !btn->is_enabled)
        return;

    LArray *a = btn->data.button.data_bind;

    // Safely extract our custom integer tracker from the void*
    int *current_index = (int *)btn->data.button.user_data;
    if (!current_index || !a || a->count == 0)
        return;

    // Calculate next step using the heap integer
    int next_index = (*current_index + 1) % a->count;
    View *curr_view = *((View **)LArray_Get(a, *current_index));
    View *next_view = *((View **)LArray_Get(a, next_index));

    // UIElement *current_item = *(UIElement **)LArray_Get(a, *current_index);
    // UIElement *next_item = *(UIElement **)LArray_Get(a, next_index);

    if (curr_view)
        ToggleElementEnabled(curr_view->container);
    if (next_view)
        ToggleElementEnabled(next_view->container);

    // Save the state back directly to that memory address
    *current_index = next_index;

    // Update the Active View in the global state if this button is associated with a view switch
    G_UIState.active_panel_view = next_view->type;

    LOG_INFO("ENUMERATE VIEW CLICK\n");
}

void HandleBtnSimpleClick(UIElement *btn)
{
    if (btn && !btn->is_enabled)
    {
        return;
    }

    // ToggleElementEnabled(btn->data.button.slave); // Toggle the slave's enabled state
    // LOG_INFO("SWITCH CLICK\n");
}

void HandleBtnSubmitClick(UIElement *btn)
{
    if (!btn || !btn->is_enabled)
        return;

    int action = BUTTON_ACTION_NONE;
    if (btn->data.button.user_data)
        action = *(int *)(btn->data.button.user_data);

    if (action == BUTTON_ACTION_CREATE_ENTITY)
    {
        if (!Universe_GetSelectedWorld(&G_Universe))
            return;

        // Enqueue a create-entity command instead of creating immediately
        if (G_UIState.newtonoid_params)
        {
            EnqueueCreateEntity(G_UIState.newtonoid_params);
        }
    } else if (action == BUTTON_ACTION_DELETE_ENTITY)
    {
        if (!Universe_GetSelectedWorld(&G_Universe))
            return;

        // Enqueue a delete-entity command instead of deleting immediately
        EntityId selected_object_id = UIState_GetSelectedObjectId();
        if (selected_object_id != INVALID_ENTITY_ID)
        {
            EnqueueDeleteEntity(selected_object_id);
        }
    }
    else if (action == BUTTON_ACTION_CREATE_WORLD)
    {
        EnqueueCreateWorld();
    }
    else if (action == BUTTON_ACTION_SELECT_WORLD_PREV)
    {
        EnqueueSelectWorld(-1);
    }
    else if (action == BUTTON_ACTION_SELECT_WORLD_NEXT)
    {
        EnqueueSelectWorld(1);
    }
}

void HandleUIDragging(UIElement *e, Vector2d mouse_coords)
{
    if (!e || !e->is_enabled)
        return;

    // Convert screen-pixel drag delta into UI logical units using the dragged
    // element's owning root, so drag math is panel-agnostic.
    Vector2d pixels_to_ui_local_scale = ResolvePixelToLocalDragScale(e);

    // Calculate the raw mouse travel distance since the click frame
    DragInteractionState *drag_ctx = GetUIDragContext();
    Vector2d mouse_down_origin = drag_ctx->pointer_state.initial_pos;
    Vector2d total_pixel_travel = VectorSum_2d(drag_ctx->pointer_state.current_pos, (Vector2d){-mouse_down_origin.x, -mouse_down_origin.y});

    // Convert total pixel travel into total virtual unit travel
    Vector2d total_local_travel = (Vector2d){total_pixel_travel.x * pixels_to_ui_local_scale.x, total_pixel_travel.y * pixels_to_ui_local_scale.y};

    // Absolute Target Calculation
    Vector2d new_local_offset;
    new_local_offset.x = drag_ctx->target_anchor.x + total_local_travel.x;
    new_local_offset.y = drag_ctx->target_anchor.y + total_local_travel.y;

    // Compute real pixel offsets for accurate telemetry tracking
    Vector2d new_pixel_offset = (Vector2d){new_local_offset.x / pixels_to_ui_local_scale.x, new_local_offset.y / pixels_to_ui_local_scale.y};
    Vector2d diff_local = VectorSum_2d(new_local_offset, (Vector2d){-e->authored_offset.offset.x, -e->authored_offset.offset.y});
    Vector2d diff_pixel = (Vector2d){diff_local.x / pixels_to_ui_local_scale.x, diff_local.y / pixels_to_ui_local_scale.y};

    // Finalize properties assignment
    if (e->parent && e->resolved_offset.offset_mode == OFFSET_PERCENT)
    {
        e->resolved_offset.offset_mode = OFFSET_FIXED;
        e->authored_offset.offset_mode = OFFSET_FIXED;
    }

    e->resolved_offset.offset = new_local_offset;
    e->authored_offset.offset = new_local_offset;
    e->is_dirty = true;

    // Clean debug logging throttling
    if ((diff_pixel.x > 0 || diff_pixel.y > 0) && frame_counter.total_frames % 120 == 0) // (frame_counter.total_frames % 180 == 0)
    {
        printf("DRAGGED [%s] | DIFF (%0.2f, %0.2f)(%0.0f, %0.0f) | NewOffset (%0.2f, %0.2f)(%0.0f, %0.0f)\n", GetElementTypeName(e->type), diff_local.x, diff_local.y, diff_pixel.x, diff_pixel.y, new_local_offset.x, new_local_offset.y, new_pixel_offset.x, new_pixel_offset.y);
    }
}

// The issue is that when changing focus from a tbox to anything else, if no text is input then no pre-existing txt is written to temp buffer. Then in here, we write "nothing" back to it!!
void RevertTextChanges(UIElement *e, Text_64_IOState *tbox_buffers)
{
    if (!IsTextbox(e))
    {
        return;
    }
    safe_strncpy(e->data.textbox.text.string, tbox_buffers->temp_buffer.string, MAX_TEXTBOX_CHARS);

    // Reset tracking buffers
    ResetTextBuffers(tbox_buffers);
}

void HandleTextCommit(UIElement *element, Text_64_IOState *tbox_buffers)
{
    if (!element || !IsTextbox(element))
    {
        RevertTextChanges(element, tbox_buffers);
        return;
    }

    // Require a valid data binding target for commit.
    if (!element->data.textbox.data_bind && !element->data.textbox.binder)
    {
        RevertTextChanges(element, tbox_buffers);
        return;
    }

    // These TextBoxes (IO) are interactive so check it's an IO type before doing anything
    if (IsInteractiveTextbox(element))
    {
        // If a Binder is attached, use it (allows validation + conversion)
        if (element->data.textbox.binder)
        {
            bool ok = Binder_ValidateAndWrite(element->data.textbox.binder, element->data.textbox.text.string);
            if (!ok)
            {
                // Validation failed; revert changes
                RevertTextChanges(element, tbox_buffers);
                return;
            }
        }
        else
        {
            // Get the type of data the textbox is bound to
            DataType data_type = element->data.textbox.data_type;
            switch (data_type)
            {
            case VECTOR2D:
                PipelineTextToVector(element->data.textbox.text.string, element->data.textbox.data_bind); // Be sure the data type and data bind are compatible!
                break;
            case FLOAT:
                PipelineTextToFloat(element->data.textbox.text.string, element->data.textbox.data_bind); // Be sure the data type and data bind are compatible!
                break;
            case INT:
                PipelineTextToInt(element->data.textbox.text.string, (int *)element->data.textbox.data_bind);
                break;
            default:
                break;
            }
        }
    }

    // Clear buffers
    ResetTextBuffers(tbox_buffers);
}

// HANDLER - MOUSE CLICK
void HandleTextBoxClick(UIElement *clicked)
{
    if (!clicked || !clicked->is_enabled)
        return;

    // If we clicked the same element that already has focus, do nothing
    if (G_UIState.focused_element == clicked)
        return;

    // 2. Clear focus from the previous element (if any)
    // if (G_UIState.focused_element != NULL)
    // {
    //     G_UIState.focused_element->is_focused = false;
    //     // Optional: Trigger an "OnBlur" event here if needed
    // }

    // Assign focus to the new element
    // G_UIState.focused_element = clicked;
    // clicked->is_focused = true;

    // Move the cursor to the end of the text string
    // This allows the user to start typing immediately after what's already there
    if (IsInteractiveTextbox(clicked))
    {
        int length = strlen(clicked->data.textbox.text.string);
        clicked->data.textbox.cursor_position = length;
    }
}

void UpdateTextIO()
{
    if (G_UIState.focused_element && !G_UIState.focused_element->is_enabled)
    {
        RevertTextChanges(G_UIState.focused_element, &tbox_io_buffers);
        ClearTextFocus(&tbox_io_buffers);
        return;
    }

    if (!IsInteractiveTextbox(G_UIState.focused_element))
        return;

    char *output_buf = G_UIState.focused_element->data.textbox.text.string;

    // "Snapshot" for Undo/Cancel
    // If this is the very first time we are typing, save the original state
    if (!HasPendingTextEdit(&tbox_io_buffers) && tbox_io_buffers.temp_buffer.string[0] == '\0')
    {
        SnapshotTextBuffers(&tbox_io_buffers, output_buf);
    }

    // Handle Character Input
    int key = GetCharPressed();
    while (key > 0)
    {
        int current_len = strlen(output_buf);

        if (current_len < MAX_TEXTBOX_CHARS - 1 && (key >= 32 && key <= 125))
        {
            output_buf[current_len] = (char)key;
            output_buf[current_len + 1] = '\0';

            // Mirror to your tracking buffer so we know we've started "editing"
            tbox_io_buffers.input_buffer.string[0] = ' '; // Just a flag to say "not empty"
        }
        key = GetCharPressed();
    }

    // Handle Backspace
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(output_buf);
        if (len > 0)
            output_buf[len - 1] = '\0';
    }

    // Handle Escape (Cancel/Undo)
    if (IsKeyPressed(KEY_ESCAPE))
    {
        // Restore from backup
        safe_strncpy(output_buf, tbox_io_buffers.temp_buffer.string, MAX_TEXTBOX_CHARS);
        // Reset tracking buffers
        ClearTextFocus(&tbox_io_buffers);
    }

    // Handle Enter (Commit)
    if (IsKeyPressed(KEY_ENTER))
    {
        HandleTextCommit(G_UIState.focused_element, &tbox_io_buffers);
        ClearTextFocus(&tbox_io_buffers);
        // Trigger physics update here! (e.g., UpdateObjectMass())
    }
}

// Legacy UI drag predicates retained for reference; use DragInteraction_IsDragActive
// and DragInteraction_IsClick instead.
// bool IsMouseDragged(MouseDownState mouse_down_state)
// {
//     return IsPointerDrag(mouse_down_state, 5.0f);
// }
//
// bool IsMouseClicked(MouseDownState mouse_down_state)
// {
//     return IsPointerClick(mouse_down_state, 20, 5.0f);
// }

// ----------------------------------------------------------------------------------
// Utility Functions
void ClearUIFocus()
{
    if (G_UIState.focused_element)
    {
        G_UIState.focused_element->is_focused = false;
        G_UIState.focused_element = NULL;
    }
}

void UpdateUIFocus(UIElement *element)
{
    if (element && !element->is_enabled)
    {
        return;
    }

    if (G_UIState.focused_element)
    {
        G_UIState.focused_element->is_focused = false;
    }
    G_UIState.focused_element = element;
    if (G_UIState.focused_element)
    {
        G_UIState.focused_element->is_focused = true;
        printf("FOCUS CHANGE: [%s]\n", GetElementTypeName(element->type));
    }
}

void UpdateDragFocus(UIElement *element, Vector2d mouse_coords)
{
    DragInteractionState *drag_ctx = GetUIDragContext();

    if (!element || !element->is_enabled)
    {
        return;
    }

    // Drag math operates in fixed local units without changing layout until a drag starts.
    Vector2d initial_element_offset;
    if (element->parent && element->resolved_offset.offset_mode == OFFSET_PERCENT)
    {
        float content_area_w = fmaxf(0.0f, element->parent->local_box.dimensions.x - (element->parent->padding.x * 2.0f));
        float content_area_h = fmaxf(0.0f, element->parent->local_box.dimensions.y - (element->parent->padding.y * 2.0f));

        initial_element_offset = (Vector2d){
            content_area_w * element->authored_offset.offset.x,
            content_area_h * element->authored_offset.offset.y};
    }
    else
    {
        initial_element_offset = element->authored_offset.offset;
    }

    DragInteraction_BeginCapture(drag_ctx, DRAG_TARGET_UI_ELEMENT, element, initial_element_offset);
    printf("DRAG FOCUS CHANGE: [%s]\n", GetElementTypeName(element->type));
}

// Legacy reset path retained for reference; DragInteraction_UpdateButtonUp clears
// the pointer state and capture together.
// void ResetDragState(void)
// {
//     DragInteractionState *drag_ctx = GetUIDragContext();
//     DragInteraction_ClearCapture(drag_ctx);
//     printf("DRAG FOCUS CLEARED\n");
// }

void InitTextBuffers(UIElement *e, Text_64_IOState *t_bufs)
{
    ResetTextBuffers(t_bufs);

    if (IsTextbox(e))
    {
        SnapshotTextBuffers(t_bufs, e->data.textbox.text.string);
    }
}

UIElement *ResolveUIRootTarget(Vector2d mouse_coords)
{
    UIElement *roots[] = {
        GetPopupMenuRoot(),
        GetUtilityPanelRoot(),
        GetStateManagerRoot(),
        GetRPanelRoot(),
        GetLPanelRoot()};

    for (int i = 0; i < (int)(sizeof(roots) / sizeof(roots[0])); i++)
    {
        if (!roots[i])
        {
            continue;
        }

        UIElement *target = GetElementAt(roots[i], mouse_coords);
        if (target)
        {
            return target;
        }
    }

    return NULL;
}

