/**********************************************************************************************
*
DETAIL STRIP SYSTEM MODULE
*
A thin, always-present read-only strip docked at the BOTTOM of the central content column (same
height as the info strip). It reports the details of the CURRENT detail context. For v1 that
context is always the ACTIVE PREFAB (the spawn tool/selecter context): the strip shows the active
prefab's id, shape/type, which components it carries, and a couple of key physics values. It
follows the existing XML-driven panel pattern (load markup via UILoader_LoadFromFileWithResolvers,
mount a single-view ViewHostSystem, draw via ViewHostSystem_Draw) and drives read-only BIND_STRING
labels through the same per-frame binding-refresh path the info strip uses. The strip is built
reusably (a single context seam) but deliberately wires only the prefab context now — there is no
multi-context provider framework.
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
