/**
 * PC platform layer for Pyrite64 runtime.
 * Provides project path and asset loading from project_path/filesystem/p64/
 * so engine code can use asset_load("rom:/p64/...")-style paths on PC.
 */
#ifndef P64_PC_PLATFORM_H
#define P64_PC_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/** Set the project root path (e.g. from P64_PROJECT_PATH or argv[1]). */
void p64_pc_set_project_path(const char* path);

/** Get the current project path. Returns "" if not set. */
const char* p64_pc_get_project_path(void);

/**
 * Load an asset from the project's filesystem/p64 directory.
 * path: same as on N64, e.g. "rom:/p64/a" or "rom:/p64/s0001o" — the "rom:/p64/" prefix
 *       is stripped and the rest is used under project_path/filesystem/p64/.
 * size_out: optional; if non-NULL, receives the file size in bytes.
 * Returns: malloc'd buffer with file contents, or NULL on failure. Caller must free().
 */
void* p64_pc_asset_load(const char* path, unsigned long* size_out);

/** Append one line to p64_pc_trace.log (for crash diagnosis; last line = step before crash). */
void p64_pc_trace(const char* step);

#ifdef __cplusplus
}
#endif

#endif /* P64_PC_PLATFORM_H */
