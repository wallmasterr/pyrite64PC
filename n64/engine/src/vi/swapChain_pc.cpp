/**
 * PC implementation of SwapChain: dummy framebuffers and delta time from host.
 * Stores and invokes the draw pass in drain() so scene.draw() runs each frame.
 */
#ifdef PLATFORM_PC
#include "vi/swapChain.h"
#include "lib/logger.h"
#include <libdragon.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>

extern "C" void p64_pc_trace(const char* step);

namespace {
  /* Match N64 resolution (display_get_* / scene use 320x240); runtime stretches to window */
  static constexpr int DISPLAY_W = 320;
  static constexpr int DISPLAY_H = 240;
  static constexpr int DISPLAY_BPP = 4; /* RGBA8 */

  static surface_t s_dummyFb[3];
  static float s_deltaTime = 1.0f / 60.0f;
  static bool s_deltaTimeSet = false;
  static P64::VI::SwapChain::RenderPassDrawTask s_drawTask{nullptr};
  static uint8_t* s_displayBuffer = nullptr;
}

namespace P64::VI::SwapChain
{
  void setPCDeltaTime(float dt) {
    s_deltaTime = dt;
    s_deltaTimeSet = true;
  }

  void init()
  {
    if (s_displayBuffer) {
      free(s_displayBuffer);
      s_displayBuffer = nullptr;
    }
    s_displayBuffer = (uint8_t*)malloc((size_t)DISPLAY_W * DISPLAY_H * DISPLAY_BPP);
    std::memset(s_dummyFb, 0, sizeof(s_dummyFb));
    for (int i = 0; i < 3; i++) {
      s_dummyFb[i].width = (uint16_t)DISPLAY_W;
      s_dummyFb[i].height = (uint16_t)DISPLAY_H;
      s_dummyFb[i].stride = DISPLAY_W * DISPLAY_BPP;
      s_dummyFb[i].flags = 0;
      s_dummyFb[i].buffer = (i == 0) ? s_displayBuffer : nullptr;
    }
    s_deltaTime = 1.0f / 60.0f;
    s_deltaTimeSet = false;
    s_drawTask = nullptr;
  }

  void getDisplayBuffer(uint8_t** outPtr, int* outW, int* outH, int* outStride) {
    if (outPtr) *outPtr = s_displayBuffer;
    if (outW) *outW = s_displayBuffer ? DISPLAY_W : 0;
    if (outH) *outH = s_displayBuffer ? DISPLAY_H : 0;
    if (outStride) *outStride = s_displayBuffer ? (DISPLAY_W * DISPLAY_BPP) : 0;
  }

  void setVBlank(bool) {}
  float getDeltaTime() { return s_deltaTime; }
  float getFPS() { return 60.0f; }
  void nextFrame() {}
  void drain() {
    if (!s_drawTask) return;
    p64_pc_trace("drain_start");
    surface_t* fb = getFrameBuffer(0);
    try {
      s_drawTask(fb, 0, [](uint32_t) {});
    } catch (const std::exception& e) {
      (void)e;
      p64_pc_trace("drain_exception");
    } catch (...) {
      p64_pc_trace("drain_unknown");
    }
    p64_pc_trace("drain_done");
  }
  void setFrameSkip(uint32_t) {}
  void setDrawPass(RenderPassDrawTask task) { s_drawTask = std::move(task); }
  void start() {}
  void setFrameBuffers(surface_t*) {}
  surface_t *getFrameBuffer(uint32_t idx) {
    return idx < 3 ? &s_dummyFb[idx] : &s_dummyFb[0];
  }
}
#endif
