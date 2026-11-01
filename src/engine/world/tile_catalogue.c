/**********************************************************************************************
*
TILE CATALOGUE MODULE (implementation)
*
Enumerates tiles/*.json once at boot and parses each file into a flat LArray of TileDefinition
value structs. Index 0 is a reserved grid-default sentinel (BLANK fill, no texture) so the public
palette accessors keep the historic "tile_type 0 => use the grid's own colour_fill" contract
bit-for-bit; authored tile files load from index 1 upward. Storage is a single file-local value
LArray; disposal is shallow (ClearLArray once) because a tile definition owns no heap buffer (a
texture_path is consumed into a TextureId at load time and never retained). The accessors degrade
gracefully (empty catalogue => count 0, default_colour / INVALID_TEXTURE_ID) so a missing init
never crashes a consumer.
*
**********************************************************************************************/
#include "world/tile_catalogue.h"
#include "world/world.h" // palette accessor prototypes (TilePalette_TypeCount / _ColourForType / _TextureForType)

#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <string.h> // qsort comparator string compare

#include "cJSON.h"
#include "filesystem/filesystem.h"   // FS_ListDirectoryFiles / FS_FreeFileList / FS_FileStem / FS_ReadFileText
#include "collections/linear_array.h" // LArray and its API
#include "system/texture_registry.h"  // TextureRegistry_LoadOrGet
#include "common/common.h"            // LOG_ERROR / LOG_WARN / BLANK_RGBA

// ============================================================================
// Catalogue storage (module-owned)
// ============================================================================

// Flat array of TileDefinition value structs. Backed by a single heap buffer (the LArray's items
// block); shallow-dispose because the entries own no further heap storage.
static LArray g_tile_catalogue = {0};
// Guards the one-shot enumeration (InitTileCatalogue) and the one-shot dispose.
static bool g_initialised = false;

// ============================================================================
// JSON parsing
// ============================================================================

// Read one 0..255 integer colour channel from a cJSON array element. Rejects non-numbers,
// non-integral values and out-of-range values so a present-but-malformed channel fails the whole
// definition (mirrors the prefab loader's tint parsing).
static bool ReadColourChannel(const cJSON *value, unsigned char *out_channel)
{
    if (!cJSON_IsNumber(value) || !out_channel || !isfinite(value->valuedouble) ||
        value->valuedouble < 0.0 || value->valuedouble > 255.0)
    {
        return false;
    }

    int as_int = (int)value->valuedouble;
    // Reject non-integral channel values (e.g. 12.5) so colours stay whole.
    if ((double)as_int != value->valuedouble)
    {
        return false;
    }

    *out_channel = (unsigned char)as_int;
    return true;
}

// Parse a single tile definition document into the output struct. Validates schema_version == 1, a
// required four-element 0..255 fill_colour array, and an optional non-empty texture_path string
// (resolved to a TextureId here, after TextureRegistry_Init). Any present-but-malformed value is a
// hard parse failure (reject the whole definition). The name is NOT read from JSON — the file stem
// is the authoritative id and is written by the caller.
static bool TileDefinition_Decode(const cJSON *root, TileDefinition *out_definition)
{
    if (!cJSON_IsObject(root) || !out_definition)
    {
        return false;
    }

    // schema_version: required, must equal 1 (matches the prefab loader's version gate).
    const cJSON *schema_version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!cJSON_IsNumber(schema_version) || schema_version->valuedouble != 1.0)
    {
        return false;
    }

    // fill_colour: required four-element [r, g, b, a] array of 0..255 integers.
    const cJSON *fill_colour = cJSON_GetObjectItemCaseSensitive(root, "fill_colour");
    if (!cJSON_IsArray(fill_colour) || cJSON_GetArraySize(fill_colour) != 4)
    {
        return false;
    }

    unsigned char channels[4] = {0, 0, 0, 0};
    for (int c = 0; c < 4; c++)
    {
        if (!ReadColourChannel(cJSON_GetArrayItem(fill_colour, c), &channels[c]))
        {
            return false;
        }
    }

    TileDefinition result = {0};
    result.fill_colour = (ColourRgba){
        .r = channels[0],
        .g = channels[1],
        .b = channels[2],
        .a = channels[3]};
    result.texture_id = INVALID_TEXTURE_ID; // Default: no texture unless a valid path resolves.

    // texture_path: optional non-empty string. Absent => colour fill only. A present-but-non-string
    // value is a parse error (reject the whole definition). When present and valid, resolve it to a
    // TextureId now (TextureRegistry_LoadOrGet is load-once). A failed load leaves texture_id at
    // INVALID_TEXTURE_ID so the cell gracefully falls back to its colour fill; the definition is
    // still accepted (it contributes a valid colour and a palette slot).
    const cJSON *texture_path = cJSON_GetObjectItemCaseSensitive(root, "texture_path");
    if (texture_path)
    {
        const char *path_text = cJSON_GetStringValue(texture_path);
        if (!path_text || path_text[0] == '\0')
        {
            return false; // present-but-malformed (non-string or empty) fails the definition.
        }
        result.texture_id = TextureRegistry_LoadOrGet(path_text);
    }

    *out_definition = result;
    return true;
}

