/**********************************************************************************************
*
DETAIL STRIP SYSTEM MODULE
*
A thin, always-present read-only strip docked at the BOTTOM of the central content column (same
height as the info strip). It shows ONE composed description line. When the pointer is over a UI
element that carries a hint (markup hint= attribute, plus an optional on_hint callback for live
values) the strip describes that element; otherwise it falls back to the active tool's context
(spawn prefab / paint tile). It follows the existing XML-driven panel pattern (load markup via
UILoader_LoadFromFileWithResolvers, mount a single-view ViewHostSystem, draw via
ViewHostSystem_Draw) and drives a read-only BIND_STRING textbox through the same per-frame
binding-refresh path the info strip uses. The hovered element is read from G_UIState.hovered_element,
which ProcessUIInput refreshes every frame.
*
**********************************************************************************************/
#ifndef DETAIL_STRIP_SYSTEM_H
#define DETAIL_STRIP_SYSTEM_H

// Build the detail strip from detail_strip.xml and mount it against the detail-strip viewport.
void InitDetailStrip(void);

// Draw the detail strip (no-op when the strip failed to load).
void DrawDetailStrip(void);

// Destroy the detail strip and clear its cached references.
void DestroyDetailStrip(void);

#endif // DETAIL_STRIP_SYSTEM_H
