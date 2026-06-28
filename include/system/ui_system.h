/**********************************************************************************************
*
UI SYSTEM MODULE
*
**********************************************************************************************/
#ifndef UI_SYSTEM_H
#define UI_SYSTEM_H
#include "common/common.h"
#include "colour/colour.h"
#include "math/cvectors.h"
#include "ui/ui.h"
#include "camera/camera.h"

//----------------------------------------------------------------------------------
// Macros and Defines
//----------------------------------------------------------------------------------

// Panel Colour Pallette
#define COLOUR_PANEL_DARK_1 OLIVE_GARDEN_GREEN_D // BROWN_1_RGBA
// #define COLOUR_PANEL_MID_1 BEIGE_RGBA
#define COLOUR_PANEL_LIGHT_1 OLIVE_GARDEN_GREEN_L // BROWN_1_RGBA_4
#define COLOUR_PANEL_LIGHT_3 OLIVE_GARDEN_CREAM
#define COLOUR_PANEL_DARK_2 OLIVE_GARDEN_TAN_D // DARKBROWN_RGBA
// #define COLOUR_PANEL_MID_2 BROWN_2_RGBA_1
#define COLOUR_PANEL_LIGHT_2 OLIVE_GARDEN_TAN_L // BROWN_2_RGBA_4
#define COLOUR_ERROR RED_ERROR_RGBA
#define COLOUR_WARNING YELLOW_WARNING_RGBA

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------



//----------------------------------------------------------------------------------
// Global Variables Declaration (shared by several modules)
//----------------------------------------------------------------------------------
// Coordinate Space Properties
extern Vector2d lpanel_u;                              // = {1, 0};
extern Vector2d lpanel_v;                              // = {0, 1};
extern Vector2d lpanel_resolution;                     //= {0};
extern Vector2d lpanel_pixel_origin, lpanel_pixel_end; //= {0};

// Logical->pixel-space conversion properties
extern Vector2d lpanel_pixel_u;            // = {75, 0};
extern Vector2d lpanel_pixel_v;            // = {0, 75};
extern Vector2d lpanel_origin, lpanel_end; //= {0}; // Dependent on the game world screen area
extern Camera2d camera_lpanel;
extern Vector2d local_to_lpanel_scale; // = {0};lpanel_to_local_scale
extern Vector2d lpanel_to_local_scale;

// UI Elements
extern UIBox seed_box; // This is the box that will be used as the parent box for the root element of the panel, and all other elements will calculate their positions and dimensions based on this box, which represents the entire panel area in pixel coordinates
extern UIElement *lpanel_root;
extern UIElement *lpanel_properties_tcont;
extern UIElement *lpanel_stats_tcont;
//extern Vector2d lpanel_properties_tcont_offset;
//extern Vector2d lpanel_stats_tcont_offset;
// - default text container props
//extern Vector2d lpanel_tcont_default_dims;
//extern Vector2d lpanel_tcont_default_padding;

// Default UI Properties
extern Offset tbox_tlabel_default_offset;
extern Vector2d tbox_default_padding;
extern Vector2d tlabel_default_padding;
extern ColourRgba tbox_default_colour_border;
extern ColourRgba tbox_default_colour_fill;// = COLOUR_PANEL_LIGHT_3;  // COLOUR_PANEL_DARK_1;
extern Size tfield_default_size;// = {{6, 0.36}, SIZE_FIXED};
extern Size tbox_default_size;
extern Vector2d tfield_default_padding;// = {0.04, 0.04};
extern ColourRgba tfield_default_colour_fill;// = COLOURLESS_RGBA;
extern Size tcont_default_size;// = {{1, 0.3}, SIZE_PERCENT};
extern Vector2d tcont_default_padding;// = {0.06, 0.06};
extern Spacing tcont_default_child_spacing;// = {{0, 0.015}, PERCENT, SPACING_NORMAL};
extern ColourRgba tcont_default_colour_fill;// = COLOUR_PANEL_LIGHT_1;
extern ColourRgba tcont_default_colour_border;// = COLOUR_PANEL_DARK_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
extern Size btn_default_size;// = {{1, 0.25}, SIZE_FIXED};
extern Vector2d btn_default_padding ;//= {0.025, 0.025};
extern Spacing btn_cont_default_child_spacing;// = {{0.03, 0.0}, PERCENT, SPACING_NONE};
extern Spacing lpanel_root_child_spacing;// = {{0, 0.015}, PERCENT, SPACING_STACKED};
extern Spacing cont_default_child_spacing;
// static Vector2d tbox_tlabel_default_offset = {0.06, 0};
// static Vector2d tbox_default_padding = {0.04, 0.04};
// static ColourRgba tbox_default_colour_border = COLOUR_PANEL_DARK_1; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
// static ColourRgba tbox_default_colour_fill = COLOUR_PANEL_LIGHT_3;  // COLOUR_PANEL_DARK_1;
// static Vector2d tfield_default_dims = {3.6, 0.4};
// static Vector2d tfield_default_padding = {0.08, 0.08};
// static ColourRgba tfield_default_colour_fill = COLOURLESS_RGBA;
// static Vector2d tcont_default_padding = {0.15, 0.15};
// static Vector2d tcont_default_child_spacing = {0, 0.05};
// static ColourRgba tcont_default_colour_fill = COLOUR_PANEL_LIGHT_1;
// static ColourRgba tcont_default_colour_border = COLOUR_PANEL_DARK_2; // {150, 115, 70, 255};//MAROON_RGBA; //{128, 99, 42, 100};
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// void UpdateUISystem(int mouse_x, int mouse_y);

#endif