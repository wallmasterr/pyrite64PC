/**
 * PC stubs for libdragon / tiny3d symbols so the engine links and runs on PC.
 * asset_load delegates to p64_pc_asset_load; most others are no-ops or minimal.
 * Include libdragon before Windows so libdragon's ERROR_BAD_COMMAND/_IO are not clobbered.
 */
#ifdef PLATFORM_PC
#include "pc_platform.h"
#include <pc_compat.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

/* Include libdragon before Windows to avoid macro conflicts (ERROR_BAD_COMMAND, _IO) */
#include <libdragon.h>

#ifdef _WIN32
#include <windows.h>
#undef near
#undef far
#else
#include <SDL3/SDL.h>
#endif

/* --- N64 system / timer (n64sys.h) --- */
/* __boot_* are declared in n64sys.h without extern "C", so define with C++ linkage */
int __boot_memsize = 8;
int __boot_consoletype = 0;
int __boot_tvtype = 0;

extern "C" {
static uint64_t pc_ticks_base = 0;
uint64_t get_ticks(void) {
#ifdef _WIN32
  LARGE_INTEGER c, f;
  if (QueryPerformanceCounter(&c) && QueryPerformanceFrequency(&f) && f.QuadPart)
    return pc_ticks_base + (uint64_t)((c.QuadPart * 1000000) / f.QuadPart);
  return pc_ticks_base + (uint64_t)GetTickCount64() * 1000;
#else
  return pc_ticks_base + (uint64_t)SDL_GetTicks() * 1000;
#endif
}
uint64_t get_ticks_us(void) { return get_ticks(); }
uint64_t get_ticks_ms(void) { return get_ticks() / 1000; }

reset_type_t sys_reset_type(void) { return RESET_COLD; }

uint64_t get_user_ticks(void) { return get_ticks(); }

bool is_memory_expanded(void) { return true; }
}

/* --- __mi_memset32 (N64 MI fill; len is bytes, must be multiple of 4) --- */
extern "C" void* __mi_memset32(void* ptr, uint32_t value, size_t len) {
  uint32_t* p = static_cast<uint32_t*>(ptr);
  for (size_t n = len / 4; n > 0; --n)
    *p++ = value;
  return ptr;
}

/* --- Asset (asset.h) --- */
extern "C" {
void __asset_init_compression_lvl2(void) {}
void __asset_init_compression_lvl3(void) {}

void* asset_load(const char* fn, int* sz) {
  unsigned long size = 0;
  void* p = p64_pc_asset_load(fn, &size);
  if (sz && p) *sz = (int)size;
  return p;
}
}

/* --- DragonFS --- */
extern "C" int dfs_init(uint32_t) { return 0; }

/* --- Interrupts (no-op on PC) --- */
extern "C" void enable_interrupts(void) {}
extern "C" void disable_interrupts(void) {}

/* --- sys_hw_memset16 (e.g. MiniMap.cpp init); PC: fill 16-bit words with value --- */
extern "C" void* sys_hw_memset16(void* ptr, uint16_t value, size_t len) {
  std::uint16_t* p = static_cast<std::uint16_t*>(ptr);
  while (len--) *p++ = value;
  return ptr;
}

/* --- sys_hw_memset (e.g. Player.cpp init); PC: byte memset --- */
extern "C" void* sys_hw_memset(void* ptr, uint8_t value, size_t len) {
  std::memset(ptr, static_cast<int>(value), len);
  return ptr;
}

/* --- N64 cache ops (e.g. globalSetup.cpp); no-op on PC --- */
extern "C" void data_cache_hit_writeback(volatile const void* addr, unsigned long size) {
  (void)addr;
  (void)size;
}
extern "C" void data_cache_hit_writeback_invalidate(volatile void* addr, unsigned long size) {
  (void)addr;
  (void)size;
}

extern "C" size_t malloc_usable_size(void* ptr) {
  (void)ptr;
  return 0;
}

/* --- Debug: debug_init_isviewer, debug_init_usblog, debugf are macros in libdragon debug.h when N64_DEBUG is off --- */

/* --- Mixer (no-op on PC) --- */
extern "C" void mixer_try_play(void) {}

