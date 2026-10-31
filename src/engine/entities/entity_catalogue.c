/**********************************************************************************************
*
ENTITY CATALOGUE MODULE (implementation)
*
Enumerates prefabs/*.json once at boot and parses each file into a flat LArray of CatalogueEntry
value structs. Storage is a single file-local LArray with a heap internal buffer; disposal is
shallow (DisposeLArray once) because a prefab-loaded EntityCreateParams owns no heap buffer: the
prefab loader zero-initialises physics.surface and never fills surface.surface_vectors, so each
entry's surface.surface_vectors.items is NULL. The accessors degrade gracefully (empty catalogue
=> count 0, id "", params NULL) so a missing init never crashes a consumer.
*
**********************************************************************************************/
#include "entities/entity_catalogue.h"

#include <stdbool.h>
#include <stdio.h> // snprintf

#include "filesystem/filesystem.h" // FS_ListDirectoryFiles / FS_FreeFileList / FS_FileStem
#include "entities/entity_prefab.h" // EntityPrefab_LoadFile
#include "collections/linear_array.h" // LArray and its API
#include "common/common.h" // LOG_ERROR

// ============================================================================
// Catalogue storage (module-owned)
// ============================================================================

// Flat array of CatalogueEntry value structs. Backed by a single heap buffer (the LArray's
// items block); shallow-dispose because the entries own no further heap storage.
static LArray g_catalogue = {0};
// Guards the one-shot enumeration (InitEntityCatalogue) and the one-shot dispose.
static bool g_initialised = false;

// ============================================================================
// Lifecycle
// ============================================================================

// Enumerate prefabs/*.json once and parse each into a flat catalogue entry. The directory is the
// SAME relative "prefabs" path the popup loads already use, and that CMake copies beside the exe,
// so enumeration resolves at runtime from the working directory. A prefab that fails to parse is
// logged and skipped so one bad file does not sink the rest of the catalogue.
void InitEntityCatalogue(void)
{
    if (g_initialised)
    {
        return; // Idempotent: load exactly once.
    }

    g_catalogue = MakeLArray(8, sizeof(CatalogueEntry));

    // Custom enumeration: non-recursive, filtered to .json. The returned list is heap-owned and
    // released with FS_FreeFileList inside this function. "prefabs" resolves relative to the
    // runtime working directory, exactly as the raylib enumeration did.
    FS_FileList files = FS_ListDirectoryFiles("prefabs", ".json");
    for (int i = 0; i < files.count; i++)
    {
        CatalogueEntry entry = {0};
        if (!EntityPrefab_LoadFile(files.paths[i], &entry.params))
        {
            // Skip a malformed prefab; keep the remainder of the catalogue usable.
            LOG_ERROR("InitEntityCatalogue: failed to parse prefab '%s'; skipping", files.paths[i]);
            continue;
        }

        // Derive the id from the file stem (e.g. gear_0.json -> "gear_0"), written straight into
        // the fixed buffer so the entry retains no pointer into the owned path list. The stem
        // equals the authored prefab_id in the existing files; the loader does not return the
        // JSON id.
        FS_FileStem(files.paths[i], entry.prefab_id, sizeof(entry.prefab_id));

        LArray_Push(&g_catalogue, &entry);
    }
    FS_FreeFileList(&files); // Release the path list immediately (nothing retained from it).

    g_initialised = true;
}

// Release the catalogue's backing storage. Shallow dispose (no per-entry cleanup) because the
// entries own no heap buffer. Idempotent: a second call is a no-op.
void DisposeEntityCatalogue(void)
{
    if (!g_initialised)
    {
        return;
    }
    DisposeLArray(&g_catalogue); // Frees the internal buffer; the stack struct remains.
    g_catalogue = (LArray){0};   // Clear so a later re-init starts from a clean state.
    g_initialised = false;
}

// ============================================================================
// Accessors
// ============================================================================

// Number of prefabs in the catalogue (0 when empty or not yet initialised).
int Catalogue_Count(void)
{
    return LArray_Count(&g_catalogue);
}

// Prefab id for entry i, or "" when i is out of range. Never NULL.
const char *Catalogue_GetId(int i)
{
    if (i < 0 || i >= Catalogue_Count())
    {
        return "";
    }
    const CatalogueEntry *entry = (const CatalogueEntry *)LArray_Get(&g_catalogue, i);
    return entry ? entry->prefab_id : "";
}

// Creation params for entry i, or NULL when i is out of range or the catalogue is empty. Borrowed
// pointer into catalogue storage; the caller copies it before spawn.
const EntityCreateParams *Catalogue_GetParams(int i)
{
    if (i < 0 || i >= Catalogue_Count())
    {
        return NULL;
    }
    const CatalogueEntry *entry = (const CatalogueEntry *)LArray_Get(&g_catalogue, i);
    return entry ? &entry->params : NULL;
}
