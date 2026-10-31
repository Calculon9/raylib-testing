/**********************************************************************************************
*
ENTITY CATALOGUE MODULE
*
A data-driven catalogue of spawnable entity prefabs. At boot the module enumerates the
prefabs/*.json files once and parses each into a flat array of {prefab id, creation params}
entries. It is the single source of truth for "which prefabs can be spawned": the active-prefab
selecter clamps against Catalogue_Count(), the spawn tool copies a Catalogue_GetParams() entry,
and the detail strip reports the active entry's details. The catalogue is additive — it does NOT
replace the popup's hardcoded gear/portal loads.
*
**********************************************************************************************/
#ifndef ENTITY_CATALOGUE_H
#define ENTITY_CATALOGUE_H

#include "entities/entity_factory.h" // EntityCreateParams

// Maximum stored length of a prefab id (fixed buffer; no heap string) so a catalogue entry stays
// shallow-dispose. 64 comfortably holds the authored ids ("gear_0", "portal_0", ...) with room
// to spare.
#define CATALOGUE_PREFAB_ID_MAX 64

// One catalogue entry: the prefab's id (fixed buffer) plus its fully-parsed creation params.
// Stored by value and shallow-dispose: EntityCreateParams transitively embeds an LArray
// (physics.surface.surface_vectors), but the prefab loader leaves that buffer NULL, so a
// catalogue entry owns no heap buffer and an array of these entries is shallow-dispose.
typedef struct CatalogueEntry
{
    char               prefab_id[CATALOGUE_PREFAB_ID_MAX];
    EntityCreateParams params;
} CatalogueEntry;

// Enumerate prefabs/*.json ONCE and load each into the catalogue. Idempotent: a second call is a
// no-op while the catalogue is already populated. Safe to call before any consumer reads the count.
void InitEntityCatalogue(void);

// Release the catalogue's backing storage (shallow dispose; no per-entry cleanup). Idempotent.
void DisposeEntityCatalogue(void);

// Number of prefabs in the catalogue (0 if none were found). Single source of truth for the
// active-prefab clamp/wrap, exactly as TilePalette_TypeCount() is for the active tile type.
int Catalogue_Count(void);

// Prefab id for entry i, or "" when i is out of range. Borrowed pointer into catalogue storage;
// valid until DisposeEntityCatalogue. Never NULL.
const char *Catalogue_GetId(int i);

// Creation params for entry i, or NULL when i is out of range or the catalogue is empty. Borrowed
// pointer into catalogue storage; the caller COPIES it (e.g. to set anchor_position) before spawn.
const EntityCreateParams *Catalogue_GetParams(int i);

#endif // ENTITY_CATALOGUE_H
