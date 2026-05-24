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
#include "system/utility_system.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
UIElement *G_FocusedElement = NULL;
DragState G_DragState = {0};
// UIElement *G_DraggedElement = NULL;
// Vector2d G_DragOffset = ZERO_VECTOR_2D;
Text_64_IOState tbox_io_buffers = {0};
MouseDownState mouse_down_state = {0};
// char tbox_input_buffer[sizeof(G_FocusedElement->data.textbox.text.string)] = ""; // Stores textbox input
// char tbox_temp_buffer[sizeof(G_FocusedElement->data.textbox.text.string)] = "";  // Stores the original text being output to a focused textbox before it was modified by the user - this buffer is used to restore it if needed

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void HandleMouseEvents(UIElement *root_element, Vector2d mouse_coords);
void HandleLeftMouseDown(UIElement *element, Vector2d mouse_coords);
void HandleLeftMouseUp(UIElement *target, Vector2d mouse_coords);
void HandleTextBoxClick(UIElement *clicked);
void HandleUIDragging(UIElement *e, Vector2d mouse_coords);
void HandleFocusSwitch(UIElement *curr_focus, UIElement *prev_focus);
void HandleTextCommit(UIElement *element, Text_64_IOState *tbox_buffers);
// Vector2d CalculateDragOffset(UIElement *target, Vector2d mouse_coords);
void UpdateTextIO(void);
void RevertTextChanges(UIElement *tbox, Text_64_IOState *t_bufs);
bool IsMouseDragged(MouseDownState mouse_down_state);
bool IsMouseClicked(MouseDownState mouse_down_state);
void UpdateMouseDownState(MouseBtn btn_type, MouseDownState *mouse_down_state, Vector2d mouse_coords);
void ResetMouseDownState(MouseDownState *mouse_down_state);
void InitTextBuffers(UIElement *e, Text_64_IOState *t_bufs);
// -----Global Helpers-----
void UpdateUIFocus(UIElement *element);
void UpdateDragFocus(UIElement *element, Vector2d mouse_coords);
void ClearUIFocus(void);
void ResetDragState(void);

// DISPATCHER - UI activities
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region)
{
    Vector2d mouse_coords = {(float)mouse_x, (float)mouse_y};
    if (!cursor_in_region)
    {
        return;
    }
    UIElement *target = GetElementAt(lpanel_root, mouse_coords);

    HandleMouseEvents(target, mouse_coords);
    // -----HANDLE LEFT MOUSE-----
    // Phase A: Handle the Start/New Events
    // Update mouse_down_state
    // if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    // {
    //     // Handle it
    //     HandleLeftMouseDown(target, mouse_coords);
    // }
    // else // It must be UP
    // {
    //     HandleLeftMouseUp(target, mouse_coords);
    // }

    // Phase B: Handle the "Ongoing" (State)
    // if (G_DraggedElement)
    // {
    //     HandleUIDragging(target, mouse_coords);
    // }

    // Phase D: Typing
    UpdateTextIO();
}

void HandleMouseEvents(UIElement *target, Vector2d mouse_coords)
{
    // -----HANDLE LEFT MOUSE-----
    // Phase A: Handle the Start/New Events
    // Update mouse_down_state
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        // Handle it
        HandleLeftMouseDown(target, mouse_coords);
    }
    else // It must be UP
    {
        HandleLeftMouseUp(target, mouse_coords);
    }
}

