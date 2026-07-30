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
#define P64_DC_FULL_SOFT_PRESENT 0
#endif

static inline uint16_t rgba8_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void show_solid(uint8_t r, uint8_t g, uint8_t b)
{
  vid_clear(r, g, b);
  vid_flip(-1);
  vid_waitvbl();
}

static void present_fast_clear_with_heartbeat(void)
{
  unsigned char r = 51, g = 51, b = 51, a = 255;
  p64_pc_get_clear_color_rgba8(&r, &g, &b, &a);
  (void)a;
  vid_clear(r, g, b);

  /* Pulsing 16x16 block so a “stuck grey” still shows the main loop is alive. */
  static unsigned tick = 0;
  ++tick;
  uint16_t* vram = (uint16_t*)vram_s;
  if (vram && vid_mode) {
    const int vw = vid_mode->width;
    const uint8_t pulse = (uint8_t)((tick * 8u) & 255u);
    const uint16_t c = rgba8_to_rgb565(255, pulse, 0);
    for (int y = 0; y < 16; y++) {
      uint16_t* row = vram + y * vw;
      for (int x = 0; x < 16; x++)
        row[x] = c;
    }
  }
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
  show_solid(0, 255, 64); /* green = init OK */

  uint64_t last = timer_ms_gettime64();
  unsigned frame = 0;
  for (;;) {
    if (frame == 0) {
      show_solid(255, 255, 0); /* yellow = first frame / scene load */
      printf("p64: first frame (scene construct)...\n");
    }

    uint64_t now = timer_ms_gettime64();
    float dt = (float)(now - last) / 1000.0f;
    last = now;
    if (dt <= 0.0f || dt > 0.1f)
      dt = 1.0f / 60.0f;

    p64_engine_run_frame(dt);

    if (frame == 0) {
      show_solid(255, 0, 255); /* magenta = first frame returned */
      printf("p64: first frame done, presenting...\n");
    }

#if P64_DC_FULL_SOFT_PRESENT
    present_display_buffer_full();
#else
    present_fast_clear_with_heartbeat();
#endif
    vid_waitvbl();

    if (frame == 0)
      printf("p64: present ok (fast clear; Tiny3D stubbed — expect scene clear color)\n");
    frame++;
  }

  p64_engine_shutdown();
  return 0;
}
