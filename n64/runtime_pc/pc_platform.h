/**
 * PC platform layer for Pyrite64 runtime.
 * Provides project path and asset loading from project_path/filesystem/p64/
 * so engine code can use asset_load("rom:/p64/...")-style paths on PC.
 */
#ifndef P64_PC_PLATFORM_H
#define P64_PC_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Set the project root path (e.g. from P64_PROJECT_PATH or argv[1]). */
void p64_pc_set_project_path(const char* path);

/**
 * If filesystem/p64/conf is missing at path_io, try:
 *   - p64_project_root.txt next to the executable (written by the editor PC build)
 *   - parent folder of the executable (exe in .../build-pc/, assets in .../filesystem/)
 * path_io: in/out buffer (mutable). path_cap: size including NUL.
 * Returns 1 if conf exists at the final path, else 0 (path_io may be unchanged).
 */
int p64_pc_discover_project_path(char* path_io, size_t path_cap);

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

/**
 * Windows: MessageBox with disk path for a failed rom:/p64/... load (e.g. missing scene blob).
 * No-op on other platforms. Safe to call from engine when PLATFORM_PC.
 */
void p64_pc_alert_asset_missing(const char* rom_style_path);

/**
 * Get the current scene clear color (RGBA 0-255). Call after p64_engine_run_frame.
 * If no scene is loaded, writes 0,0,0,255. Pointers must be non-NULL.
 */
void p64_pc_get_clear_color_rgba8(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);

/**
 * Get the engine display buffer (RGBA8, 640x480). Call after p64_engine_run_frame.
 * If no buffer, out_ptr is set to NULL and dimensions to 0. Non-NULL pointers required.
 */
void p64_pc_get_display_buffer(unsigned char** out_ptr, int* out_w, int* out_h, int* out_stride);

#ifdef __cplusplus
}
#endif

#endif /* P64_PC_PLATFORM_H */