/* --- RDPQ (no-op stubs; do not redefine inlines from rdpq_mode.h / rdpq.h) --- */
extern "C" {
void rdpq_init(void) {}
void rdpq_close(void) {}
void rdpq_wait(void) {}
void rdpq_sync_pipe(void) {}
void rdpq_sync_tile(void) {}
void rdpq_sync_load(void) {}
void rdpq_set_mode_standard(void) {}
/* rdpq_mode_antialias is inline in rdpq_mode.h */
/* Minimal uploaded tile state for PC blitting path. */
struct PcTileTex {
  const void* pixels;
  tex_format_t fmt;
  int width;
  int height;
  int stride;
  int palette_base;
  bool valid;
};
static PcTileTex s_pc_tile_tex[8]{};
static uint16_t s_pc_tlut[256]{};

static inline bool pc_fmt_supported(tex_format_t fmt) {
  switch (fmt) {
    case FMT_RGBA32:
    case FMT_RGBA16:
    case FMT_I8:
    case FMT_I4:
    case FMT_IA8:
    case FMT_IA4:
    case FMT_IA16:
    case FMT_CI8:
    case FMT_CI4:
      return true;
    default:
      return false;
  }
}

static inline bool pc_tex_sane(const PcTileTex& t) {
  if (!t.pixels || !pc_fmt_supported(t.fmt)) return false;
  if (t.width <= 0 || t.height <= 0 || t.width > 2048 || t.height > 2048) return false;
  const int minStride = (int)TEX_FORMAT_PIX2BYTES(t.fmt, t.width);
  if (t.stride < minStride || t.stride > 1 << 20) return false;
  return true;
}

static inline uint32_t pc_decode_texel_rgba(const PcTileTex& t, int s, int tt)
{
  if (!t.valid || !pc_tex_sane(t)) return 0;
  if (s < 0) s = 0; if (tt < 0) tt = 0;
  if (s >= t.width) s = t.width - 1;
  if (tt >= t.height) tt = t.height - 1;

  const uint8_t* row = (const uint8_t*)t.pixels + (size_t)tt * (size_t)t.stride;
  auto decode_rgba16 = [](uint16_t px) -> uint32_t {
    const uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
    const uint8_t g = (uint8_t)(((px >> 6) & 0x1F) * 255 / 31);
    const uint8_t b = (uint8_t)(((px >> 1) & 0x1F) * 255 / 31);
    const uint8_t a = (px & 1) ? 255 : 0;
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
  };
  switch (t.fmt) {
    case FMT_RGBA32: {
      const uint8_t* p = row + (size_t)s * 4u;
      return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    case FMT_RGBA16: {
      return decode_rgba16(((const uint16_t*)row)[s]);
    }
    case FMT_I8: {
      const uint8_t i = row[s];
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | 0xFF000000u;
    }
    case FMT_I4: {
      const uint8_t b = row[s >> 1];
      const uint8_t n = (s & 1) ? (b & 0x0F) : (b >> 4);
      const uint8_t i = (uint8_t)(n * 17);
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | 0xFF000000u;
    }
    case FMT_IA8: {
      const uint8_t v = row[s];
      const uint8_t i = (uint8_t)((v >> 4) * 17);
      const uint8_t a = (uint8_t)((v & 0xF) * 17);
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | ((uint32_t)a << 24);
    }
    case FMT_IA4: {
      const uint8_t b = row[s >> 1];
      const uint8_t n = (s & 1) ? (b & 0x0F) : (b >> 4);
      const uint8_t i = (uint8_t)(((n >> 1) & 0x7) * 255 / 7);
      const uint8_t a = (n & 1) ? 255 : 0;
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | ((uint32_t)a << 24);
    }
    case FMT_IA16: {
      const uint16_t v = ((const uint16_t*)row)[s];
      const uint8_t i = (uint8_t)(v >> 8);
      const uint8_t a = (uint8_t)(v & 0xFF);
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | ((uint32_t)a << 24);
    }
    case FMT_CI8: {
      uint32_t idx = (uint32_t)row[s] + (uint32_t)t.palette_base;
      if (idx > 255) idx = 255;
      return decode_rgba16(s_pc_tlut[idx]);
    }
    case FMT_CI4: {
      const uint8_t b = row[s >> 1];
      uint32_t idx = (uint32_t)((s & 1) ? (b & 0x0F) : (b >> 4)) + (uint32_t)t.palette_base;
      if (idx > 255) idx = 255;
      return decode_rgba16(s_pc_tlut[idx]);
    }
    default:
      return 0;
  }
}

int rdpq_sprite_upload(rdpq_tile_t tile, sprite_t* spr, const rdpq_texparms_t* parms) {
  if ((unsigned)tile >= 8u || !spr) return 0;
  PcTileTex& t = s_pc_tile_tex[(unsigned)tile];
  t.pixels = (const void*)spr->data;
  t.fmt = sprite_get_format(spr);
  t.width = (int)spr->width;
  t.height = (int)spr->height;
  t.stride = (int)TEX_FORMAT_PIX2BYTES(t.fmt, t.width);
  t.palette_base = (parms && t.fmt == FMT_CI4) ? (parms->palette * 16) : 0;
  t.valid = pc_tex_sane(t);
  if (!t.valid) { t.pixels = nullptr; return 0; }
  return 0;
}
void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1);
void __rdpq_texture_rectangle(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3) {
  /* Decode packed RDP TEXTURE_RECTANGLE command and forward to offline blitter. */
  int32_t x1 = (int32_t)((w0 >> 12) & 0xFFF);
  int32_t y1 = (int32_t)(w0 & 0xFFF);
  rdpq_tile_t tile = (rdpq_tile_t)((w1 >> 24) & 0x7);
  int32_t x0 = (int32_t)((w1 >> 12) & 0xFFF);
  int32_t y0 = (int32_t)(w1 & 0xFFF);

  int32_t s0 = (int16_t)((w2 >> 16) & 0xFFFF);
  int32_t t0 = (int16_t)(w2 & 0xFFFF);
  int32_t dsdx = (int16_t)((w3 >> 16) & 0xFFFF);
  int32_t dtdy = (int16_t)(w3 & 0xFFFF);

  int32_t dx = x1 - x0;
  int32_t dy = y1 - y0;
  int32_t s1 = s0;
  int32_t t1 = t0;
  if (dx != 0) s1 = s0 + (int32_t)(((int64_t)dx * (int64_t)dsdx) >> 7);
  if (dy != 0) t1 = t0 + (int32_t)(((int64_t)dy * (int64_t)dtdy) >> 7);
  __rdpq_texture_rectangle_scaled_offline(tile, x0, y0, x1, y1, s0, t0, s1, t1);
}
void rdpq_text_register_font(uint8_t font_id, const rdpq_font_t*) { (void)font_id; }
rdpq_font_t* rdpq_font_load(const char*) { return nullptr; }
void rdpq_font_free(rdpq_font_t*) {}
/* Return current attached surface for user scripts (e.g. rdpq_get_attached()->width); PC stub. */
static surface_t s_rdpq_attached = { FMT_RGBA32, 320, 240, 320 * 4, nullptr };
const surface_t* rdpq_get_attached(void) { return &s_rdpq_attached; }

/* Store attached color surface and current draw target (set_color_image) - must be before rdpq_set_color_image/rdpq_attach */
static const surface_t* s_pc_attached_color = nullptr;
static const surface_t* s_pc_current_color_image = nullptr;
static uint32_t s_pc_fill_color = 0xFF000000u; /* opaque black */
/* Pack color for GL RGBA (little-endian: R at low addr). Convert libdragon 0xRRGGBBAA to GL order. */
static inline uint32_t pc_pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}
static inline uint32_t pc_swap_to_rgba(uint32_t packed) {
  return (packed >> 24) | ((packed >> 8) & 0xFF00) | ((packed << 8) & 0xFF0000) | ((packed << 24) & 0xFF000000);
}
static inline uint16_t pc_rgba32_to_rgba16(uint32_t rgba) {
  const uint8_t r8 = (uint8_t)(rgba & 0xFF);
  const uint8_t g8 = (uint8_t)((rgba >> 8) & 0xFF);
  const uint8_t b8 = (uint8_t)((rgba >> 16) & 0xFF);
  const uint8_t a8 = (uint8_t)((rgba >> 24) & 0xFF);
  const uint16_t r5 = (uint16_t)((r8 * 31 + 127) / 255);
  const uint16_t g5 = (uint16_t)((g8 * 31 + 127) / 255);
  const uint16_t b5 = (uint16_t)((b8 * 31 + 127) / 255);
  const uint16_t a1 = (uint16_t)(a8 >= 128 ? 1 : 0);
  return (uint16_t)((r5 << 11) | (g5 << 6) | (b5 << 1) | a1);
}
static inline void pc_store_pixel(surface_t* surf, int x, int y, uint32_t rgba) {
  if (!surf || !surf->buffer) return;
  if (x < 0 || y < 0 || x >= (int)surf->width || y >= (int)surf->height) return;
  uint8_t* row = (uint8_t*)surf->buffer + (size_t)y * (size_t)surf->stride;
  tex_format_t fmt = surface_get_format(surf);
  if (fmt == FMT_RGBA16) {
    ((uint16_t*)row)[x] = pc_rgba32_to_rgba16(rgba);
  } else {
    ((uint32_t*)row)[x] = rgba;
  }
}

