/**********************************************************************************************
*
UI ICONS MODULE
*
A small home for the toolbar's procedural icon glyphs. Each function is an on_draw hook
(UIEventHandler-compatible: void Fn(UIElement *)) that the renderer calls on top of an element's
flat box. Every glyph reads e->screen_box for geometry, is composed from the same raylib
primitives the Nudge glyph uses (DrawLineEx / DrawRectangleLinesEx / DrawCircleV / line loops),
and tints itself by ON/OFF state. The toggle glyphs read e->colour_fill (which the per-frame
binding refresh sets to toggle_on_fill/off), so they stay in lock-step with the button fill
without duplicating the name->DebugOverlayId mapping; the Nudge glyph keeps its own tool-state
tint because the Nudge button carries no binding=.
*
**********************************************************************************************/
#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "ui/ui.h" // UIElement (the on_draw hook target)

// Relocated Nudge tool glyph: an arrow pushing a small box with a couple of trailing motion dots.
void UIIcon_DrawNudge(UIElement *e);

// Cell Paint tool glyph: a 2x2 grid of cells with one cell filled ("paint a cell"). The frame
// ink follows the active-tool state while the inner swatch shows the ACTIVE TILE TYPE's palette
// colour (merged in from the retired tile-type cycle glyph), so the one consolidated button shows
// both the tool-active state and the colour the mouse wheel selects.
void UIIcon_DrawCellPaint(UIElement *e);

// Spawn TOOL glyph: a generic place/crosshair pictogram (a small shape outline with a crosshair),
// drawn in the active-tool ink (bright button_fill when TOOL_SPAWN is active, normal text ink
// otherwise), exactly like the other tool glyphs.
void UIIcon_DrawToolSpawn(UIElement *e);

// Dashboard toggle (debug.dashboard): a gauge dial (circle ring + short radial needle).
void UIIcon_DrawDashboard(UIElement *e);

// Viewport Grid toggle (view.viewport-grid): a screen frame with a single centred cross.
void UIIcon_DrawViewportGrid(UIElement *e);

// World Grid toggle (view.world-grid): a rect outline split 3x3 by two H + two V lines (dense).
void UIIcon_DrawWorldGrid(UIElement *e);

// World Grid Labels toggle (view.world-grid-labels): the dense 3x3 grid plus tick dots.
void UIIcon_DrawWorldGridLabels(UIElement *e);

// Universe Grid Labels toggle (view.universe-grid-labels): a coarse single-cell grid plus ticks.
void UIIcon_DrawUniverseGridLabels(UIElement *e);

// UI Borders toggle (view.ui-borders): a thick rect outline with a thinner concentric inset.
void UIIcon_DrawUIBorders(UIElement *e);

// Object Axes toggle (gizmo.object-axes): two perpendicular arrows from a common origin.
void UIIcon_DrawObjectAxes(UIElement *e);

// Object Hull toggle (gizmo.object-hull): a convex polygon (hexagon) outline.
void UIIcon_DrawObjectHull(UIElement *e);

// Object AABB toggle (gizmo.object-aabb): a rect outline with a small centred dot inside.
void UIIcon_DrawObjectAABB(UIElement *e);

#endif // UI_ICONS_H
