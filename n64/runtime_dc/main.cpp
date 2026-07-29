/**
 * Dreamcast host: KallistiOS + software framebuffer present (RGB565 VRAM).
 * Engine compiled with PLATFORM_PC (+ PLATFORM_DC) reuses main_pc / swapChain_pc.
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

static inline uint16_t rgba8_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void present_display_buffer(void)
{
  unsigned char* buf = nullptr;
  int w = 0, h = 0, stride = 0;
  p64_pc_get_display_buffer(&buf, &w, &h, &stride);

  uint16_t* vram = (uint16_t*)vram_s;
  const int vw = vid_mode->width;
  const int vh = vid_mode->height;

  if (!buf || w <= 0 || h <= 0) {
    unsigned char r = 20, g = 20, b = 40, a = 255;
    p64_pc_get_clear_color_rgba8(&r, &g, &b, &a);
    uint16_t c = rgba8_to_rgb565(r, g, b);
    for (int i = 0; i < vw * vh; i++)
      vram[i] = c;
    return;
  }

  /* Nearest-neighbor scale engine 320x240 (or whatever) → video mode */
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

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  char root[64] = "/cd";
  if (!p64_pc_discover_project_path(root, sizeof(root))) {
    printf("p64: no /cd/p64/conf or /rd/p64/conf — place filesystem on disc (-D filesystem)\n");
    p64_pc_set_project_path("/cd");
  } else {
    printf("p64: asset root %s\n", p64_pc_get_project_path());
  }

  vid_set_mode(DM_640x480, PM_RGB565);
  maple_wait_scan();

  p64_engine_init();

  uint64_t last = timer_ms_gettime64();
  for (;;) {
    uint64_t now = timer_ms_gettime64();
    float dt = (float)(now - last) / 1000.0f;
    last = now;
    if (dt <= 0.0f || dt > 0.1f)
      dt = 1.0f / 60.0f;

    p64_engine_run_frame(dt);
    present_display_buffer();
  }

  p64_engine_shutdown();
  return 0;
}
