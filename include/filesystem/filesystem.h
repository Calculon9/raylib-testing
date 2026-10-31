/**********************************************************************************************
*
FILESYSTEM UTILITY MODULE
*
Small, self-contained file IO helper built on portable POSIX dirent.h (directory enumeration)
and standard C stdio (file-text reading). It replaces the raylib filesystem calls previously
used by prefab loading and the prefab catalogue, so those subsystems no longer depend on raylib
for file IO. Heap buffers are routed through the project memory wrappers (memory/cmemory.h),
keeping these allocations inside the existing byte-tracking system.
*
**********************************************************************************************/
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h> // size_t

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------

// Owned result of a directory scan: a heap array of heap-allocated full-path strings plus their
// count. An empty or failed scan yields { NULL, 0 }.
typedef struct FS_FileList
{
    char **paths; // Array of owned NUL-terminated path strings (dir + "/" + name).
    int    count; // Number of valid entries in paths.
} FS_FileList;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------

// Read an entire text file into a freshly allocated, NUL-terminated heap buffer. The buffer is
// sized file_length + 1 with a trailing '\0', so the result is safe to pass straight to
// cJSON_Parse. Returns NULL on any failure (cannot open, cannot size, short read). This is a
// drop-in replacement for raylib's LoadFileText.
// OWNERSHIP: caller owns the returned buffer and must release it with FS_FreeFileText().
char *FS_ReadFileText(const char *path);

// Release a buffer returned by FS_ReadFileText. Safe to call with NULL (no-op).
// OWNERSHIP: transfers the buffer back to the allocator and clears the caller's reference.
void FS_FreeFileText(char *text);

// List files in `dir` whose name ends with `extension` (e.g. ".json"), non-recursive. The match
// is case-sensitive. Each returned path is the directory joined to the entry name with a forward
// slash (dir + "/" + name). Pass extension as NULL or "" to match every regular entry. Returns
// { NULL, 0 } when the directory cannot be opened or contains no match. `dir` resolves relative
// to the runtime working directory, exactly as raylib's LoadDirectoryFilesEx did.
// OWNERSHIP: caller owns the result and must release it with FS_FreeFileList().
FS_FileList FS_ListDirectoryFiles(const char *dir, const char *extension);

// Release every path string and the paths array of a list, then zero the struct. Safe to call on
// a { NULL, 0 } list.
// OWNERSHIP: transfers all list storage back to the allocator.
void FS_FreeFileList(FS_FileList *list);

// Write the filename of `path` without its directory and without its extension into the
// caller-provided buffer `out` (NUL-terminated, truncated to out_size). For example
// "prefabs/gear_0.json" becomes "gear_0". A path with no extension copies the whole filename; a
// leading-dot dotfile (e.g. ".config") is treated as having no extension and is copied whole.
// `out` is set to "" when path is NULL/empty or out_size is 0.
// OWNERSHIP: no allocation; the caller owns `out`.
void FS_FileStem(const char *path, char *out, size_t out_size);

#endif // FILESYSTEM_H