void rdpq_mode_begin(void) {}
void rdpq_mode_end(void) {}
void rdpq_mode_push(void) {}
void rdpq_mode_pop(void) {}
void rdpq_set_color_image(const surface_t* surf) { s_pc_current_color_image = surf; }
void rdpq_set_z_image(const surface_t*) {}
void rdpq_attach(const surface_t* color, const surface_t*) {
  s_pc_attached_color = color;
  s_pc_current_color_image = color;
}
void rdpq_detach_cb(void (*)(void*), void*) {}
void rdpq_tex_multi_begin(void) {}
int rdpq_tex_multi_end(void) { return 0; }
int rdpq_tex_upload(rdpq_tile_t tile, const surface_t* surf, const rdpq_texparms_t*) {
  if ((unsigned)tile >= 8u || !surf) return 0;
  PcTileTex& t = s_pc_tile_tex[(unsigned)tile];
  t.pixels = surf->buffer;
  t.fmt = surface_get_format(surf);
  t.width = (int)surf->width;
  t.height = (int)surf->height;
  t.stride = (int)surf->stride;
  t.palette_base = 0;
  t.valid = pc_tex_sane(t);
  if (!t.valid) { t.pixels = nullptr; return 0; }
  return 0;
}
int rdpq_tex_reuse(rdpq_tile_t, const rdpq_texparms_t*) { return 0; }
void rdpq_tex_upload_tlut(uint16_t *tlut, int color_idx, int num_colors) {
  if (!tlut || num_colors <= 0 || color_idx < 0 || color_idx > 255) return;
  int n = num_colors;
  if (color_idx + n > 256) n = 256 - color_idx;
  for (int i = 0; i < n; i++) s_pc_tlut[color_idx + i] = tlut[i];
}
void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1);
static void pc_blit_rect_from_tile(rdpq_tile_t tile, float x0, float y0, int w, int h) {
  if (w <= 0 || h <= 0) return;
  int32_t ix0 = (int32_t)(x0 * 4.0f);
  int32_t iy0 = (int32_t)(y0 * 4.0f);
  int32_t ix1 = ix0 + w * 4;
  int32_t iy1 = iy0 + h * 4;
  __rdpq_texture_rectangle_scaled_offline(tile, ix0, iy0, ix1, iy1, 0, 0, w * 32, h * 32);
}
void rdpq_tex_blit(const surface_t* surf, float x0, float y0, const rdpq_blitparms_t* parms) {
  if (!surf || !surf->buffer) return;
  rdpq_tile_t tile = parms ? parms->tile : TILE0;
  if ((unsigned)tile >= 8u) tile = TILE0;
  rdpq_tex_upload(tile, surf, nullptr);
  int w = parms && parms->width  > 0 ? parms->width  : (int)surf->width;
  int h = parms && parms->height > 0 ? parms->height : (int)surf->height;
  pc_blit_rect_from_tile(tile, x0, y0, w, h);
}
void rdpq_sprite_blit(sprite_t* spr, float x0, float y0, const rdpq_blitparms_t* parms) {
  if (!spr) return;
  rdpq_tile_t tile = parms ? parms->tile : TILE0;
  if ((unsigned)tile >= 8u) tile = TILE0;
  rdpq_sprite_upload(tile, spr, nullptr);
  int w = parms && parms->width  > 0 ? parms->width  : (int)spr->width;
  int h = parms && parms->height > 0 ? parms->height : (int)spr->height;
  pc_blit_rect_from_tile(tile, x0, y0, w, h);
}
const rdpq_font_t* rdpq_text_get_font(uint8_t) { return nullptr; }
void rdpq_font_style(rdpq_font_t*, uint8_t, const rdpq_fontstyle_t*) {}
rdpq_textmetrics_t rdpq_text_printn(const rdpq_textparms_t*, uint8_t, float, float, const char*, int) {
  rdpq_textmetrics_t m = {0,0,0,0}; return m;
}
rdpq_textmetrics_t rdpq_text_printf(const rdpq_textparms_t*, uint8_t, float, float, const char*, ...) {
  rdpq_textmetrics_t m = {0,0,0,0}; return m;
}

