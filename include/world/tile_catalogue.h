/**********************************************************************************************
*
TILE CATALOGUE MODULE
*
A data-driven catalogue of tile definitions that backs the shared tile palette. At boot the module
reserves index 0 as the grid-default sentinel, then enumerates tiles/*.json once and parses each
into a flat array of {name, fill colour, optional texture} value structs. It is the single source
of truth for "which tile types exist and how they render": the cell-paint tool clamps against
TilePalette_TypeCount(), and DrawGridSpace resolves each present cell's fill colour (and optional
texture) through the palette accessors.
*
It mirrors the entity catalogue (entity_catalogue.h/.c): a static value LArray, loaded once at
init, cleared with ClearLArray at shutdown. The three public palette accessors
(TilePalette_TypeCount / TilePalette_ColourForType / TilePalette_TextureForType) are declared in
world/world.h (where the first two already lived) and implemented in tile_catalogue.c.
*
**********************************************************************************************/
#ifndef TILE_CATALOGUE_H
#define TILE_CATALOGUE_H

#include "common/common.h"             // ColourRgba
#include "system/texture_registry.h"   // TextureId / INVALID_TEXTURE_ID

// Maximum stored length of a tile name/id (fixed buffer; no heap string) so a tile definition
// stays shallow-dispose. Mirrors CATALOGUE_PREFAB_ID_MAX.
#define TILE_CATALOGUE_NAME_MAX 64

// One tile definition: the tile's id (file stem, fixed buffer), its resolved fill colour, and an
// optional resolved texture handle. Stored by value and shallow-dispose: the definition owns no
// heap storage (a texture_path is consumed into a TextureId at load time and never retained).
typedef struct TileDefinition
{
    char       name[TILE_CATALOGUE_NAME_MAX]; // File stem (id); "" for the reserved index-0 entry.
    ColourRgba fill_colour;                   // Resolved fill colour; unused for the index-0 entry.
    TextureId  texture_id;                    // INVALID_TEXTURE_ID when the definition has no texture.
} TileDefinition;

// Enumerate tiles/*.json ONCE and load each into the catalogue, reserving index 0 as the
// grid-default sentinel first. Idempotent: a second call while already populated is a no-op. MUST
// run AFTER TextureRegistry_Init() because a definition's optional texture_path is resolved to a
// TextureId at load time (a GPU upload). Safe to call before any consumer reads the count.
void InitTileCatalogue(void);

// Release the catalogue's backing storage (shallow dispose; no per-entry cleanup). Idempotent.
void DisposeTileCatalogue(void);

#endif // TILE_CATALOGUE_H