// Load and parse one tile-definition JSON file into the output struct. Leaves the output untouched
// on failure. Mirrors EntityPrefab_LoadFile's read/parse/free flow.
static bool TileDefinition_LoadFile(const char *file_path, TileDefinition *out_definition)
{
    if (!file_path || !out_definition)
    {
        return false;
    }

    char *file_text = FS_ReadFileText(file_path);
    if (!file_text)
    {
        LOG_WARN("Cannot read tile definition: %s\n", file_path);
        return false;
    }

    cJSON *root = cJSON_Parse(file_text);
    bool is_valid = TileDefinition_Decode(root, out_definition);
    if (root)
    {
        cJSON_Delete(root);
    }
    FS_FreeFileText(file_text);

    if (!is_valid)
    {
        LOG_WARN("Invalid or unsupported tile definition: %s\n", file_path);
    }

    return is_valid;
}

// qsort comparator giving the enumerated file list a deterministic order by full path. readdir
// order is not guaranteed sorted; sorting keeps the authored 1..N tile indices predictable across
// runs/platforms. Allocation-free (compares the borrowed path strings in place).
static int CompareFilePaths(const void *left, const void *right)
{
    const char *const *a = (const char *const *)left;
    const char *const *b = (const char *const *)right;
    return strcmp(*a, *b);
}

// ============================================================================
// Lifecycle
// ============================================================================

// Enumerate tiles/*.json once and parse each into a tile definition. Index 0 is reserved first as
// the grid-default sentinel so the palette accessors keep the "tile_type 0 => grid colour_fill"
// contract. The directory resolves relative to the runtime working directory (same as "prefabs",
// and CMake copies it beside the exe). A file that fails to parse is logged and skipped so one bad
// file does not sink the rest of the catalogue. MUST run after TextureRegistry_Init() because a
// definition's optional texture_path is resolved to a TextureId here.
void InitTileCatalogue(void)
{
    if (g_initialised)
    {
        return; // Idempotent: load exactly once.
    }

    g_tile_catalogue = MakeLArray(8, sizeof(TileDefinition));

    // Reserve index 0 as the grid-default sentinel: a BLANK fill (never read) and no texture. This
    // keeps index 0 meaning "use the grid's own colour / no texture", identical to the previous
    // hardcoded placeholder slot, so TilePalette_TypeCount() counts it and the UI default active
    // type of 1 still lands on the first authored tile.
    TileDefinition grid_default = {0};
    grid_default.name[0] = '\0';
    grid_default.fill_colour = BLANK_RGBA;
    grid_default.texture_id = INVALID_TEXTURE_ID;
    LArray_Push(&g_tile_catalogue, &grid_default);

    // Custom enumeration: non-recursive, filtered to .json. The returned list is heap-owned and
    // released with FS_FreeFileList inside this function. "tiles" resolves relative to the runtime
    // working directory, exactly as "prefabs" does.
    FS_FileList files = FS_ListDirectoryFiles("tiles", ".json");

    // Sort the file list so authored tiles get a deterministic 1..N index order regardless of the
    // platform's readdir order.
    if (files.count > 1 && files.paths)
    {
        qsort(files.paths, (size_t)files.count, sizeof(files.paths[0]), CompareFilePaths);
    }

    for (int i = 0; i < files.count; i++)
    {
        TileDefinition definition = {0};
        if (!TileDefinition_LoadFile(files.paths[i], &definition))
        {
            // Skip a malformed tile definition; keep the remainder of the catalogue usable.
            LOG_ERROR("InitTileCatalogue: failed to parse tile '%s'; skipping", files.paths[i]);
            continue;
        }

        // Derive the id from the file stem (e.g. terracotta.json -> "terracotta"), written straight
        // into the fixed buffer so the entry retains no pointer into the owned path list.
        FS_FileStem(files.paths[i], definition.name, sizeof(definition.name));

        LArray_Push(&g_tile_catalogue, &definition);
    }
    FS_FreeFileList(&files); // Release the path list immediately (nothing retained from it).

    g_initialised = true;
}