void __rdpq_fixup_mode(uint32_t, uint32_t, uint32_t) {}
void __rdpq_fixup_mode3(uint32_t, uint32_t, uint32_t, uint32_t) {}
void __rdpq_fixup_mode4(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void __rdpq_fixup_write8_syncchange(uint32_t, uint32_t, uint32_t, uint32_t) {}
void __rdpq_write8_syncchange(uint32_t, uint32_t, uint32_t, uint32_t) {}
void __rdpq_write8(uint32_t, uint32_t, uint32_t) {}
void __rdpq_set_scissor(uint32_t, uint32_t) {}
void __rdpq_set_mode_fill(void) {}
void __rdpq_set_fill_color(uint32_t packed) { s_pc_fill_color = packed; }
void __rdpq_fill_rectangle(uint32_t, uint32_t) {}
void __rdpq_fill_rectangle_offline(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  if (!s_pc_current_color_image || !s_pc_current_color_image->buffer) return;
  int px0 = x0 / 4, py0 = y0 / 4, px1 = x1 / 4, py1 = y1 / 4;
  if (px0 < 0) px0 = 0; if (py0 < 0) py0 = 0;
  int w = (int)s_pc_current_color_image->width, h = (int)s_pc_current_color_image->height;
  if (px1 > w) px1 = w; if (py1 > h) py1 = h;
  if (px0 >= px1 || py0 >= py1) return;
  int stride = s_pc_current_color_image->stride;
  uint8_t* base = (uint8_t*)s_pc_current_color_image->buffer;
  uint32_t rgba = pc_swap_to_rgba(s_pc_fill_color);
  tex_format_t fmt = surface_get_format(s_pc_current_color_image);
  for (int y = py0; y < py1; y++) {
    if (fmt == FMT_RGBA16) {
      uint16_t* row16 = (uint16_t*)(base + (size_t)y * (size_t)stride);
      uint16_t v16 = pc_rgba32_to_rgba16(rgba);
      for (int x = px0; x < px1; x++) row16[x] = v16;
    } else {
      uint32_t* row32 = (uint32_t*)(base + (size_t)y * (size_t)stride);
      for (int x = px0; x < px1; x++) row32[x] = rgba;
    }
  }
}
void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1);
void __rdpq_texture_rectangle_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0) {
  if ((unsigned)tile >= 8u) return;
  const PcTileTex& tex = s_pc_tile_tex[(unsigned)tile];
  if (!tex.valid || !pc_tex_sane(tex)) return;
  /* Non-scaled variant: bottom-right tex coords derive from destination size. */
  const int32_t s1 = s0 + (((x1 - x0) / 4) * 32);
  const int32_t t1 = t0 + (((y1 - y0) / 4) * 32);
  __rdpq_texture_rectangle_scaled_offline(tile, x0, y0, x1, y1, s0, t0, s1, t1);
}
}

