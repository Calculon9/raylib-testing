/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H
#include "common/common.h"
#include "input/drag_interaction.h"
#include "input/pointer_input.h"
#include "math/cvectors.h"
#include "ui/ui.h"
#include "camera/camera.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

typedef enum
{
	UI_PALETTE_SURFACE_TRANSPARENT,
	UI_PALETTE_SURFACE_CONTAINER,
	UI_PALETTE_SURFACE_FIELD_ROW,
	UI_PALETTE_SURFACE_INPUT,
	UI_PALETTE_SURFACE_BUTTON
} UIPaletteSurface;

typedef struct UIPalette
{
	ColourRgba panel_background;
	ColourRgba container_border;
	ColourRgba container_fill;
	ColourRgba field_row_border;
	ColourRgba field_row_fill;
	ColourRgba input_border;
	ColourRgba input_fill;
	ColourRgba button_border;
	ColourRgba button_fill;
	ColourRgba text;
	ColourRgba label_text;
	ColourRgba text_on_dark;
	ColourRgba error;
	ColourRgba warning;
} UIPalette;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// Default UI Properties
extern const Size ui_standard_control_size;
extern const Size ui_standard_textbox_size;
extern const Size ui_standard_textfield_input_size;
extern Vector2d tfield_default_padding;// = {0.04, 0.04};
extern const Size ui_standard_container_size;
extern const Size ui_fill_container_size;
extern const Vector2d ui_standard_container_padding;
extern const Vector2d ui_standard_field_padding;
extern const Size ui_standard_button_size;
extern const Size ui_small_horizontal_button_size;
extern const Size ui_fill_button_size;
extern const Vector2d ui_standard_button_padding;
extern const Size ui_standard_selector_container_size;
extern const Size ui_standard_selector_button_size;
extern const Spacing ui_standard_stack_spacing;
extern const Spacing ui_compact_stack_spacing;
extern const Spacing ui_standard_wrap_spacing;
extern const Spacing ui_compact_wrap_spacing;
extern const Spacing ui_zero_horizontal_wrap_spacing;
extern const Spacing ui_standard_inline_spacing;
extern const Spacing ui_zero_inline_spacing;

extern const UIPalette ui_default_palette;
extern const UIPalette ui_classic_palette;
extern const UIPalette ui_earth_palette;
extern const UIPalette ui_harbor_palette;
extern const UIPalette ui_meadow_palette;

void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
								 ColourRgba *out_border, ColourRgba *out_fill);
InputRouteResult UpdateUISystem(const InputFrame *input);
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);
void UpdateUISpace(UIElement *root_element, UIBox seed_box);

#endif
