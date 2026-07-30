/**
 * Built assets are big-endian (N64). PLATFORM_PC / Dreamcast are little-endian hosts.
 */
#pragma once

#include <cstdint>
#include <cstring>

namespace P64::AssetEndian
{
#ifdef PLATFORM_PC
  inline uint16_t be16(uint16_t v) { return __builtin_bswap16(v); }
  inline uint32_t be32(uint32_t v) { return __builtin_bswap32(v); }
  inline int32_t be32i(int32_t v) { return (int32_t)__builtin_bswap32((uint32_t)v); }

  inline float be_f32(float v)
  {
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    u = __builtin_bswap32(u);
    float out;
    std::memcpy(&out, &u, sizeof(out));
    return out;
  }

  template<typename Vec3>
  inline void be_vec3(Vec3& v)
  {
    v.x = be_f32(v.x);
    v.y = be_f32(v.y);
    v.z = be_f32(v.z);
  }
#else
  inline uint16_t be16(uint16_t v) { return v; }
  inline uint32_t be32(uint32_t v) { return v; }
  inline int32_t be32i(int32_t v) { return v; }
  inline float be_f32(float v) { return v; }
  template<typename Vec3>
  inline void be_vec3(Vec3&) {}
#endif
}