/* --- RSPQ blocks and queue --- */
static uint32_t rspq_dummy_buf[64];
extern "C" {
volatile uint32_t* rspq_cur_pointer = rspq_dummy_buf;
volatile uint32_t* rspq_cur_sentinel = rspq_dummy_buf + 63;
void rspq_block_begin(void) {}
rspq_block_t* rspq_block_end(void) { return nullptr; }
void rspq_block_run(rspq_block_t*) {}
void rspq_next_buffer(void) {}
void rspq_flush(void) {}
uint32_t rspq_overlay_register(rsp_ucode_t*) { return 0; }
void rspq_overlay_unregister(uint32_t) {}
}

/* --- Display --- */
extern "C" {
uint32_t display_get_width(void) { return 320; }
uint32_t display_get_height(void) { return 240; }
}

/* --- Surface --- */
extern "C" {
surface_t surface_make_sub(surface_t* parent, uint16_t x0, uint16_t y0, uint16_t width, uint16_t height) {
  surface_t s = *parent;
  s.width = width;
  s.height = height;
  if (parent && parent->buffer) {
    tex_format_t fmt = surface_get_format(parent);
    size_t row_off = (size_t)y0 * (size_t)parent->stride;
    size_t col_off = (size_t)TEX_FORMAT_PIX2BYTES(fmt, x0);
    s.buffer = (uint8_t*)parent->buffer + row_off + col_off;
    /* keep parent stride; sub-surfaces are views into same row layout */
    s.stride = parent->stride;
  }
  return s;
}
}

/* --- Audio / mixer / wav64 --- */
extern "C" {
void audio_init(int, int) {}
void audio_close(void) {}
void mixer_init(int) {}
void mixer_close(void) {}
void mixer_ch_stop(int) {}
bool mixer_ch_playing(int) { return false; }
void mixer_ch_set_vol(int, float, float) {}
void mixer_ch_set_freq(int, float) {}
void wav64_play(wav64_t*, int) {}
void wav64_close(wav64_t*) {}
void wav64_set_loop(wav64_t*, bool) {}
}

/* --- Dir --- */
extern "C" {
int dir_findfirst(const char* const path, dir_t* dir) {
  (void)path;
  if (dir) { dir->d_name[0] = '\0'; dir->d_type = 0; dir->d_size = 0; dir->d_cookie = 0; }
  return -1;
}
int dir_findnext(const char* const path, dir_t* dir) {
  (void)path;
  if (dir) { dir->d_name[0] = '\0'; dir->d_type = 0; dir->d_size = 0; dir->d_cookie = 0; }
  return -1;
}
}

/* --- Asset --- */
#include <cstdio>
extern "C" {
FILE* asset_fopen(const char* fn, int* sz) {
  (void)fn;
  if (sz) *sz = 0;
  return nullptr;
}
}

