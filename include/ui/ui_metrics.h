/**********************************************************************************************
*
UI METRICS: the single home for every UI pixel constant
*
* All values are in UI pixels (the layout unit; the renderer multiplies by the UI pixel scale when
* drawing) unless a name says otherwise. Code and markup never repeat these as literals: C uses the
* macro, markup uses the matching named `size=` keyword (see UILoader size keywords in ui_loader.c).
*
**********************************************************************************************/
#ifndef UI_METRICS_H
#define UI_METRICS_H

//----------------------------------------------------------------------------------
// Section titles (accent bar)
//----------------------------------------------------------------------------------
// Section-title (accent bar) geometry: a 4x16 bar at the label's left edge, with the title text
// starting 10 px right of the bar's left edge. Shared by measuring and drawing so every title
// inherits the same spacing.
#define UI_TITLE_BAR_WIDTH_PX 4.0f
#define UI_TITLE_BAR_HEIGHT_PX 16.0f
#define UI_TITLE_TEXT_INDENT_PX 10.0f

//----------------------------------------------------------------------------------
// Layout
//----------------------------------------------------------------------------------
// Grid chips (Container columns=): a Button whose text run is wider than its cell minus this
// margin (the text's side breathing room) spans the whole row instead.
#define UI_GRID_CHIP_TEXT_MARGIN_PX 6.0f

// Tolerance when deciding whether a child still fits a wrapping line: equal grid columns sum to the
// line limit only up to float rounding, so a child within this slack still fits.
#define UI_WRAP_EPSILON_PX 0.01f

//----------------------------------------------------------------------------------
// Text boxes and fields
//----------------------------------------------------------------------------------
// Gap between a bordered textbox's border and its text: with the default 1 px border the glyph's own
// 2 px blank column places the first ink 7 px from the box edge, as in the mock.
#define UI_TEXTBOX_TEXT_GAP_PX 4.0f
// Glyphs in the "..." appended to read-only text that does not fit its box.
#define UI_ELLIPSIS_CHAR_COUNT 3
// Horizontal gap between a stacked vector box's "x" / "y" prefix and its value.
#define UI_VECTOR_PREFIX_ADVANCE_PX 16.0f
// Gap kept between a field's value text and its right-aligned unit suffix.
#define UI_FIELD_UNIT_GAP_PX 4.0f

//----------------------------------------------------------------------------------
// Borders and rules
//----------------------------------------------------------------------------------
// Upper bound accepted for a border-weight= attribute, in screen pixels.
#define UI_MAX_BORDER_WEIGHT 16
// Thickness of the coral rule along the bottom of every panel header block.
#define PANEL_HEADER_RULE_PX 2.0f
// Border weight (screen pixels) of the floating popup sheets' outline.
#define UI_POPUP_EDGE_WEIGHT_PX 1

//----------------------------------------------------------------------------------
// Named sizes (authored in markup as size="<keyword>", see the loader's size keyword table)
//----------------------------------------------------------------------------------
// Panel header block: title line and sub line heights (both lines fill the panel width).
#define UI_HEADER_TITLE_HEIGHT_PX 20.0f
#define UI_HEADER_SUB_HEIGHT_PX 17.0f
// One standard control row: the height of buttons, tiles and fields.
#define UI_CONTROL_HEIGHT_PX 25.0f
// Icon-only toolbar tiles (width; height is UI_CONTROL_HEIGHT_PX).
#define UI_ICON_BUTTON_WIDTH_PX 34.0f
// COMP action buttons (REMOVE / ADD): fixed widths, one control row high.
#define UI_ACTION_REMOVE_WIDTH_PX 78.0f
#define UI_ACTION_ADD_WIDTH_PX 48.0f
// Width of the popup menu host (its height hugs the content).
#define UI_POPUP_WIDTH_PX 188.0f

//----------------------------------------------------------------------------------
// Scrollbars
//----------------------------------------------------------------------------------
// View scrollbars (Y on the right edge, X on the bottom edge): bar width, gap kept from the box
// edges, and shortest thumb.
#define UI_SCROLLBAR_WIDTH_PX 6.0f
#define UI_SCROLLBAR_INSET_PX 4.0f
#define UI_SCROLLBAR_MIN_THUMB_PX 20.0f
// Scrollbars of scrolling lists inside a view: narrower, flush with the element's right edge.
#define UI_LIST_SCROLLBAR_WIDTH_PX 4.0f
#define UI_LIST_SCROLLBAR_MIN_THUMB_PX 12.0f

#endif
