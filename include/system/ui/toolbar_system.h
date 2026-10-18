/**********************************************************************************************
*
TOOLBAR SYSTEM MODULE
*
A thin vertical tool toolbar mounted against the far-left toolbar viewport band. It follows the
existing XML-driven panel pattern (load markup via UILoader_LoadFromFileWithResolvers, mount a
single-view ViewHostSystem, draw via ViewHostSystem_Draw). Each tool is a radio-exclusive toggle
button: its binding reports whether that tool is the active one (pressed/unpressed visual), and
its action sets the active tool. The Nudge tool action itself is applied additively at the
world click-confirmed site (see Toolbar_ApplyNudgeToEntity), never suppressing normal input.
*
**********************************************************************************************/
#ifndef TOOLBAR_SYSTEM_H
#define TOOLBAR_SYSTEM_H

#include "ui/ui.h"            // UIElement (for the input hit-test root accessor)
#include "entities/entity_id.h" // EntityId (nudge target identifier)

// Build the toolbar from toolbar.xml and mount it against the far-left toolbar viewport band.
void InitToolbar(void);

// Draw the toolbar (no-op when the toolbar failed to load).
void DrawToolbar(void);

// Destroy the toolbar and clear its cached references.
void DestroyToolbar(void);

// Return the toolbar's root UIElement for input hit-testing, or NULL when not loaded.
UIElement *GetToolbarRoot(void);

// Apply one random-direction linear impulse to the entity, additively (on top of its current
// velocity). Wakes a sleeping body first, skips position-locked bodies, and keeps momentum in
// step with the new velocity. Called from the world click-confirmed commit site when the Nudge
// tool is active; takes the entity id resolved by that click.
void Toolbar_ApplyNudgeToEntity(EntityId id);

#endif // TOOLBAR_SYSTEM_H
