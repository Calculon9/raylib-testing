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
#include "system/lpanel_system.h"
#include "system/viewport_system.h"
#include "system/utility_system.h"
#include <stdlib.h>
#include "world/world.h"
#include "system/str_helpers.h"
#include "system/rpanel_system.h"
#include "system/command_queue.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
DragState G_DragState = {0};
// UIElement *G_DraggedElement = NULL;
// Vector2d G_DragOffset = ZERO_VECTOR_2D;
Text_64_IOState tbox_io_buffers = {0};
MouseDownState mouse_down_state = {0};
// char tbox_input_buffer[sizeof(G_UIState.focused_element->data.textbox.text.string)] = ""; // Stores textbox input
// char tbox_temp_buffer[sizeof(G_UIState.focused_element->data.textbox.text.string)] = "";  // Stores the original text being output to a focused textbox before it was modified by the user - this buffer is used to restore it if needed

//----------------------------------------------------------------------------------
// Functions Definition
//----------------------------------------------------------------------------------
void HandleMouseEvents(UIElement *root_element, Vector2d mouse_coords);
void HandleLeftMouseDown(UIElement *element, Vector2d mouse_coords);
void HandleLeftMouseUp(UIElement *target, Vector2d mouse_coords);
void HandleTextBoxClick(UIElement *clicked);
void HandleBtnClick(UIElement *target);
void HandleUIDragging(UIElement *e, Vector2d mouse_coords);
void HandleFocusSwitch(UIElement *curr_focus, UIElement *prev_focus);
void HandleTextCommit(UIElement *element, Text_64_IOState *tbox_buffers);
// Vector2d CalculateDragOffset(UIElement *target, Vector2d mouse_coords);
void UpdateTextIO(void);
void RevertTextChanges(UIElement *tbox, Text_64_IOState *t_bufs);
bool IsMouseDragged(MouseDownState mouse_down_state);
bool IsMouseClicked(MouseDownState mouse_down_state);
void InitTextBuffers(UIElement *e, Text_64_IOState *t_bufs);
UIElement *ResolveUIRootTarget(Vector2d mouse_coords);
Vector2d ResolvePixelToLocalDragScale(UIElement *element);
bool ResolvePointerLocalCoords(UIElement *element, Vector2d mouse_pixel_coords, Vector2d *out_local_coords);
void UpdateUIFocus(UIElement *element);
void UpdateDragFocus(UIElement *element, Vector2d mouse_coords);
void ClearUIFocus(void);
void ResetDragState(void);

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
void ProcessUIInput(int mouse_x, int mouse_y, bool cursor_in_region)
{
    Vector2d mouse_coords = {(float)mouse_x, (float)mouse_y};
    if (!cursor_in_region)
    {
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
    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    // -----UPDATE MOUSE DOWN STATE-----
    // 1. ALWAYS update the mouse state first so everything below has fresh data
    UpdatePointerState(POINTER_BUTTON_LEFT, &mouse_down_state, mouse_coords);

    // 2. EVENT PHASE: Check if focus shifted (Only runs on the *initial* press frame)
    if (mouse_down_state.left_button_hold_ticks == 1)
    {
        if (target != G_UIState.focused_element)
        {
            // Cleanup any state from previous focus
            RevertTextChanges(G_UIState.focused_element, &tbox_io_buffers);

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

        // Use threshold check to see if the user has dragged past the deadzone
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
    if (target && !target->is_enabled)
    {
        target = NULL;
    }

    // -----CHECK IF IT WAS A CLICK (e.g. MOUSE DOWN FOR <20 frames)-----
    if (mouse_down_state.left_button_hold_ticks > 0) // Only need to do something if the mouse was down, i.e. there was either a drag, or click
    {
        if (IsMouseClicked(mouse_down_state))
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

        ResetPointerState(&mouse_down_state);
        ResetDragState();
        printf("ENDED MOUSE DOWN [%s]\n", GetElementTypeName(target ? target->type : UI_ELEMENT_NONE), G_DragState.drag_delta.x, G_DragState.drag_delta.y);
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
        if (!G_WorldState.world)
            return;

        // Enqueue a create-entity command instead of creating immediately
        if (G_WorldState.newtonoid_params)
        {
            EnqueueCreateEntity(G_WorldState.newtonoid_params);
        }
    } else if (action == BUTTON_ACTION_DELETE_ENTITY)
    {
        if (!G_WorldState.world)
            return;

        // Enqueue a delete-entity command instead of deleting immediately
        if (G_WorldState.selected_object)
        {
            EnqueueDeleteEntity(G_WorldState.selected_object);
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
    Vector2d mouse_down_origin = mouse_down_state.initial_pos;
    Vector2d total_pixel_travel = VectorSum_2d(mouse_down_state.current_pos, (Vector2d){-mouse_down_origin.x, -mouse_down_origin.y});

    // Convert total pixel travel into total virtual unit travel
    Vector2d total_local_travel = (Vector2d){total_pixel_travel.x * pixels_to_ui_local_scale.x, total_pixel_travel.y * pixels_to_ui_local_scale.y};

    // Absolute Target Calculation
    // For this approach to be seamless, G_DragState.initial_element_offset must be a Vector2d captured inside your UpdateDragFocus function on the frame left_button_hold_ticks == 1.
    Vector2d new_local_offset;
    new_local_offset.x = G_DragState.initial_element_offset.x + total_local_travel.x;
    new_local_offset.y = G_DragState.initial_element_offset.y + total_local_travel.y;

    // Compute real pixel offsets for accurate telemetry tracking
    Vector2d new_pixel_offset = (Vector2d){new_local_offset.x / pixels_to_ui_local_scale.x, new_local_offset.y / pixels_to_ui_local_scale.y};
    Vector2d diff_local = VectorSum_2d(new_local_offset, (Vector2d){-e->manual_parent_offset.x, -e->manual_parent_offset.y});
    Vector2d diff_pixel = (Vector2d){diff_local.x / pixels_to_ui_local_scale.x, diff_local.y / pixels_to_ui_local_scale.y};

    // Finalize properties assignment
    e->parent_offset.offset = new_local_offset;
    e->manual_parent_offset = new_local_offset;
    e->has_manual_parent_offset = true;
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
    char *output_buf = NULL;

    output_buf = e->data.textbox.text.string;
    safe_strncpy(output_buf, tbox_buffers->temp_buffer.string, MAX_TEXTBOX_CHARS);

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
    if (element->type == UI_ELEMENT_TEXTBOX_IO || element->type == UI_ELEMENT_TEXTBOX_SAFE_IO)
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
    if (clicked->type == UI_ELEMENT_TEXTBOX_IO || clicked->type == UI_ELEMENT_TEXTBOX_SAFE_IO)
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

    if (!G_UIState.focused_element || (G_UIState.focused_element->type != UI_ELEMENT_TEXTBOX_IO && G_UIState.focused_element->type != UI_ELEMENT_TEXTBOX_SAFE_IO))
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

bool IsMouseDragged(MouseDownState mouse_down_state)
{
    return IsPointerDrag(mouse_down_state, 5.0f);
}

bool IsMouseClicked(MouseDownState mouse_down_state)
{
    return IsPointerClick(mouse_down_state, 20, 5.0f);
}

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
    if (!element || !element->is_enabled)
    {
        return;
    }

    G_DragState.target_element = element;

    // float parent_offset_x = 0.0f;
    // float parent_offset_y = 0.0f;

    // if (element->parent)
    // {
    //     // Account for parent origin + its inner padding
    //     parent_offset_x = element->parent_offset.offset.x; // + (e->parent->padding.x * basis_scale.x);
    //     parent_offset_y = element->parent_offset.offset.y; // + (e->parent->padding.y * basis_scale.y);
    // }

    G_DragState.initial_element_offset = element->manual_parent_offset;
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
    ResetTextBuffers(t_bufs);

    if (IsTextbox(e))
    {
        SnapshotTextBuffers(t_bufs, e->data.textbox.text.string);
    }
}

UIElement *ResolveUIRootTarget(Vector2d mouse_coords)
{
    UIElement *target = NULL;

    if (rpanel_root)
    {
        target = GetElementAt(rpanel_root, mouse_coords);
    }

    if (!target && lpanel_root)
    {
        target = GetElementAt(lpanel_root, mouse_coords);
    }

    return target;
}

