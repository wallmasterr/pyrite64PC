/**
 * Dreamcast host: KallistiOS + software framebuffer present (RGB565 VRAM).
 * Engine compiled with PLATFORM_PC (+ PLATFORM_DC) reuses main_pc / swapChain_pc.
 *
 * Tiny3D/RSP are stubbed on DC, so the soft FB is usually just the scene clear
 * color. Full RGBA8→RGB565 blit every frame is very expensive on SH-4 — use a
 * fast solid present (+ heartbeat) until a real draw path exists.
 */
#include "dc_platform.h"

#include <kos.h>
#include <dc/video.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C" void p64_engine_init(void);
extern "C" void p64_engine_run_frame(float dt);
extern "C" void p64_engine_shutdown(void);
extern "C" void p64_pc_get_display_buffer(unsigned char** out_ptr, int* out_w, int* out_h, int* out_stride);
extern "C" void p64_pc_get_clear_color_rgba8(unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a);

KOS_INIT_FLAGS(INIT_DEFAULT);

/* Set to 1 to force full soft-FB blit (slow; for debugging draw stubs). */
#ifndef P64_DC_FULL_SOFT_PRESENT
#define P64_DC_FULL_SOFT_PRESENT 1
#endif

static inline uint16_t rgba8_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* 3x5 digits + a few glyphs, bit0 = left column of each row */
static const uint8_t kFont3x5[][5] = {
  {0x7,0x5,0x5,0x5,0x7}, /* 0 */
  {0x2,0x6,0x2,0x2,0x7}, /* 1 */
  {0x7,0x1,0x7,0x4,0x7}, /* 2 */
  {0x7,0x1,0x7,0x1,0x7}, /* 3 */
  {0x5,0x5,0x7,0x1,0x1}, /* 4 */
  {0x7,0x4,0x7,0x1,0x7}, /* 5 */
  {0x7,0x4,0x7,0x5,0x7}, /* 6 */
  {0x7,0x1,0x1,0x1,0x1}, /* 7 */
  {0x7,0x5,0x7,0x5,0x7}, /* 8 */
  {0x7,0x5,0x7,0x1,0x7}, /* 9 */
  {0x7,0x5,0x7,0x5,0x5}, /* A=10 F-ish for 'F' use index 11 */
  {0x7,0x4,0x7,0x4,0x4}, /* F=11 */
  {0x7,0x5,0x7,0x5,0x5}, /* P=12 */
  {0x7,0x4,0x7,0x4,0x7}, /* S=13 */
  {0x0,0x0,0x0,0x0,0x0}, /* space=14 */
  {0x0,0x2,0x0,0x2,0x0}, /* :=15 */
};

static int glyph_index(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c == 'F' || c == 'f') return 11;
  if (c == 'P' || c == 'p') return 12;
  if (c == 'S' || c == 's') return 13;
  if (c == ':') return 15;
  if (c == 'A' || c == 'a') return 10;
  return 14;
}

static void vram_draw_char(uint16_t* vram, int vw, int vh, int x, int y, char c, uint16_t color, int scale)
{
  if (!vram || scale < 1) return;
  const uint8_t* g = kFont3x5[glyph_index(c)];
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      if (!(g[row] & (1u << (2 - col)))) continue;
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          const int px = x + col * scale + sx;
          const int py = y + row * scale + sy;
          if (px < 0 || py < 0 || px >= vw || py >= vh) continue;
          vram[py * vw + px] = color;
        }
      }
    }
  }
}

static void vram_draw_text(uint16_t* vram, int vw, int vh, int x, int y, const char* text, uint16_t color, int scale)
{
  if (!text) return;
  int cx = x;
  const int advance = (3 + 1) * scale;
  for (const char* p = text; *p; ++p) {
    vram_draw_char(vram, vw, vh, cx, y, *p, color, scale);
    cx += advance;
  }
}

static float s_fps_display = 0.f;

static void show_solid(uint8_t r, uint8_t g, uint8_t b)
{
  vid_clear(r, g, b);
  vid_flip(-1);
  vid_waitvbl();
}

static void fps_tick(void)
{
  static uint64_t window_start = 0;
  static unsigned window_frames = 0;
  const uint64_t now = timer_ms_gettime64();
  if (window_start == 0)
    window_start = now;
  ++window_frames;
  const uint64_t elapsed = now - window_start;
  if (elapsed >= 500) {
    s_fps_display = (float)window_frames * 1000.f / (float)elapsed;
    window_start = now;
    window_frames = 0;
  }
}

