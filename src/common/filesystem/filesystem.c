/**********************************************************************************************
*
FILESYSTEM UTILITY MODULE (implementation)
*
Implements the custom file IO helpers declared in filesystem/filesystem.h using standard C stdio
(file-text reading) and POSIX dirent.h (directory enumeration). Owned heap buffers are allocated
and released through the project memory wrappers (AllocateBytes / ReallocateBytes / Deallocate),
so every allocation stays inside the existing byte-tracking system. The free paths pass 0 for the
Deallocate byte count: cmemory frees the block and NULLs the pointer regardless of the count
(which only feeds diagnostic counters), and the public free signatures do not carry a size.
*
**********************************************************************************************/
#include "filesystem/filesystem.h"

#include <stdio.h>  // fopen / fseek / ftell / fread / fclose / snprintf
#include <string.h> // strlen / strrchr
#include <dirent.h> // opendir / readdir / closedir

#include "memory/cmemory.h" // AllocateBytes / ReallocateBytes / Deallocate

// ============================================================================
// File-text reader
// ============================================================================

// Read an entire text file into a freshly allocated, NUL-terminated heap buffer. Opens in binary
// mode so the ftell size matches the fread byte count exactly (no CRLF translation on Windows).
// Returns NULL on any failure. The reader deliberately does not log: callers attach context.
// OWNERSHIP: caller owns the returned buffer and must release it with FS_FreeFileText().
char *FS_ReadFileText(const char *path)
{
    // Guard: a NULL path has nothing to read.
    if (!path)
    {
        return NULL;
    }

    // Open in binary mode so the byte count from ftell matches fread on Windows.
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return NULL;
    }

    // Size the file by seeking to the end and reading the offset, then rewind.
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0)
    {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    // Allocate length + 1 so there is always room for the terminating '\0', even for an empty
    // file (AllocateBytes zero-initialises and never returns NULL for a >= 1 byte request).
    char *buffer = (char *)AllocateBytes((size_t)len + 1);

    // Read the whole file; a short read is treated as failure so callers never see partial data.
    size_t read = fread(buffer, 1, (size_t)len, f);
    if (read != (size_t)len)
    {
        Deallocate((void **)&buffer, 0);
        fclose(f);
        return NULL;
    }

    // NUL-terminate so the buffer is safe to hand straight to cJSON_Parse.
    buffer[len] = '\0';

    fclose(f);
    return buffer;
}

// Release a buffer returned by FS_ReadFileText. Safe to call with NULL (Deallocate tolerates it).
// OWNERSHIP: transfers the buffer back to the allocator and clears the caller's reference.
void FS_FreeFileText(char *text)
{
    // Byte count is 0: cmemory frees and NULLs regardless; the public signature carries no size.
    Deallocate((void **)&text, 0);
}

// ============================================================================
// Directory enumeration
// ============================================================================

// List files in `dir` whose name ends with `extension` (case-sensitive), non-recursive. Each
// returned path is dir + "/" + name. Returns { NULL, 0 } when the directory cannot be opened or
// contains no match.
// OWNERSHIP: caller owns the result and must release it with FS_FreeFileList().
FS_FileList FS_ListDirectoryFiles(const char *dir, const char *extension)
{
    FS_FileList list = {0};

    // Guard: a NULL directory yields an empty list.
    if (!dir)
    {
        return list;
    }

    // opendir resolves `dir` relative to the runtime working directory, matching raylib's
    // LoadDirectoryFilesEx. A failure to open yields an empty list (zero catalogue iterations).
    DIR *d = opendir(dir);
    if (!d)
    {
        return list;
    }

    // Grow a heap array of owned path strings by doubling from an initial capacity of 8, so the
    // common small directory needs a single allocation and no second pass.
    int capacity = 8;
    char **paths = (char **)AllocateBytes((size_t)capacity * sizeof(char *));
    int count = 0;

    // Only apply an extension filter when a non-empty extension is supplied.
    const size_t ext_len = (extension && extension[0] != '\0') ? strlen(extension) : 0;
    const size_t dir_len = strlen(dir);

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        const char *name = entry->d_name;

        // Skip the current- and parent-directory entries.
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
        {
            continue;
        }

        // Case-sensitive suffix match against the requested extension (name-based, like raylib).
        if (ext_len > 0)
        {
            const size_t name_len = strlen(name);
            if (name_len < ext_len || strcmp(name + (name_len - ext_len), extension) != 0)
            {
                continue;
            }
        }

        // Build the full path as dir + "/" + name into an exactly-sized heap buffer.
        const size_t name_len = strlen(name);
        const size_t path_bytes = dir_len + 1 + name_len + 1; // dir + '/' + name + '\0'
        char *path = (char *)AllocateBytes(path_bytes);
        snprintf(path, path_bytes, "%s/%s", dir, name);

        // Grow the array by doubling when full before pushing the new path.
        if (count == capacity)
        {
            const int new_capacity = capacity * 2;
            paths = (char **)ReallocateBytes(paths,
                                             (size_t)capacity * sizeof(char *),
                                             (size_t)new_capacity * sizeof(char *));
            capacity = new_capacity;
        }

        paths[count] = path;
        count++;
    }

    closedir(d);

    // No matches: release the empty array and return a { NULL, 0 } list.
    if (count == 0)
    {
        Deallocate((void **)&paths, 0);
        return list;
    }

    list.paths = paths;
    list.count = count;
    return list;
}

// Release every path string and the paths array of a list, then zero the struct. Safe on a
// { NULL, 0 } list (the loop does not run and Deallocate tolerates NULL).
// OWNERSHIP: transfers all list storage back to the allocator.
void FS_FreeFileList(FS_FileList *list)
{
    // Guard: nothing to free for a NULL handle.
    if (!list)
    {
        return;
    }

    // Free each owned path string, then the array itself.
    for (int i = 0; i < list->count; i++)
    {
        Deallocate((void **)&list->paths[i], 0);
    }
    Deallocate((void **)&list->paths, 0);

    // Reset the struct so a disposed list reads as empty.
    *list = (FS_FileList){0};
}

// ============================================================================
// Path-stem helper
// ============================================================================

// Write the filename of `path` without its directory and without its extension into `out`,
// NUL-terminated and truncated to out_size. Pure string work into a caller buffer.
// OWNERSHIP: no allocation; the caller owns `out`.
void FS_FileStem(const char *path, char *out, size_t out_size)
{
    // Nothing to write without an output buffer.
    if (out_size == 0)
    {
        return;
    }

    // Default to an empty result; covers NULL/empty path and short-circuits the edge cases.
    out[0] = '\0';
    if (!path || path[0] == '\0')
    {
        return;
    }

    // Find the base name: the character just after the last '/' or '\\' (or the whole string).
    const char *base = path;
    for (const char *p = path; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }

    // An empty base (trailing slash, e.g. "prefabs/") has no stem.
    if (base[0] == '\0')
    {
        return;
    }

    // Find the extension: the last '.' within the base, but only when it is NOT the first
    // character (so a dotfile such as ".config" keeps its whole name).
    const char *dot = NULL;
    for (const char *p = base; *p != '\0'; p++)
    {
        if (*p == '.' && p != base)
        {
            dot = p;
        }
    }

    // Stem length is up to the extension dot, or the whole base when there is no valid extension.
    size_t stem_len = dot ? (size_t)(dot - base) : strlen(base);

    // Truncate to fit the output buffer, leaving room for the terminator.
    if (stem_len > out_size - 1)
    {
        stem_len = out_size - 1;
    }

    memcpy(out, base, stem_len);
    out[stem_len] = '\0';
}
