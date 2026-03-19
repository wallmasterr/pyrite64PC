/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "rspHDR.h"
#include <cstdint>
#include <libdragon.h>

extern "C" {
  DEFINE_RSP_UCODE(rsp_hdr);

  // Some libdragon issues with C++ and namespaces
  inline void rspq_write_1(uint32_t rspID, uint32_t cmd, uint32_t a) {
    rspq_write(rspID, cmd, a);
  }
  inline void rspq_write_2(uint32_t rspID, uint32_t cmd, uint32_t a, uint32_t b) {
    rspq_write(rspID, cmd, a, b);
  }
  inline void rspq_write_3(uint32_t rspID, uint32_t cmd, uint32_t a, uint32_t b, uint32_t c) {
    rspq_write(rspID, cmd, a, b, c);
  }
  inline void rspq_write_4(uint32_t rspID, uint32_t cmd, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    rspq_write(rspID, cmd, a, b, c, d);
  }
}

namespace {
  constexpr uint32_t BLUR_STRIDE = 80*4;

  constexpr uint32_t CMD_HDR_BLIT   = 0x00;
  constexpr uint32_t CMD_BLUR       = 0x01;
  constexpr uint32_t CMD_DOWN_SCALE = 0x02;

  uint32_t rspIdFX{0};
}

void RspHDR::init()
{
  if(rspIdFX == 0) {
    rspIdFX = rspq_overlay_register(&rsp_hdr);
  }
}

void RspHDR::destroy()
{
  if(rspIdFX)rspq_overlay_unregister(rspIdFX);
  rspIdFX = 0;
}

void RspHDR::hdrBlit(void* rgba32In, void *rgba16Out, void* rgba32BloomIn, float factor)
{
  uint32_t factorInt = (uint32_t)(factor * 0xFFFF);
  rspq_write_4(rspIdFX, CMD_HDR_BLIT,
     (uint32_t)(uintptr_t)rgba32In & 0xFFFFFF,
     (uint32_t)(uintptr_t)rgba16Out & 0xFFFFFF,
     (uint32_t)(uintptr_t)rgba32BloomIn & 0xFFFFFF,
     factorInt
  );
}

void RspHDR::downscale(void* rgba32In, void* rgba32Out)
{
  rspq_write_2(rspIdFX, CMD_DOWN_SCALE,
     (uint32_t)(uintptr_t)rgba32In & 0xFFFFFF,
     (uint32_t)(uintptr_t)rgba32Out & 0xFFFFFF
  );
}

void RspHDR::blur(void* rgba32In, void* rgba32Out, float brightness, float threshold)
{
  constexpr float quantFactor = (1 << 12) * 0.99f;

  uint32_t factors = (uint32_t)(threshold * 0x7FFF) & 0xFFFF;
  factors <<= 16;
  factors |= (uint32_t)(brightness * quantFactor) & 0xFFFF;

  rspq_write_3(rspIdFX, CMD_BLUR,
    (uint32_t)(uintptr_t)rgba32In & 0xFFFFFF,
    ((uint32_t)(uintptr_t)rgba32Out - BLUR_STRIDE) & 0xFFFFFF,
    factors
  );
}