static void draw_fps_overlay(void)
{
  uint16_t* vram = (uint16_t*)vram_s;
  if (!vram || !vid_mode) return;
  const int vw = vid_mode->width;
  const int vh = vid_mode->height;
  char buf[24];
  const int fps_i = (int)(s_fps_display + 0.5f);
  snprintf(buf, sizeof(buf), "FPS:%d", fps_i);

  /* Shadow + bright text near top-left (right of heartbeat) */
  const int scale = 2;
  const int x = 20;
  const int y = 4;
  vram_draw_text(vram, vw, vh, x + 1, y + 1, buf, rgba8_to_rgb565(0, 0, 0), scale);
  vram_draw_text(vram, vw, vh, x, y, buf, rgba8_to_rgb565(255, 255, 64), scale);
}

static void draw_heartbeat_overlay(void)
{
  static unsigned tick = 0;
  ++tick;
  uint16_t* vram = (uint16_t*)vram_s;
  if (!vram || !vid_mode) return;
  const int vw = vid_mode->width;
  const uint8_t pulse = (uint8_t)((tick * 8u) & 255u);
  const uint16_t c = rgba8_to_rgb565(255, pulse, 0);
  for (int y = 0; y < 12; y++) {
    uint16_t* row = vram + y * vw;
    for (int x = 0; x < 12; x++)
      row[x] = c;
  }
}

static void present_fast_clear_with_heartbeat(void)
{
  unsigned char r = 51, g = 51, b = 51, a = 255;
  p64_pc_get_clear_color_rgba8(&r, &g, &b, &a);
  (void)a;
  vid_clear(r, g, b);
  draw_heartbeat_overlay();
  draw_fps_overlay();
  vid_flip(-1);
}

static void present_display_buffer_full(void)
{
  unsigned char* buf = nullptr;
  int w = 0, h = 0, stride = 0;
  p64_pc_get_display_buffer(&buf, &w, &h, &stride);

  uint16_t* vram = (uint16_t*)vram_s;
  if (!vram || !vid_mode) return;
  const int vw = vid_mode->width;
  const int vh = vid_mode->height;
  if (vw <= 0 || vh <= 0) return;

  if (!buf || w <= 0 || h <= 0) {
    present_fast_clear_with_heartbeat();
    return;
  }

  if (w == vw && h == vh && stride == w * 4) {
    for (int y = 0; y < vh; y++) {
      const uint8_t* row = buf + (size_t)y * (size_t)stride;
      uint16_t* dst = vram + y * vw;
      for (int x = 0; x < vw; x++) {
        const uint8_t* p = row + (size_t)x * 4u;
        dst[x] = rgba8_to_rgb565(p[0], p[1], p[2]);
      }
    }
  } else {
    for (int y = 0; y < vh; y++) {
      int sy = (y * h) / vh;
      const uint8_t* row = buf + (size_t)sy * (size_t)stride;
      uint16_t* dst = vram + y * vw;
      for (int x = 0; x < vw; x++) {
        int sx = (x * w) / vw;
        const uint8_t* p = row + (size_t)sx * 4u;
        dst[x] = rgba8_to_rgb565(p[0], p[1], p[2]);
      }
    }
  }

  draw_heartbeat_overlay();
  draw_fps_overlay();
  vid_flip(-1);
}

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  vid_set_mode(DM_320x240, PM_RGB565);
  show_solid(0, 180, 255); /* cyan = video OK */

  char root[64] = "/cd";
  if (!p64_pc_discover_project_path(root, sizeof(root))) {
    printf("p64: no /cd/p64/conf or /rd/p64/conf — place filesystem on disc (-D filesystem)\n");
    p64_pc_set_project_path("/cd");
    show_solid(255, 64, 0); /* orange = assets missing */
  } else {
    printf("p64: asset root %s\n", p64_pc_get_project_path());
  }

  maple_wait_scan();
  printf("p64: maple ready, engine_init...\n");

  p64_engine_init();
  printf("p64: engine_init done\n");
  /* Skip solid-color boot flash; first present shows game FB + FPS overlay. */

  uint64_t last = timer_ms_gettime64();
  unsigned frame = 0;
  for (;;) {
    if (frame == 0)
      printf("p64: first frame (scene construct)...\n");

    uint64_t now = timer_ms_gettime64();
    float dt = (float)(now - last) / 1000.0f;
    last = now;
    if (dt <= 0.0f || dt > 0.1f)
      dt = 1.0f / 60.0f;

    p64_engine_run_frame(dt);
    fps_tick();

    if (frame == 0)
      printf("p64: first frame done, presenting...\n");

#if P64_DC_FULL_SOFT_PRESENT
    present_display_buffer_full();
#else
    present_fast_clear_with_heartbeat();
#endif
    vid_waitvbl();

    if (frame == 0)
      printf("p64: present ok (soft FB blit; soft Tiny3D)\n");
    frame++;
  }

  p64_engine_shutdown();
  return 0;
}
