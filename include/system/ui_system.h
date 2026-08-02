/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H
#include "common/common.h"
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
	ColourRgba text_on_dark;
	ColourRgba error;
	ColourRgba warning;
} UIPalette;



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// Default UI Properties
extern Offset tbox_tlabel_default_offset;
extern Vector2d tbox_default_padding;
extern Vector2d tlabel_default_padding;
extern Size ui_default_control_size;
extern Size tbox_default_size;
extern Vector2d tfield_default_padding;// = {0.04, 0.04};
extern Size tcont_default_size;// = {{1, 0.3}, SIZE_PERCENT};
extern Vector2d tcont_default_padding;// = {0.06, 0.06};
extern Spacing tcont_default_child_spacing;// = {{0, 0.015}, PERCENT, SPACING_NORMAL};
extern Size btn_default_size;
extern Vector2d btn_default_padding ;//= {0.025, 0.025};
extern Spacing btn_cont_default_child_spacing;// = {{0.03, 0.0}, PERCENT, SPACING_NONE};
extern Spacing lpanel_root_child_spacing;// = {{0, 0.015}, PERCENT, SPACING_STACKED};
extern Spacing cont_default_child_spacing;

extern const UIPalette ui_default_palette;
extern const UIPalette ui_classic_palette;
extern const UIPalette ui_earth_palette;
extern const UIPalette ui_harbor_palette;

void UIPalette_GetSurfaceColours(const UIPalette *palette, UIPaletteSurface surface,
								 ColourRgba *out_border, ColourRgba *out_fill);
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);
void UpdateUISpace(UIElement *root_element, UIBox seed_box);

#endif
