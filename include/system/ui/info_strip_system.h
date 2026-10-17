/**********************************************************************************************
*
INFO STRIP SYSTEM MODULE
*
A thin, read-only overlay strip that reports what the pointer is hovering over and what is
currently selected, both expressed through the universal TargetRef resolver. It follows the
existing XML-driven panel pattern (load markup via UILoader_LoadFromFileWithResolvers, mount a
single-view ViewHostSystem, draw via ViewHostSystem_Draw) and drives two live BIND_STRING
labels through the same per-frame binding-refresh path the other panels use.
*
**********************************************************************************************/
#ifndef INFO_STRIP_SYSTEM_H
#define INFO_STRIP_SYSTEM_H

#include "input/pointer_input.h"

// Build the info strip from info_strip.xml and mount it against the game viewport.
void InitInfoStrip(void);

// Draw the info strip (no-op when the strip failed to load).
void DrawInfoStrip(void);

// Recompute the hovered TargetRef from the current pointer position. READ-ONLY: it only reads
// input->pointer_position and never consumes input or participates in the route-claim protocol.
void InfoStrip_UpdateHover(const InputFrame *input);

// Destroy the info strip and clear its cached references.
void DestroyInfoStrip(void);

#endif // INFO_STRIP_SYSTEM_H
