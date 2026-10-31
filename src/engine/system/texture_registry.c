/**********************************************************************************************
*
TEXTURE REGISTRY MODULE (implementation)
*
Holds loaded GPU textures in a single file-local LArray of TextureRecord value structs, structured
line-for-line like entity_catalogue.c (file-local LArray, a g_initialised one-shot guard, idempotent
shutdown, graceful-empty accessors). The array's backing buffer is shallow-dispose; each record's
Texture2D is a GPU handle released EXPLICITLY by UnloadTexture in Shutdown, not by DisposeLArray.
*
OWNERSHIP: the registry is the SOLE owner of every Texture2D (see texture_registry.h). Exactly one
UnloadTexture per LoadTexture, all in TextureRegistry_Shutdown, which must run while the GL context
is still open. Get returns a borrowed pointer that the caller never frees.
*
**********************************************************************************************/
#include "system/texture_registry.h"

#include <stdbool.h>
#include <stdio.h>  // snprintf
#include <string.h> // strcmp

#include "collections/linear_array.h" // LArray and its API
#include "common/common.h"            // LOG_ERROR

// ============================================================================
// Registry storage (module-owned)
// ============================================================================

// Flat array of TextureRecord value structs, backed by a single heap buffer (the LArray's items
// block). Shallow-dispose of the array; each record's Texture2D is a GPU handle released
// explicitly by UnloadTexture in Shutdown, not by DisposeLArray.
static LArray g_textures = {0};
// Guards the one-shot init and the one-shot (idempotent) shutdown.
static bool g_initialised = false;

// One registry record. File-local, not exposed: consumers hold only a TextureId handle.
typedef struct TextureRecord
{
    TextureId id;                              // Dense 1-based handle (array index + 1).
    Texture2D texture;                         // Owned GPU texture; sole owner is the registry.
    char      path[TEXTURE_REGISTRY_PATH_MAX]; // Load path, for load-once de-duplication.
    int       width;                           // Cached texture.width at load (convenience).
    int       height;                          // Cached texture.height at load.
} TextureRecord;

// ============================================================================
// Lifecycle
// ============================================================================

// Initialise the backing storage once. Idempotent, mirroring InitEntityCatalogue's guard. MUST run
// after the GL context exists; LoadOrGet uploads to the GPU.
void TextureRegistry_Init(void)
{
    if (g_initialised)
    {
        return; // Idempotent: allocate exactly once.
    }

    g_textures = MakeLArray(8, sizeof(TextureRecord));
    g_initialised = true;
}

// Load-once accessor. Returns the existing id if `path` is already loaded, otherwise loads via
// LoadTexture, stores a new record and returns its id. Returns INVALID_TEXTURE_ID (logged) on a
// NULL/empty path or a load failure.
TextureId TextureRegistry_LoadOrGet(const char *path)
{
    // Reject an unusable path before touching the GPU.
    if (path == NULL || path[0] == '\0')
    {
        LOG_ERROR("TextureRegistry_LoadOrGet: NULL or empty path");
        return INVALID_TEXTURE_ID;
    }

    // Defensive self-init so a mis-ordered caller logs a clear failure rather than touching a NULL
    // array. The designed boot order (TextureRegistry_Init beside EntityRegistry_Init) means this
    // branch is not normally taken.
    if (!g_initialised)
    {
        TextureRegistry_Init();
    }

    // Load-once: linear scan for a record whose path matches. Linear is correct and simple for the
    // handful of textures here, matching the catalogue's own linear access and avoiding a map.
    for (int i = 0; i < g_textures.count; i++)
    {
        const TextureRecord *record = (const TextureRecord *)LArray_Get(&g_textures, i);
        if (record != NULL && strcmp(record->path, path) == 0)
        {
            return record->id; // Already loaded: return the cached handle, no second GPU upload.
        }
    }

    // First sight of this path: upload it. raylib sets texture.id to 0 on failure.
    Texture2D texture = LoadTexture(path);
    if (texture.id == 0)
    {
        LOG_ERROR("TextureRegistry_LoadOrGet: failed to load '%s'", path);
        return INVALID_TEXTURE_ID; // Do NOT store or UnloadTexture a zero-id texture.
    }

    // Build a dense 1-based record so 0 stays the sentinel, and copy the path into the fixed buffer
    // (snprintf truncates safely if the path is longer than the buffer).
    TextureRecord record = {0};
    record.id = (TextureId)(g_textures.count + 1);
    record.texture = texture;
    snprintf(record.path, sizeof record.path, "%s", path);
    record.width = texture.width;
    record.height = texture.height;

    LArray_Push(&g_textures, &record);
    return record.id;
}

// Borrowed pointer to the Texture2D for `id`, or NULL for the sentinel / an out-of-range id.
// Graceful-empty, exactly like Catalogue_GetParams.
const Texture2D *TextureRegistry_Get(TextureId id)
{
    if (id == INVALID_TEXTURE_ID)
    {
        return NULL;
    }

    // Ids are dense 1-based indices, so the slot is id - 1; bounds-check before dereferencing.
    int index = (int)(id - 1);
    if (index < 0 || index >= g_textures.count)
    {
        return NULL;
    }

    const TextureRecord *record = (const TextureRecord *)LArray_Get(&g_textures, index);
    return record ? &record->texture : NULL; // Borrowed: caller must not UnloadTexture it.
}

// Release every GPU texture (while the GL context is still open) then dispose the backing array.
// Idempotent. This is the SOLE place UnloadTexture runs, one call per load, honouring the registry's
// sole-owner contract.
void TextureRegistry_Shutdown(void)
{
    if (!g_initialised)
    {
        return;
    }

    // One UnloadTexture per LoadTexture (the GPU analogue of the catalogue's single DisposeLArray).
    for (int i = 0; i < g_textures.count; i++)
    {
        TextureRecord *record = (TextureRecord *)LArray_Get(&g_textures, i);
        if (record != NULL)
        {
            UnloadTexture(record->texture);
        }
    }

    DisposeLArray(&g_textures); // Frees the internal buffer; the stack struct remains.
    g_textures = (LArray){0};   // Clear so a later re-init starts from a clean state.
    g_initialised = false;
}
