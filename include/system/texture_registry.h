/**********************************************************************************************
*
TEXTURE REGISTRY MODULE
*
Engine-owned store of GPU textures with raylib (LoadTexture / UnloadTexture) sitting at the very
floor, mirroring how the entity catalogue owns its prefab data. The registry is the single source
of truth for "which textures are loaded": a path is loaded at most once (load-once) and handed back
as a lightweight TextureId handle, so consumers never hold a Texture2D copy.
*
OWNERSHIP (per OWNERSHIP.md):
  - The registry is the SOLE owner of every Texture2D it loads. Each LoadTexture has EXACTLY one
    matching UnloadTexture, and every UnloadTexture happens inside TextureRegistry_Shutdown. No
    other module unloads a registry texture.
  - TextureRegistry_Get returns a BORROWED (@borrowed) const Texture2D *: valid until
    TextureRegistry_Shutdown; the caller MUST NOT UnloadTexture it or retain it past shutdown.
  - Consumers store a TextureId handle (a dense key), never a Texture2D by value, so there is no
    copy / double-free surface.
*
**********************************************************************************************/
#ifndef TEXTURE_REGISTRY_H
#define TEXTURE_REGISTRY_H

#include "raylib.h" // Texture2D

#include <stdint.h> // uint32_t

// Opaque handle into the texture registry. A dense 1-based index (NOT a generational slot handle):
// textures are never released mid-session in Layer 1, so the catalogue-style dense key is enough,
// mirroring INVALID_ENTITY_ID's role for EntityId.
typedef uint32_t TextureId;

// Reserved "no texture" sentinel. Zero so a zero-initialised field means "unset", exactly as
// INVALID_ENTITY_ID == 0 does for entities.
#define INVALID_TEXTURE_ID ((TextureId)0)

// Longest texture path the registry stores inline (fixed buffer; no heap string), mirroring
// CATALOGUE_PREFAB_ID_MAX so a record stays shallow-dispose.
#define TEXTURE_REGISTRY_PATH_MAX 260

// Initialise the registry's backing storage. Idempotent: a second call while already initialised
// is a no-op. MUST be called after the GL context exists (InitWindow) because LoadOrGet uploads to
// the GPU.
void TextureRegistry_Init(void);

// Load-once accessor: if `path` is already loaded, return its existing id; otherwise LoadTexture,
// store a new record, and return the new id. Returns INVALID_TEXTURE_ID (with LOG_ERROR) if the
// load fails or `path` is NULL/empty. The `path` is borrowed: copied into the record, never retained.
TextureId TextureRegistry_LoadOrGet(const char *path);

// Borrowed pointer to the stored Texture2D for `id`, or NULL when `id` is INVALID_TEXTURE_ID or
// not found. Valid until TextureRegistry_Shutdown. The caller MUST NOT UnloadTexture it.
const Texture2D *TextureRegistry_Get(TextureId id);

// UnloadTexture every stored record (while the GL context is still open) then dispose the backing
// array. Idempotent: a second call is a no-op. The SOLE place UnloadTexture is called.
void TextureRegistry_Shutdown(void);

#endif // TEXTURE_REGISTRY_H