/* --- N64 uncached heap / heap stats --- */
extern "C" {
void* malloc_uncached(size_t size) { return std::malloc(size); }
void free_uncached(void* p) { std::free(p); }
void sys_get_heap_stats(heap_stats_t* stats) {
  if (stats) { std::memset(stats, 0, sizeof(*stats)); }
}
}

/* --- VI --- */
extern "C" {
void vi_init(void) {}
void vi_set_dedither(int) {}
void vi_set_aa_mode(int) {}
void vi_set_interlaced(int) {}
void vi_set_divot(int) {}
void vi_set_gamma(int) {}
void vi_blank(int) {}
void vi_install_vblank_handler(void (*)(void*, void*), void*) {}
void vi_write_begin(void) {}
void vi_write_end(void) {}
void vi_show(surface_t*) {}
float vi_get_refresh_rate(void) { return 60.0f; }
}

/* --- fmath (libdragon fast math; PC uses standard sin/cos) --- */
extern "C" {
float fm_sinf(float x) { return std::sinf(x); }
float fm_cosf(float x) { return std::cosf(x); }
void fm_sincosf(float x, float* sin_out, float* cos_out) {
  if (sin_out) *sin_out = std::sinf(x);
  if (cos_out) *cos_out = std::cosf(x);
}
float fm_sinf_approx(float x, int) { return std::sinf(x); }
}

/* --- Joypad --- */
extern "C" {
void joypad_init(void) {}
void joypad_poll(void) {}
joypad_inputs_t joypad_get_inputs(joypad_port_t port) {
  (void)port;
  joypad_inputs_t z{};
  return z;
}
joypad_8way_t joypad_get_direction(joypad_port_t port, joypad_2d_t axes) {
  (void)port;
  (void)axes;
  return JOYPAD_8WAY_NONE;
}
joypad_buttons_t joypad_get_buttons_pressed(joypad_port_t) { joypad_buttons_t z{}; return z; }
joypad_buttons_t joypad_get_buttons_held(joypad_port_t) { joypad_buttons_t z{}; return z; }
}

