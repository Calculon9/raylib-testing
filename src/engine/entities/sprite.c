#include "entities/sprite.h"

#include "system/texture_registry.h" // TextureRegistry_LoadOrGet / INVALID_TEXTURE_ID

// Load the texture via the registry (load-once) and store the resulting handle plus placement/tint.
// Returns false on NULL args or a failed/invalid load so the factory treats the attach as failed.
bool SpriteComponent_Initialise(SpriteComponent *component, const SpriteComponentParams *params)
{
    if (!component || !params)
    {
        return false;
    }

    // The registry logs its own failure and returns the sentinel; a sentinel handle is a hard fail.
    TextureId texture_id = TextureRegistry_LoadOrGet(params->texture_path);
    if (texture_id == INVALID_TEXTURE_ID)
    {
        return false;
    }

    *component = (SpriteComponent){
        .texture_id = texture_id,
        .size = params->size,
        .offset = params->offset,
        .tint = params->tint};
    return true;
}

// Return whether the component holds a valid texture handle and a positive world-unit size.
bool SpriteComponent_IsValid(const SpriteComponent *component)
{
    return component && component->texture_id != INVALID_TEXTURE_ID &&
           component->size.x > 0.0f && component->size.y > 0.0f;
}