// Release the catalogue's backing storage. Shallow dispose (no per-entry cleanup) because the
// entries own no heap buffer. Idempotent: a second call is a no-op.
void DisposeTileCatalogue(void)
{
    if (!g_initialised)
    {
        return;
    }
    // g_tile_catalogue is a file-local VALUE struct (made with MakeLArray), not a heap-allocated
    // LArray from AllocLArray. DisposeLArray() would free() the static struct address itself —
    // undefined behaviour — so use ClearLArray(), which releases only the internal buffer and
    // leaves the struct intact (matching entity_catalogue.c and every other value LArray teardown).
    ClearLArray(&g_tile_catalogue); // Frees the internal buffer; the static struct remains.
    g_tile_catalogue = (LArray){0}; // Clear so a later re-init starts from a clean state.
    g_initialised = false;
}

// ============================================================================
// Public palette accessors (declared in world/world.h)
// ============================================================================

// Return the number of tile definitions in the catalogue (valid tile_type values are 0..count-1).
// This includes the reserved index-0 grid-default entry, so a loaded catalogue returns
// 1 + number_of_authored_files. 0 when never initialised — the UI clamp already treats < 1 as the
// degenerate fallback, so a missing init never crashes. Single source of truth for the active-tile
// clamp/wrap.
int TilePalette_TypeCount(void)
{
    return LArray_Count(&g_tile_catalogue);
}

// Resolve a cell's tile_type to its fill colour. Index 0 (the grid-default sentinel) and any
// out-of-range index fall back to default_colour — the grid's own colour_fill passed in by the
// caller — reproducing the previous hardcoded contract verbatim. Bounds-safe by construction.
ColourRgba TilePalette_ColourForType(int tile_type, ColourRgba default_colour)
{
    int count = TilePalette_TypeCount();
    if (tile_type <= 0 || tile_type >= count)
    {
        return default_colour; // 0 = grid default; out-of-range = safe fallback to the grid colour.
    }
    const TileDefinition *definition = (const TileDefinition *)LArray_Get(&g_tile_catalogue, tile_type);
    return definition ? definition->fill_colour : default_colour;
}

// Resolve a cell's tile_type to its texture handle, or INVALID_TEXTURE_ID when it has none. Index 0
// (grid-default) and any out-of-range index always report "no texture", matching the colour
// contract, so callers fall back to the colour-fill path for those cells.
TextureId TilePalette_TextureForType(int tile_type)
{
    int count = TilePalette_TypeCount();
    if (tile_type <= 0 || tile_type >= count)
    {
        return INVALID_TEXTURE_ID; // 0 = grid default; out-of-range = no texture.
    }
    const TileDefinition *definition = (const TileDefinition *)LArray_Get(&g_tile_catalogue, tile_type);
    return definition ? definition->texture_id : INVALID_TEXTURE_ID;
}