/* --- T3D / TPX (minimal stubs); Windows "near"/"far" macros break tiny3d params --- */
#ifdef _WIN32
#undef near
#undef far
#endif
#include <t3d/t3d.h>
#include <t3d/tpx.h>
#include <t3d/t3dmodel.h>
extern "C" {
void t3d_init(T3DInitParams) {}
void t3d_destroy(void) {}
void tpx_init(TPXInitParams) {}
void tpx_close(void) {}
T3DModel* t3d_model_load(const char*) { return nullptr; }
void t3d_model_free(T3DModel*) {}
void t3d_matrix_push_pos(int) {}
void t3d_matrix_pop(int) {}
T3DViewport* t3d_viewport_get(void) { return nullptr; }
void t3d_viewport_calc_viewspace_pos(T3DViewport*, T3DVec3* out, const T3DVec3* pos) { (void)out; (void)pos; }
void t3d_viewport_set_perspective(T3DViewport*, float, float, float, float) {}
void t3d_viewport_set_view_matrix(T3DViewport*, const T3DMat4*) {}
void t3d_viewport_attach(T3DViewport*) {}
void t3d_mat4_look_at(T3DMat4* out, const T3DVec3*, const T3DVec3*, const T3DVec3*) { (void)out; }
void t3d_fog_set_range(float, float) {}
void t3d_screen_clear_depth(void) {}
void t3d_screen_clear_color(color_t c) {
  if (!s_pc_attached_color || !s_pc_attached_color->buffer) return;
  uint32_t packed = pc_pack_rgba(c.r, c.g, c.b, c.a);
  tex_format_t fmt = surface_get_format(s_pc_attached_color);
  size_t stride = (size_t)s_pc_attached_color->stride;
  size_t h = s_pc_attached_color->height;
  uint8_t* base = (uint8_t*)s_pc_attached_color->buffer;
  for (size_t y = 0; y < h; y++) {
    if (fmt == FMT_RGBA16) {
      uint16_t* row16 = (uint16_t*)(base + y * stride);
      uint16_t v16 = pc_rgba32_to_rgba16(packed);
      for (int x = 0; x < (int)s_pc_attached_color->width; x++) row16[x] = v16;
    } else {
      uint32_t* row32 = (uint32_t*)(base + y * stride);
      for (int x = 0; x < (int)s_pc_attached_color->width; x++) row32[x] = packed;
    }
  }
}
void t3d_anim_attach(void*, void*) {}
void* t3d_skeleton_create_buffered(void) { return nullptr; }
void t3d_skeleton_update(void*) {}
void* t3d_skeleton_clone(void*) { return nullptr; }
void* t3d_anim_create(void) { return nullptr; }
bool t3d_model_iter_next(T3DModelIter* iter) { (void)iter; return false; }
void t3d_model_draw_material(T3DMaterial* mat, T3DModelState* state) { (void)mat; (void)state; }
void t3d_model_draw_object(const T3DObject* obj, const T3DMat4FP* boneMatrices) { (void)obj; (void)boneMatrices; }
void t3d_anim_destroy(void*) {}
void t3d_skeleton_destroy(void*) {}
void t3d_state_set_vertex_fx(T3DVertexFX, int16_t, int16_t) {}
void t3d_anim_update(void*, float) {}
void t3d_skeleton_blend(void*, void*, void*, float) {}
void t3d_mat4fp_from_srt(T3DMat4FP* mat, const float* scale, const float* rotQuat, const float* translate) { (void)mat; (void)scale; (void)rotQuat; (void)translate; }
void t3d_segment_set(uint8_t, void*) {}
void t3d_matrix_set(const T3DMat4FP* mat, bool doMultiply) { (void)mat; (void)doMultiply; }
void t3d_metrics_fetch(T3DMetrics* data) { (void)data; }
void t3d_light_set_directional(int, const uint8_t*, const T3DVec3*) {}
void t3d_light_set_ambient(const uint8_t*) {}
void t3d_light_set_count(int) {}
void t3d_light_set_point(int, const uint8_t*, const T3DVec3*, float, bool) {}
bool t3d_frustum_vs_aabb(const T3DFrustum* frustum, const T3DVec3* min, const T3DVec3* max) { (void)frustum; (void)min; (void)max; return true; }
bool t3d_frustum_vs_sphere(const T3DFrustum* frustum, const T3DVec3* center, float radius) { (void)frustum; (void)center; (void)radius; return true; }
void t3d_frustum_scale(T3DFrustum* frustum, float scale) { (void)frustum; (void)scale; }
void t3d_model_bvh_query_frustum(const T3DBvh* bvh, const T3DFrustum* frustum) { (void)bvh; (void)frustum; }
void tpx_state_from_t3d(void) {}
void tpx_state_set_scale(float, float) {}
void tpx_state_set_base_size(uint16_t baseSize) { (void)baseSize; }
void tpx_state_set_tex_params(int16_t offsetX, uint16_t mirrorPoint) { (void)offsetX; (void)mirrorPoint; }
void tpx_particle_draw_tex_s8(TPXParticleS8* particles, uint32_t count) { (void)particles; (void)count; }
void tpx_particle_draw_s16(TPXParticleS16* particles, uint32_t count) { (void)particles; (void)count; }
void tpx_particle_draw_tex_s16(TPXParticleS16* particles, uint32_t count) { (void)particles; (void)count; }
void tpx_particle_draw_s8(TPXParticleS8* particles, uint32_t count) { (void)particles; (void)count; }
void t3d_state_set_drawflags(enum T3DDrawFlags drawFlags) { (void)drawFlags; }
void t3d_state_set_depth_offset(int16_t offset) { (void)offset; }
void t3d_vert_load(const T3DVertPacked* vertices, uint32_t offset, uint32_t count) { (void)vertices; (void)offset; (void)count; }
void t3d_quad_draw_unindexed(uint32_t baseIndex, uint32_t quadCount) { (void)baseIndex; (void)quadCount; }
void tpx_buffer_s8_copy(TPXParticleS8* pt, uint32_t idxDst, uint32_t idxSrc) { (void)pt; (void)idxDst; (void)idxSrc; }
void tpx_buffer_s16_copy(TPXParticleS16 pt[], uint32_t idxDst, uint32_t idxSrc) { (void)pt; (void)idxDst; (void)idxSrc; }
uint32_t T3D_RSP_ID = 0;  /* defined here; declared extern in t3d.h */
}

/* --- BigTex (assembly on N64; C stub on PC) --- */
extern "C" uint32_t BigTex_applyTexture(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; return 0; }

/* --- RSP overlay symbols (addresses of ucode; dummy on PC so refs resolve) --- */
extern "C" {
char rsp_hdr_text_start[1];
char rsp_hdr_text_end[1];
char rsp_hdr_data_start[1];
char rsp_hdr_data_end[1];
char rsp_bigtex_text_start[1];
char rsp_bigtex_text_end[1];
char rsp_bigtex_data_start[1];
char rsp_bigtex_data_end[1];
}

/* --- WAV64 (stub: no audio on PC yet) --- */
extern "C" {
wav64_t* wav64_load(const char* path, wav64_loadparms_t* parms) {
  (void)path;
  (void)parms;
  return nullptr;
}
}

