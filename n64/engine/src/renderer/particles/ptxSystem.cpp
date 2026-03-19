/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "renderer/particles/ptxSystem.h"
#include "lib/matrixManager.h"
#ifdef PLATFORM_PC
#include <cstdlib>
#include <cstring>
#endif

P64::PTX::System::System(Type ptxType, uint32_t maxSize)
  : countMax{maxSize}, count{0}, type{ptxType}
{
  assert(sizeof(countMax) % 2 == 0);
  if(countMax > 0) {
    std::size_t allocSize = countMax * sizeof(TPXParticle) / 2;
#ifdef PLATFORM_PC
    particles = std::malloc(allocSize);
    if (particles) std::memset(particles, 0, allocSize);
#else
    particles = malloc_uncached(allocSize);
    sys_hw_memset(particles, 0, allocSize);
#endif
  }
}

P64::PTX::System::~System() {
  if(particles) {
#ifdef PLATFORM_PC
    std::free(particles);
    particles = nullptr;
#else
    free_uncached(particles);
#endif
  }
}

void P64::PTX::System::draw() const {
  if(count == 0)return;

  uint32_t safeCount = count;
  if(safeCount % 2 != 0) {
    if(type == COLOR_A_S16 || type == TEX_A_S16) {
      *tpx_buffer_s16_get_size((TPXParticleS16*)particles, count) = 0;
    } else {
      *tpx_buffer_s8_get_size((TPXParticleS8*)particles, count) = 0;
    }
    ++safeCount;
  }

  switch(type)
  {
    case COLOR_RGBA_S8: tpx_particle_draw_s8((TPXParticleS8*)particles, safeCount); break;
    case TEX_RGBA_S8: tpx_particle_draw_tex_s8((TPXParticleS8*)particles, safeCount); break;
    case COLOR_A_S16: tpx_particle_draw_s16((TPXParticleS16*)particles, safeCount); break;
    case TEX_A_S16: tpx_particle_draw_tex_s16((TPXParticleS16*)particles, safeCount); break;
  }
}