// HANDLER - MOUSE DOWN
void HandleLeftMouseDown(UIElement *target, Vector2d mouse_coords)
{
    // -----UPDATE MOUSE DOWN STATE-----
    // 1. ALWAYS update the mouse state first so everything below has fresh data
    UpdateMouseDownState(LEFT, &mouse_down_state, mouse_coords);

    // 2. EVENT PHASE: Check if focus shifted (Only runs on the *initial* press frame)
    if (mouse_down_state.left_button_hold_ticks == 1)
    {
        if (target != G_FocusedElement)
        {
            // Cleanup any state from previous focus
            RevertTextChanges(G_FocusedElement, &tbox_io_buffers);

            // Initialise new focus and new state
            UpdateUIFocus(target);
            InitTextBuffers(target, &tbox_io_buffers);
            // Do NOT return here anymore; let the code cascade into starting the drag!
        }

        // 3. EVENT PHASE: Initialize Dragging if applicable
        if (target && !G_DragState.target_element) // && target->is_draggable ) //Assume everything is draggable for now
        {
            UpdateDragFocus(target, mouse_coords);
        }
    }

    // 4. STATE PHASE: Process Ongoing Drag (Runs every frame the mouse is held down, including frame 1)
    if (G_DragState.target_element)
    {
        // Calculate fresh delta
        G_DragState.drag_delta = VectorSum_2d(mouse_down_state.current_pos, (Vector2d){-mouse_down_state.initial_pos.x, -mouse_down_state.initial_pos.y});

        // Use your threshold check to see if the user has dragged past the deadzone
        if (IsMouseDragged(mouse_down_state))
        {
            if (G_DragState.target_element->is_draggable)
            {
                // CRITICAL: Always drag the lock-on target, NOT the element currently under the hover cursor!
                HandleUIDragging(G_DragState.target_element, mouse_coords);
            }
        }
    }

    // Debug logging
    if (frame_counter.total_frames % 180 == 0)
    {
        printf("MOUSE DOWN [%s] | DRAG_DELTA (%0.0f,%0.0f)\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), G_DragState.drag_delta.x, G_DragState.drag_delta.y);
    }
}

// HANDLER - MOUSE DOWN
void HandleLeftMouseUp(UIElement *target, Vector2d mouse_coords)
{
    // -----CHECK IF IT WAS A CLICK (e.g. MOUSE DOWN FOR <20 frames)-----
    if (mouse_down_state.left_button_hold_ticks > 0) // Only need to do something if the mouse was down, i.e. there was either a drag, or click
    {
        if (IsMouseClicked(mouse_down_state))
        {
            Vector2d mouse_local_coords = (Vector2d){lpanel_to_local_scale.x * mouse_coords.x, lpanel_to_local_scale.y * mouse_coords.y};
            int cell_index = ((int)mouse_local_coords.y * (int)lpanel_resolution.x) + (int)mouse_local_coords.x;
            printf("CLICKED [%s] | PIXEL (%.0f, %.0f) | CELL (%d)(%.1f, %.1f)\n", GetElementTypeName(target->type), mouse_coords.x, mouse_coords.y, cell_index, mouse_local_coords.x, mouse_local_coords.y);

            // Handle Interaction
            // if (IsTextbox(target))
            // {
            //     HandleTextBoxClick(target);
            // }
        }

        ResetMouseDownState(&mouse_down_state);
        ResetDragState();
        printf("ENDED MOUSE DOWN [%s]\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), G_DragState.drag_delta.x, G_DragState.drag_delta.y);
    }
}

void HandleUIDragging(UIElement *e, Vector2d mouse_coords)
{
    if (!e)
        return;

    // 1. Correctly map screen destination pixels to source virtual coordinates
    Vector2d basis_scale = BasisTransform_2d_Scale(camera_lpanel.destination_basis, camera_lpanel.source_basis);

    // 2. Calculate the raw mouse travel distance since the click frame
    Vector2d mouse_down_origin = mouse_down_state.initial_pos;
    Vector2d total_pixel_travel = VectorSum_2d(mouse_down_state.current_pos, (Vector2d){-mouse_down_origin.x, -mouse_down_origin.y});

    // Convert total pixel travel into total virtual unit travel
    Vector2d total_local_travel = (Vector2d){total_pixel_travel.x * basis_scale.x, total_pixel_travel.y * basis_scale.y};

    // 3. Absolute Target Calculation
    // For this approach to be seamless, G_DragState.initial_element_offset must be a Vector2d captured inside your UpdateDragFocus function on the frame left_button_hold_ticks == 1.
    Vector2d new_local_offset;
    new_local_offset.x = G_DragState.initial_element_offset.x + total_local_travel.x;
    new_local_offset.y = G_DragState.initial_element_offset.y + total_local_travel.y;

    // 4. Boundaries Clamping Constraints
    new_local_offset.x = fmaxf(new_local_offset.x, 0.0f);
    new_local_offset.y = fmaxf(new_local_offset.y, 0.0f);

    // 5. Compute real pixel offsets for accurate telemetry tracking
    Vector2d new_pixel_offset = (Vector2d){new_local_offset.x * basis_scale.x, new_local_offset.y * basis_scale.y};
    Vector2d diff_local = VectorSum_2d(new_local_offset, (Vector2d){-e->parent_offset.offset.x, -e->parent_offset.offset.y});
    Vector2d diff_pixel = (Vector2d){diff_local.x / basis_scale.x, diff_local.y / basis_scale.y};

    // 6. Finalize properties assignment
    e->parent_offset.offset = new_local_offset;

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
    char *output_buf = NULL;

    output_buf = e->data.textbox.text.string;
    strncpy(output_buf, tbox_buffers->temp_buffer.string, MAX_TEXTBOX_CHARS - 1);

    // Reset tracking buffers
    tbox_buffers->input_buffer.string[0] = '\0';
    tbox_buffers->temp_buffer.string[0] = '\0';
}

void HandleTextCommit(UIElement *element, Text_64_IOState *tbox_buffers)
{
    // PIPELINE COMMITTED TEXT TO THE SELECTED OBJECT
    Polygonoid *obj = G_WorldState.selected_object;
    if (!element || !IsTextbox(element) || !obj)
    {
        RevertTextChanges(element, tbox_buffers);
        return;
    }
        

    // These TextBoxes (IO) are interactive so check it's an IO type before doing anything
    if (element->type == UI_ELEMENT_TEXTBOX_IO || element->type == UI_ELEMENT_TEXTBOX_SAFE_IO)
    {
        // Get the type of data the textbox is bound to
        DatatType data_type = element->data.textbox.data_type;
        switch (data_type)
        {
        case VECTOR2D:
            PipelineTextToVector(element->data.textbox.text.string, element->data.textbox.data_bind); // Be sure the data type and data bind are compatible!
            break;
        case FLOAT:
            PipelineTextToFloat(element->data.textbox.text.string, element->data.textbox.data_bind); // Be sure the data type and data bind are compatible!
            break;
        // case STRING64: case STRING128:
        //     PipelineTextToFloat(tbox_io_buffers.input_buffer.string, element->data.textbox.data_bind); // Be sure the data type and data bind are compatible!
        //     break;
        default:
            break;
        }
    }

    // Clear buffers
    tbox_io_buffers.input_buffer.string[0] = '\0';
    tbox_io_buffers.temp_buffer.string[0] = '\0';
}

// HANDLER - MOUSE CLICK
void HandleTextBoxClick(UIElement *clicked)
{
    if (!clicked)
        return;

    // 1. If we clicked the same element that already has focus, do nothing
    if (G_FocusedElement == clicked)
        return;

    // 2. Clear focus from the previous element (if any)
    // if (G_FocusedElement != NULL)
    // {
    //     G_FocusedElement->is_focused = false;
    //     // Optional: Trigger an "OnBlur" event here if needed
    // }

    // Assign focus to the new element
    // G_FocusedElement = clicked;
    // clicked->is_focused = true;

    // Move the cursor to the end of the text string
    // This allows the user to start typing immediately after what's already there
    if (clicked->type == UI_ELEMENT_TEXTBOX_IO || clicked->type == UI_ELEMENT_TEXTBOX_SAFE_IO)
    {
        int length = strlen(clicked->data.textbox.text.string);
        clicked->data.textbox.cursor_position = length;
    }
}

void UpdateTextIO()
{
    if (!G_FocusedElement || (G_FocusedElement->type != UI_ELEMENT_TEXTBOX_IO && G_FocusedElement->type != UI_ELEMENT_TEXTBOX_SAFE_IO))
        return;

    char *output_buf = G_FocusedElement->data.textbox.text.string;

    // 1. "Snapshot" for Undo/Cancel
    // If this is the very first time we are typing, save the original state
    if (strlen(tbox_io_buffers.input_buffer.string) == 0 && strlen(tbox_io_buffers.temp_buffer.string) == 0)
    {
        strncpy(tbox_io_buffers.input_buffer.string, output_buf, MAX_TEXTBOX_CHARS - 1);
        tbox_io_buffers.temp_buffer.string[MAX_TEXTBOX_CHARS - 1] = '\0';
    }

    // 2. Handle Character Input
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

    // 3. Handle Backspace
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        int len = strlen(output_buf);
        if (len > 0)
            output_buf[len - 1] = '\0';
    }

    // 4. Handle Escape (Cancel/Undo)
    if (IsKeyPressed(KEY_ESCAPE))
    {
        // Restore from backup
        strncpy(output_buf, tbox_io_buffers.temp_buffer.string, MAX_TEXTBOX_CHARS - 1);
        // Reset tracking buffers
        tbox_io_buffers.input_buffer.string[0] = '\0';
        tbox_io_buffers.temp_buffer.string[0] = '\0';
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
    }

    // 5. Handle Enter (Commit)
    if (IsKeyPressed(KEY_ENTER))
    {
        HandleTextCommit(G_FocusedElement, &tbox_io_buffers);
        tbox_io_buffers.input_buffer.string[0] = '\0';
        tbox_io_buffers.temp_buffer.string[0] = '\0';
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
        // Trigger physics update here! (e.g., UpdateObjectMass())
    }
}

bool IsMouseDragged(MouseDownState mouse_down_state)
{
    Vector2d local_displacement_xy = {0};
    // If mouse is held down for 20 frames, it qualifies
    if (mouse_down_state.left_button_hold_ticks >= 12)
    {
        // If mouse displacement (in local coords/space, not pixels) while being dragged is >= 3% of the local space's resolution (total cells)
        // Vector2d drag_delta = VectorSum_2d(mouse_down_state.current_pos, (Vector2d){-mouse_down_state.initial_pos.x, -mouse_down_state.initial_pos.y});
        Vector2d local_displacement_xy = {lpanel_to_local_scale.x * G_DragState.drag_delta.x, lpanel_to_local_scale.y * G_DragState.drag_delta.y};
        float local_displacement = VectorMagnitude_2d(local_displacement_xy);
        float min_drag = 0.005 * VectorMagnitude_2d(lpanel_resolution);

        frame_counter.total_frames % 180 == 0 ? printf("TOTAL DRAG DELTA: %0.2f | THRESHOLD: %0.2f\n", local_displacement, min_drag) : (void)0;
        // printf("DRAGGED [%s] | Delta (%0.2f, %0.2f)(%0.0f, %0.0f) | New Offset (%0.2f, %0.2f)(%0.0f, %0.0f)\n", GetElementTypeName(e->type), local_delta.x, local_delta.y, pixel_delta.x, pixel_delta.y, new_offset.x, new_offset.y, pixel_new_offset.x, pixel_new_offset.y);

        if (local_displacement > min_drag)
            return true;
    }
    return false;
}

bool IsMouseClicked(MouseDownState mouse_down_state)
{
    // If mouse is held down for less than 20 frames, it qualifies as a click (not a drag)
    if (mouse_down_state.left_button_hold_ticks > 0 && mouse_down_state.left_button_hold_ticks < 20)
    {
        Vector2d drag_delta = VectorSum_2d(mouse_down_state.current_pos, (Vector2d){-mouse_down_state.initial_pos.x, -mouse_down_state.initial_pos.y});
        if (VectorMagnitude_2d(drag_delta) < 5)
            return true;
    }
    return false;
}

void UpdateMouseDownState(MouseBtn btn_type, MouseDownState *mouse_down_state, Vector2d mouse_coords)
{
    switch (btn_type)
    {
    case LEFT:
        mouse_down_state->left_button_hold_ticks++;
        break;
    case RIGHT:
        mouse_down_state->right_button_hold_ticks++;
    default:
        break;
    }
    if (mouse_down_state->left_button_hold_ticks == 1 || mouse_down_state->right_button_hold_ticks == 1)
    {
        mouse_down_state->initial_pos = mouse_coords;
    }
    else
    {
        mouse_down_state->previous_pos = mouse_down_state->current_pos;
        mouse_down_state->current_pos = mouse_coords;
    }
}

// HANDLER - FOCUS SWITCH
void HandleFocusSwitch(UIElement *curr_focus, UIElement *prev_focus)
{
    // Need to perform clean-up actions based on what was previously focused

    // Textboxes
    // if (IsTextbox(prev_focus))
    // {
    //     // If the text being output in the tbox wasn't overwritten by the user, the input_buf will be empty, so just clear the temp_buf
    //     if (strlen(tbox_io_buffers.input_buffer.string) == 0)
    //     {
    //         strncpy(tbox_io_buffers.input_buffer.string, output_buf, MAX_TEXTBOX_CHARS - 1);
    //         tbox_io_buffers.temp_buffer.string[MAX_TEXTBOX_CHARS - 1] = '\0';
    //         tbox_io_buffers.input_buffer.string[0] = '\0';
    //         tbox_io_buffers.temp_buffer.string[0] = '\0';
    //     }
    // }
}

// ----------------------------------------------------------------------------------
// Utility Functions
void ResetMouseDownState(MouseDownState *mouse_down_state)
{
    mouse_down_state->left_button_hold_ticks = 0;
    mouse_down_state->right_button_hold_ticks = 0;
    mouse_down_state->initial_pos = (Vector2d){0, 0};
    mouse_down_state->current_pos = (Vector2d){0, 0};
    mouse_down_state->previous_pos = (Vector2d){0, 0};
}

void ClearUIFocus()
{
    if (G_FocusedElement)
    {
        G_FocusedElement->is_focused = false;
        G_FocusedElement = NULL;
    }
}

void UpdateUIFocus(UIElement *element)
{
    if (G_FocusedElement)
    {
        G_FocusedElement->is_focused = false;
    }
    G_FocusedElement = element;
    if (G_FocusedElement)
    {
        G_FocusedElement->is_focused = true;
        printf("FOCUS CHANGE: [%s]\n", GetElementTypeName(element->type));
    }
}

void UpdateDragFocus(UIElement *element, Vector2d mouse_coords)
{
    G_DragState.target_element = element;

    // float parent_offset_x = 0.0f;
    // float parent_offset_y = 0.0f;

    // if (element->parent)
    // {
    //     // Account for parent origin + its inner padding
    //     parent_offset_x = element->parent_offset.offset.x; // + (e->parent->padding.x * basis_scale.x);
    //     parent_offset_y = element->parent_offset.offset.y; // + (e->parent->padding.y * basis_scale.y);
    // }

    G_DragState.initial_element_offset = element->parent_offset.offset;
    printf("DRAG FOCUS CHANGE: [%s]\n", GetElementTypeName(element->type));
}

void ResetDragState()
{
    G_DragState.drag_delta = (Vector2d){0, 0};
    G_DragState.target_element = NULL;
    printf("DRAG FOCUS CLEARED\n");
}

void InitTextBuffers(UIElement *e, Text_64_IOState *t_bufs)
{
    t_bufs->input_buffer.string[0] = '\0';

    if (IsTextbox(e))
    {
        t_bufs->temp_buffer = e->data.textbox.text;
    }
}