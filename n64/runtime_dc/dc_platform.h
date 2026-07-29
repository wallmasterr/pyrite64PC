/**
 * Dreamcast platform: load rom:/p64/... from /cd/p64 or /rd/p64.
 * Implements the same C ABI as the PC runtime (p64_pc_*) so engine PLATFORM_PC paths work.
 */
#ifndef P64_DC_PLATFORM_H
#define P64_DC_PLATFORM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void p64_pc_set_project_path(const char* path);
int p64_pc_discover_project_path(char* path_io, size_t path_cap);
const char* p64_pc_get_project_path(void);
void* p64_pc_asset_load(const char* path, unsigned long* size_out);
void p64_pc_trace(const char* step);
void p64_pc_alert_asset_missing(const char* rom_style_path);
void p64_pc_get_clear_color_rgba8(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);
void p64_pc_get_display_buffer(unsigned char** out_ptr, int* out_w, int* out_h, int* out_stride);

#ifdef __cplusplus
}
#endif

#endif /* P64_DC_PLATFORM_H */
