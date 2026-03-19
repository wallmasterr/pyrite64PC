/**
 * PC implementation of SwapChain: dummy framebuffers and delta time from host.
 */
#ifdef PLATFORM_PC
#include "vi/swapChain.h"
#include "lib/logger.h"
#include <libdragon.h>
#include <cstdint>
#include <cstring>

namespace {
  static surface_t s_dummyFb[3];
  static float s_deltaTime = 1.0f / 60.0f;
  static bool s_deltaTimeSet = false;
}

namespace P64::VI::SwapChain
{
  void setPCDeltaTime(float dt) {
    s_deltaTime = dt;
    s_deltaTimeSet = true;
  }

  void init()
  {
    std::memset(s_dummyFb, 0, sizeof(s_dummyFb));
    for (int i = 0; i < 3; i++) {
      s_dummyFb[i].width = 640;
      s_dummyFb[i].height = 480;
      s_dummyFb[i].stride = 640 * 2;
      s_dummyFb[i].flags = 0;
      s_dummyFb[i].buffer = nullptr;
    }
    s_deltaTime = 1.0f / 60.0f;
    s_deltaTimeSet = false;
  }

  void setVBlank(bool) {}
  float getDeltaTime() { return s_deltaTime; }
  float getFPS() { return 60.0f; }
  void nextFrame() {}
  void drain() {}
  void setFrameSkip(uint32_t) {}
  void setDrawPass(RenderPassDrawTask) {}
  void start() {}
  void setFrameBuffers(surface_t*) {}
  surface_t *getFrameBuffer(uint32_t idx) {
    return idx < 3 ? &s_dummyFb[idx] : &s_dummyFb[0];
  }
}
#endif