/* --- Surface / sprite (surface_alloc returns by value; surface_get_format is inline in header) --- */
extern "C" {
surface_t surface_alloc(tex_format_t format, uint16_t width, uint16_t height) {
  surface_t s = {0};
  if (format == FMT_NONE || width == 0 || height == 0) return s;
  uint16_t stride = (uint16_t)TEX_FORMAT_PIX2BYTES(format, width);
  size_t bytes = (size_t)stride * (size_t)height;
  void* buf = std::malloc(bytes);
  if (!buf) return s;
  std::memset(buf, 0, bytes);
  s.flags = (uint16_t)(format | SURFACE_FLAGS_OWNEDBUFFER);
  s.width = width;
  s.height = height;
  s.stride = stride;
  s.buffer = buf;
  return s;
}
void surface_free(surface_t* surf) {
  if (!surf) return;
  if (surf->buffer && (surf->flags & SURFACE_FLAGS_OWNEDBUFFER)) {
    std::free(surf->buffer);
  }
  surf->flags = 0;
  surf->width = 0;
  surf->height = 0;
  surf->stride = 0;
  surf->buffer = nullptr;
}
sprite_t* sprite_load(const char*) { return nullptr; }
void sprite_free(sprite_t*) {}
}

/* --- RDPQ (screenFade, fonts): triangle + fill format + texture rect scaled --- */
extern "C" {
const rdpq_trifmt_t TRIFMT_FILL = (rdpq_trifmt_t){
    .pos_offset = 0, .shade_offset = -1, .shade_flat = false, .tex_offset = -1,
    .tex_tile = TILE0, .tex_mipmaps = 0, .z_offset = -1
};
void rdpq_triangle(const rdpq_trifmt_t* fmt, const float* v1, const float* v2, const float* v3) {
  (void)fmt; (void)v1; (void)v2; (void)v3;
}
void __rdpq_texture_rectangle_scaled_offline(rdpq_tile_t tile, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t s0, int32_t t0, int32_t s1, int32_t t1) {
  if ((unsigned)tile >= 8u) return;
  if (!s_pc_current_color_image || !s_pc_current_color_image->buffer) return;
  const PcTileTex& tex = s_pc_tile_tex[(unsigned)tile];
  if (!tex.pixels) return;

  int dx0 = x0 / 4, dy0 = y0 / 4, dx1 = x1 / 4, dy1 = y1 / 4;
  if (dx0 > dx1) { int tmp = dx0; dx0 = dx1; dx1 = tmp; }
  if (dy0 > dy1) { int tmp = dy0; dy0 = dy1; dy1 = tmp; }
  if (dx0 < 0) dx0 = 0; if (dy0 < 0) dy0 = 0;
  const int dstW = (int)s_pc_current_color_image->width;
  const int dstH = (int)s_pc_current_color_image->height;
  if (dx1 > dstW) dx1 = dstW;
  if (dy1 > dstH) dy1 = dstH;
  if (dx0 >= dx1 || dy0 >= dy1) return;

  /* RDP texture coordinates are in 5-bit fixed point. */
  const float fs0 = (float)s0 / 32.0f;
  const float ft0 = (float)t0 / 32.0f;
  const float fs1 = (float)s1 / 32.0f;
  const float ft1 = (float)t1 / 32.0f;
  const int rw = dx1 - dx0;
  const int rh = dy1 - dy0;
  if (rw <= 0 || rh <= 0) return;
  const float ds = (fs1 - fs0) / (float)rw;
  const float dt = (ft1 - ft0) / (float)rh;

  for (int y = dy0; y < dy1; y++) {
    const float tv = ft0 + dt * (float)(y - dy0);
    for (int x = dx0; x < dx1; x++) {
      const float su = fs0 + ds * (float)(x - dx0);
      const int ts = (int)su;
      const int tt = (int)tv;
      pc_store_pixel((surface_t*)s_pc_current_color_image, x, y, pc_decode_texel_rgba(tex, ts, tt));
    }
  }
}
}

/* --- Coroutine (node graph scripts / dialog / screenFade) --- */
extern "C" void coro_sleep(uint64_t ticks) { (void)ticks; }
extern "C" coroutine_t* coro_get_current(void) { return nullptr; }
extern "C" void coro_yield(void) { }

/* --- RSP / RSPQ profile (__rsp_check_assert already static inline in rsp.h) --- */
extern "C" void rspq_wait(void) {}
extern "C" void rspq_profile_start(void) {}
extern "C" void rspq_profile_next_frame(void) {}
extern "C" void rspq_profile_dump(void) {}
extern "C" void rspq_profile_reset(void) {}
extern "C" void rspq_profile_get_data(void*) {}

/* RSPQ deferred callback and block free (e.g. user script Credits.cpp destroy) */
extern "C" void rspq_block_free(rspq_block_t* block) { (void)block; }
extern "C" void rspq_call_deferred(void (*fn)(void*), void* arg) { if (fn) fn(arg); }

#endif /* PLATFORM_PC */
