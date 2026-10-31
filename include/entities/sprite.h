#ifndef SPRITE_COMPONENT_H
#define SPRITE_COMPONENT_H

#include <stdbool.h>

#include "common/common.h"            // ColourRgba
#include "math/cvectors.h"            // Vector2d
#include "system/texture_registry.h" // TextureId

// Minimal Layer-1 sprite component. Stores ONLY a texture handle plus placement/tint data, so it is
// a flat POD with no heap or Texture2D inside: it is trivially memcpy-safe and entity copy/spawn can
// never duplicate or double-free a GPU resource (the registry is the sole texture owner). Source
// rect / z-index / shader are deliberately deferred — they bolt on later without reworking this.
typedef struct SpriteComponent
{
    TextureId  texture_id; // Handle into the texture registry (INVALID_TEXTURE_ID => draw nothing).
    Vector2d   size;       // Sprite size in WORLD units (independent of the collider bounds).
    Vector2d   offset;     // World-unit offset of the sprite centre from the entity anchor_position.
    ColourRgba tint;       // Colour modulation; {255,255,255,255} = untinted.
} SpriteComponent;

// Parameters for creating a sprite. The texture path is stored INLINE (not a borrowed pointer) so
// the struct is fully memcpy-safe and self-contained: the prefab catalogue copies EntityCreateParams
// by value and outlives the parsed cJSON document, so a pointer into that document would dangle.
// This mirrors the catalogue's own fixed-buffer convention (e.g. CatalogueEntry.prefab_id).
typedef struct SpriteComponentParams
{
    char       texture_path[TEXTURE_REGISTRY_PATH_MAX]; // Inline path passed to TextureRegistry_LoadOrGet.
    Vector2d   size;
    Vector2d   offset;
    ColourRgba tint;
} SpriteComponentParams;

// Load the texture via TextureRegistry_LoadOrGet(params->texture_path) and store the resulting
// handle plus size/offset/tint. Returns false on NULL args or when the load yields
// INVALID_TEXTURE_ID (so the factory treats the attach as failed).
bool SpriteComponent_Initialise(SpriteComponent *component, const SpriteComponentParams *params);

// Return whether the component holds a valid texture handle and positive world-unit size.
bool SpriteComponent_IsValid(const SpriteComponent *component);

#endif
