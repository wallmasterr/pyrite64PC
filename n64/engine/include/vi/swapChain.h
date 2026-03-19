/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <functional>

namespace P64::VI::SwapChain
{
  using RenderPassCB = void(*)(uint32_t fbIndex);
  using RenderPassDrawTask = std::function<void(surface_t* fb, uint32_t fbIndex, RenderPassCB done)>;

  void init();

  void setVBlank(bool enabled);
  float getDeltaTime();
  float getFPS();

  void nextFrame();
  void drain();
  void setFrameSkip(uint32_t skip);

  void setDrawPass(RenderPassDrawTask task);
  void start();

  surface_t *getFrameBuffer(uint32_t idx);
  void setFrameBuffers(surface_t buffers[3]);

#ifdef PLATFORM_PC
  /** Set delta time for this frame (PC only). Call before runOneFrame. */
  void setPCDeltaTime(float dt);
  /** Get the display buffer (RGBA8) for blit to window. */
  void getDisplayBuffer(uint8_t** outPtr, int* outW, int* outH, int* outStride);
#endif
}