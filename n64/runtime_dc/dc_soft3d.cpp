/**
 * Dreamcast soft Tiny3D: load .t3dm (BE→LE), float matrices, flat-shaded triangles
 * into the soft RGBA8 framebuffer attached via rdpq_attach.
 */
#include "dc_platform.h"

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdio>

#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmodel.h>
#include "assets/assetManager.h"

extern "C" {
  const surface_t* p64_dc_soft_get_color_target(void);
  void p64_dc_soft_set_color_target(const surface_t* surf);
}

namespace {

constexpr int kMatStackMax = 16;
constexpr int kBlockCmdMax = 64;

inline uint16_t be16(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t be32(uint32_t v) { return __builtin_bswap32(v); }
inline int16_t be16s(int16_t v) { return (int16_t)be16((uint16_t)v); }

struct Mat4 {
  float m[4][4];
};

inline Mat4 mat_identity()
{
  Mat4 r{};
  r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
  return r;
}

inline Mat4 mat_mul(const Mat4& a, const Mat4& b)
{
  Mat4 r{};
  for (int c = 0; c < 4; c++)
    for (int row = 0; row < 4; row++)
      r.m[c][row] = a.m[0][row] * b.m[c][0] + a.m[1][row] * b.m[c][1]
                  + a.m[2][row] * b.m[c][2] + a.m[3][row] * b.m[c][3];
  return r;
}

inline void mat_mul_vec3(const Mat4& m, float x, float y, float z, float* ox, float* oy, float* oz, float* ow)
{
  *ox = m.m[0][0] * x + m.m[1][0] * y + m.m[2][0] * z + m.m[3][0];
  *oy = m.m[0][1] * x + m.m[1][1] * y + m.m[2][1] * z + m.m[3][1];
  *oz = m.m[0][2] * x + m.m[1][2] * y + m.m[2][2] * z + m.m[3][2];
  *ow = m.m[0][3] * x + m.m[1][3] * y + m.m[2][3] * z + m.m[3][3];
}

Mat4 mat_from_srt(const float scale[3], const float quat[4], const float translate[3])
{
  /* quat is x,y,z,w */
  const float x = quat[0], y = quat[1], z = quat[2], w = quat[3];
  const float x2 = x + x, y2 = y + y, z2 = z + z;
  const float xx = x * x2, yy = y * y2, zz = z * z2;
  const float xy = x * y2, xz = x * z2, yz = y * z2;
  const float wx = w * x2, wy = w * y2, wz = w * z2;
  Mat4 r = mat_identity();
  r.m[0][0] = (1.f - (yy + zz)) * scale[0];
  r.m[0][1] = (xy + wz) * scale[0];
  r.m[0][2] = (xz - wy) * scale[0];
  r.m[1][0] = (xy - wz) * scale[1];
  r.m[1][1] = (1.f - (xx + zz)) * scale[1];
  r.m[1][2] = (yz + wx) * scale[1];
  r.m[2][0] = (xz + wy) * scale[2];
  r.m[2][1] = (yz - wx) * scale[2];
  r.m[2][2] = (1.f - (xx + yy)) * scale[2];
  r.m[3][0] = translate[0];
  r.m[3][1] = translate[1];
  r.m[3][2] = translate[2];
  return r;
}

Mat4 mat_look_at(const float* eye, const float* target, const float* up)
{
  float fx = target[0] - eye[0], fy = target[1] - eye[1], fz = target[2] - eye[2];
  float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
  if (fl > 1e-6f) { fx /= fl; fy /= fl; fz /= fl; }
  float sx = fy * up[2] - fz * up[1];
  float sy = fz * up[0] - fx * up[2];
  float sz = fx * up[1] - fy * up[0];
  float sl = std::sqrt(sx * sx + sy * sy + sz * sz);
  if (sl > 1e-6f) { sx /= sl; sy /= sl; sz /= sl; }
  float ux = sy * fz - sz * fy;
  float uy = sz * fx - sx * fz;
  float uz = sx * fy - sy * fx;
  Mat4 r = mat_identity();
  r.m[0][0] = sx; r.m[1][0] = sy; r.m[2][0] = sz;
  r.m[0][1] = ux; r.m[1][1] = uy; r.m[2][1] = uz;
  r.m[0][2] = -fx; r.m[1][2] = -fy; r.m[2][2] = -fz;
  r.m[3][0] = -(sx * eye[0] + sy * eye[1] + sz * eye[2]);
  r.m[3][1] = -(ux * eye[0] + uy * eye[1] + uz * eye[2]);
  r.m[3][2] = -(-fx * eye[0] - fy * eye[1] - fz * eye[2]);
  return r;
}

Mat4 mat_perspective(float fov, float aspect, float zn, float zf)
{
  /* Match tiny3d t3d_mat4_perspective */
  const float tanHalf = std::tan(fov * 0.5f);
  Mat4 r{};
  r.m[0][0] = 1.f / (aspect * tanHalf);
  r.m[1][1] = 1.f / tanHalf;
  r.m[2][2] = zf / (zn - zf);
  r.m[2][3] = -1.f;
  r.m[3][2] = -2.f * (zf * zn) / (zf - zn);
  return r;
}

Mat4 mat_ortho(float l, float r, float b, float t, float zn, float zf)
{
  Mat4 m = mat_identity();
  m.m[0][0] = 2.f / (r - l);
  m.m[1][1] = 2.f / (t - b);
  m.m[2][2] = -2.f / (zf - zn);
  m.m[3][0] = -(r + l) / (r - l);
  m.m[3][1] = -(t + b) / (t - b);
  m.m[3][2] = -(zf + zn) / (zf - zn);
  return m;
}

struct SoftViewport {
  Mat4 view = mat_identity();
  Mat4 proj = mat_identity();
  int ox = 0, oy = 0, w = 320, h = 240;
  bool ortho = false;
};

SoftViewport s_vp{};
SoftViewport* s_vp_attached = &s_vp;

Mat4 s_stack[kMatStackMax];
int s_stack_top = 0;
Mat4 s_model = mat_identity();

const surface_t* s_color = nullptr;
float* s_depth = nullptr;
int s_depth_w = 0;
int s_depth_h = 0;

void soft_depth_ensure(int w, int h)
{
  if (w <= 0 || h <= 0) return;
  if (s_depth && s_depth_w == w && s_depth_h == h) return;
  free(s_depth);
  s_depth = (float*)malloc((size_t)w * (size_t)h * sizeof(float));
  s_depth_w = w;
  s_depth_h = h;
}

void soft_depth_clear(void)
{
  if (!s_color || !s_color->buffer) return;
  const int w = (int)s_color->width;
  const int h = (int)s_color->height;
  soft_depth_ensure(w, h);
  if (!s_depth) return;
  const int n = w * h;
  for (int i = 0; i < n; i++)
    s_depth[i] = 1.f; /* far NDC z */
}

struct SoftBlock {
  T3DObject* objs[kBlockCmdMax];
  int count = 0;
};
SoftBlock* s_recording = nullptr;

void put_pixel(uint8_t* base, int stride, int w, int h, int x, int y, uint32_t rgba)
{
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  uint32_t* row = (uint32_t*)(base + (size_t)y * (size_t)stride);
  row[x] = rgba;
}

struct SoftTex {
  const uint8_t* pixels = nullptr;
  tex_format_t fmt = FMT_NONE;
  int width = 0, height = 0, stride = 0;
  bool ok = false;
};

inline uint16_t be16u(uint16_t v) { return __builtin_bswap16(v); }

uint32_t sample_tex(const SoftTex& tex, float u, float v)
{
  if (!tex.ok || !tex.pixels || tex.width <= 0 || tex.height <= 0)
    return 0xFFFFFFFFu;
  /* Wrap like RDP clamp/wrap for crate UVs spanning ~0..44 */
  int s = (int)std::floor(u);
  int t = (int)std::floor(v);
  s %= tex.width; if (s < 0) s += tex.width;
  t %= tex.height; if (t < 0) t += tex.height;

  const uint8_t* row = tex.pixels + (size_t)t * (size_t)tex.stride;
  switch (tex.fmt) {
    case FMT_RGBA16: {
      const uint16_t px = be16u(((const uint16_t*)row)[s]);
      const uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
      const uint8_t g = (uint8_t)(((px >> 6) & 0x1F) * 255 / 31);
      const uint8_t b = (uint8_t)(((px >> 1) & 0x1F) * 255 / 31);
      const uint8_t a = (px & 1) ? 255 : 0;
      return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }
    case FMT_RGBA32: {
      const uint8_t* p = row + (size_t)s * 4u;
      /* BE RGBA8 in file */
      return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    case FMT_I8: {
      const uint8_t i = row[s];
      return (uint32_t)i | ((uint32_t)i << 8) | ((uint32_t)i << 16) | 0xFF000000u;
    }
    default:
      return 0xFF808080u;
  }
}

void fill_tri_tex(uint8_t* base, int stride, int w, int h,
                  float x0, float y0, float z0, float u0, float v0,
                  float x1, float y1, float z1, float u1, float v1,
                  float x2, float y2, float z2, float u2, float v2,
                  const SoftTex* tex, uint32_t flatFallback)
{
  /* Sort by y, carrying depth + UVs */
  auto swap8 = [](float& ax, float& ay, float& az, float& au, float& av,
                  float& bx, float& by, float& bz, float& bu, float& bv) {
    std::swap(ax, bx); std::swap(ay, by); std::swap(az, bz);
    std::swap(au, bu); std::swap(av, bv);
  };
  if (y1 < y0) swap8(x0, y0, z0, u0, v0, x1, y1, z1, u1, v1);
  if (y2 < y0) swap8(x0, y0, z0, u0, v0, x2, y2, z2, u2, v2);
  if (y2 < y1) swap8(x1, y1, z1, u1, v1, x2, y2, z2, u2, v2);

  const float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
  if (std::fabs(area) < 0.01f) return;
  const float invA = 1.f / area;

  auto edge = [](float y0, float x0, float y1, float x1, float y) -> float {
    if (std::fabs(y1 - y0) < 1e-6f) return x0;
    return x0 + (x1 - x0) * ((y - y0) / (y1 - y0));
  };

  soft_depth_ensure(w, h);
  float* depth = s_depth;

  const int yStart = (int)std::ceil(y0);
  const int yEnd = (int)std::floor(y2);
  for (int y = yStart; y <= yEnd; y++) {
    if (y < 0 || y >= h) continue;
    float xa, xb;
    if (y < y1 || std::fabs(y2 - y1) < 1e-6f) {
      xa = edge(y0, x0, y2, x2, (float)y);
      xb = edge(y0, x0, y1, x1, (float)y);
    } else {
      xa = edge(y0, x0, y2, x2, (float)y);
      xb = edge(y1, x1, y2, x2, (float)y);
    }
    if (xb < xa) std::swap(xa, xb);
    int x0i = (int)std::ceil(xa);
    int x1i = (int)std::floor(xb);
    for (int x = x0i; x <= x1i; x++) {
      if (x < 0 || x >= w) continue;
      const float px = (float)x + 0.5f, py = (float)y + 0.5f;
      const float bw0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * invA;
      const float bw1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * invA;
      const float bw2 = 1.f - bw0 - bw1;
      const float z = bw0 * z0 + bw1 * z1 + bw2 * z2;
      if (depth) {
        float& dz = depth[y * w + x];
        if (z >= dz) continue; /* farther or equal — keep existing */
        dz = z;
      }
      uint32_t col = flatFallback;
      if (tex && tex->ok) {
        const float u = bw0 * u0 + bw1 * u1 + bw2 * u2;
        const float v = bw0 * v0 + bw1 * v1 + bw2 * v2;
        col = sample_tex(*tex, u, v);
        if ((col >> 24) == 0) continue;
      }
      put_pixel(base, stride, w, h, x, y, col);
    }
  }
}

SoftTex soft_tex_from_sprite(sprite_t* spr)
{
  SoftTex out{};
  if (!spr || spr->width == 0 || spr->height == 0) return out;
  out.pixels = (const uint8_t*)spr->data;
  out.fmt = sprite_get_format(spr);
  out.width = (int)spr->width;
  out.height = (int)spr->height;
  out.stride = (int)TEX_FORMAT_PIX2BYTES(out.fmt, out.width);
  out.ok = out.pixels && out.stride > 0 &&
           (out.fmt == FMT_RGBA16 || out.fmt == FMT_RGBA32 || out.fmt == FMT_I8);
  return out;
}

SoftTex soft_tex_from_material(void* matPtr)
{
  SoftTex out{};
  if (!matPtr) return out;
  const uint8_t* m = (const uint8_t*)matPtr;
  const uint32_t flags = be32(*(const uint32_t*)m);
  constexpr uint32_t FLAG_TEX0 = 1u << 8;
  if (!(flags & FLAG_TEX0)) return out;
  const uint16_t texIdx = be16(*(const uint16_t*)(m + 8));
  sprite_t* spr = (sprite_t*)P64::AssetManager::getByIndex(texIdx);
  return soft_tex_from_sprite(spr);
}

uint32_t rgba_be_to_host(uint32_t be)
{
  /* Stored BE in file; after vert byteswap it's host. Pack for LE FB as R,G,B,A bytes. */
  const uint8_t r = (uint8_t)((be >> 24) & 0xFF);
  const uint8_t g = (uint8_t)((be >> 16) & 0xFF);
  const uint8_t b = (uint8_t)((be >> 8) & 0xFF);
  const uint8_t a = (uint8_t)(be & 0xFF);
  return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

inline void* align_ptr(void* p, uintptr_t align)
{
  uintptr_t v = (uintptr_t)p;
  return (void*)((v + align - 1u) & ~(align - 1u));
}

void soft_draw_object(T3DObject* obj)
{
  if (!obj || !s_color || !s_color->buffer) return;
  uint8_t* base = (uint8_t*)s_color->buffer;
  const int w = (int)s_color->width;
  const int h = (int)s_color->height;
  const int stride = (int)s_color->stride;
  if (w <= 0 || h <= 0) return;

  const SoftTex tex = soft_tex_from_material(obj->material);
  const Mat4 mvp = mat_mul(s_vp_attached->proj, mat_mul(s_vp_attached->view, s_model));
  const float ox = (float)s_vp_attached->ox;
  const float oy = (float)s_vp_attached->oy;
  const float vw = (float)s_vp_attached->w;
  const float vh = (float)s_vp_attached->h;

  auto project = [&](T3DVertPacked* vert, int vi, float& sx, float& sy, float& sz,
                     float& su, float& sv, uint32_t& col) -> bool {
    int16_t* p = t3d_vertbuffer_get_pos(vert, vi);
    float x = (float)p[0], y = (float)p[1], z = (float)p[2];
    float oxw, oyw, ozw, oww;
    mat_mul_vec3(mvp, x, y, z, &oxw, &oyw, &ozw, &oww);
    if (oww <= 0.001f) return false;
    const float inv = 1.f / oww;
    const float ndcX = oxw * inv;
    const float ndcY = oyw * inv;
    sx = ox + (ndcX * 0.5f + 0.5f) * vw;
    sy = oy + (1.f - (ndcY * 0.5f + 0.5f)) * vh;
    sz = ozw * inv; /* NDC z — closer is smaller with our proj */
    int16_t* st = t3d_vertbuffer_get_uv(vert, vi);
    su = (float)st[0] / 32.f;
    sv = (float)st[1] / 32.f;
    col = rgba_be_to_host(*t3d_vertbuffer_get_color(vert, vi));
    if ((col & 0x00FFFFFFu) == 0)
      col = 0xFFFFFFFFu;
    return true;
  };

  auto draw_tri = [&](T3DVertPacked* vert, int i0, int i1, int i2) {
    float x0, y0, z0, u0, v0, x1, y1, z1, u1, v1, x2, y2, z2, u2, v2;
    uint32_t c0, c1, c2;
    if (!project(vert, i0, x0, y0, z0, u0, v0, c0)) return;
    if (!project(vert, i1, x1, y1, z1, u1, v1, c1)) return;
    if (!project(vert, i2, x2, y2, z2, u2, v2, c2)) return;
    fill_tri_tex(base, stride, w, h,
                 x0, y0, z0, u0, v0, x1, y1, z1, u1, v1, x2, y2, z2, u2, v2,
                 tex.ok ? &tex : nullptr, c0);
  };

  for (uint16_t pi = 0; pi < obj->numParts; pi++) {
    T3DObjectPart& part = obj->parts[pi];
    if (!part.vert) continue;

    /* Triangle list */
    if (part.indices && part.numIndices >= 3) {
      for (uint16_t i = 0; i + 2 < part.numIndices; i += 3)
        draw_tri(part.vert, part.indices[i], part.indices[i + 1], part.indices[i + 2]);
    }

    /* Sequential unindexed */
    if (part.idxSeqCount >= 3) {
      for (uint16_t i = 0; i + 2 < part.idxSeqCount; i += 3)
        draw_tri(part.vert, part.idxSeqBase + i, part.idxSeqBase + i + 1, part.idxSeqBase + i + 2);
    }

    /* Triangle strips (local indices, MSB = restart) — file format before DMEM convert */
    if (part.numStripIndices[0] != 0 && part.indices) {
      uint8_t* stripPtr = (uint8_t*)align_ptr(part.indices + part.numIndices, 8);
      for (int s = 0; s < 4; ++s) {
        const uint16_t n = part.numStripIndices[s];
        if (n == 0) break;
        int16_t* idx = (int16_t*)stripPtr;
        int i = 0;
        int prev[3] = {0, 0, 0};
        bool flip = false;
        bool have = false;
        while (i < (int)n) {
          const uint16_t raw = (uint16_t)idx[i];
          const bool restart = (i == 0) || ((raw & 0x8000u) != 0);
          const int v = (int)(raw & 0x7FFFu);
          if (restart) {
            if (i + 2 >= (int)n) break;
            prev[0] = v;
            prev[1] = (int)((uint16_t)idx[i + 1] & 0x7FFFu);
            prev[2] = (int)((uint16_t)idx[i + 2] & 0x7FFFu);
            draw_tri(part.vert, prev[0], prev[1], prev[2]);
            flip = true;
            have = true;
            i += 3;
          } else {
            if (!have) { i++; continue; }
            const int a = prev[1], b = prev[2], c = v;
            if (flip)
              draw_tri(part.vert, a, c, b);
            else
              draw_tri(part.vert, a, b, c);
            prev[0] = a;
            prev[1] = b;
            prev[2] = c;
            flip = !flip;
            i++;
          }
        }
        stripPtr = (uint8_t*)align_ptr(stripPtr + (size_t)n * 2u, 8);
      }
    }
  }
}

void byteswap_vert_chunk(T3DVertPacked* verts, uint32_t packedCount)
{
  for (uint32_t i = 0; i < packedCount; i++) {
    for (int k = 0; k < 3; k++) {
      verts[i].posA[k] = be16s(verts[i].posA[k]);
      verts[i].posB[k] = be16s(verts[i].posB[k]);
    }
    verts[i].normA = be16(verts[i].normA);
    verts[i].normB = be16(verts[i].normB);
    verts[i].rgbaA = be32(verts[i].rgbaA);
    verts[i].rgbaB = be32(verts[i].rgbaB);
    verts[i].stA[0] = be16s(verts[i].stA[0]);
    verts[i].stA[1] = be16s(verts[i].stA[1]);
    verts[i].stB[0] = be16s(verts[i].stB[0]);
    verts[i].stB[1] = be16s(verts[i].stB[1]);
  }
}

} // namespace

extern "C" {

void p64_dc_soft_set_color_target(const surface_t* surf) { s_color = surf; }
const surface_t* p64_dc_soft_get_color_target(void) { return s_color; }

void p64_dc_soft_clear_depth(void) { soft_depth_clear(); }

T3DModel* t3d_model_load(const char* path)
{
  int size = 0;
  T3DModel* model = (T3DModel*)asset_load(path, &size);
  if (!model || size < 32) {
    printf("[p64] t3d_model_load fail: %s\n", path ? path : "?");
    return nullptr;
  }
  if (memcmp(model->magic, "T3M", 3) != 0) {
    printf("[p64] bad magic %.4s path=%s\n", model->magic, path);
    free(model);
    return nullptr;
  }

  model->chunkCount = be32(model->chunkCount);
  model->totalVertCount = be16(model->totalVertCount);
  model->totalIndexCount = be16(model->totalIndexCount);
  model->chunkIdxVertices = be32(model->chunkIdxVertices);
  model->chunkIdxIndices = be32(model->chunkIdxIndices);
  model->chunkIdxMaterials = be32(model->chunkIdxMaterials);
  model->stringTablePtr = (char*)(uintptr_t)be32((uint32_t)(uintptr_t)model->stringTablePtr);
  for (int i = 0; i < 3; i++) {
    model->aabbMin[i] = be16s(model->aabbMin[i]);
    model->aabbMax[i] = be16s(model->aabbMax[i]);
  }
  for (uint32_t i = 0; i < model->chunkCount; i++)
    model->chunkOffsets[i].offset = be32(model->chunkOffsets[i].offset);

  model->stringTablePtr = (char*)model + (uintptr_t)model->stringTablePtr;
  model->userBlock = nullptr;

  void* basePtrVertices = (char*)model + T3D_CHUNK_OFF(model->chunkOffsets[model->chunkIdxVertices]);
  void* basePtrIndices = (char*)model + T3D_CHUNK_OFF(model->chunkOffsets[model->chunkIdxIndices]);

  /* Vert chunk: each pack is 32 bytes; totalVertCount is vertex count */
  byteswap_vert_chunk((T3DVertPacked*)basePtrVertices, (model->totalVertCount + 1u) / 2u);

  for (uint32_t i = 0; i < model->chunkCount; i++) {
    const char chunkType = T3D_CHUNK_TYPE(model->chunkOffsets[i]);
    const uint32_t offset = T3D_CHUNK_OFF(model->chunkOffsets[i]);
    if (chunkType != T3D_CHUNK_TYPE_OBJECT)
      continue;

    T3DObject* obj = (T3DObject*)((char*)model + offset);
    obj->numParts = be16(obj->numParts);
    obj->triCount = be16(obj->triCount);
    obj->name = (char*)(uintptr_t)be32((uint32_t)(uintptr_t)obj->name);
    if (obj->name)
      obj->name = (char*)((uintptr_t)model->stringTablePtr + (uintptr_t)obj->name);

    const uint32_t matRel = be32((uint32_t)(uintptr_t)obj->material);
    const uint32_t matIdx = model->chunkIdxMaterials + matRel;
    obj->material = (T3DMaterial*)((char*)model + T3D_CHUNK_OFF(model->chunkOffsets[matIdx]));

    for (int k = 0; k < 3; k++) {
      obj->aabbMin[k] = be16s(obj->aabbMin[k]);
      obj->aabbMax[k] = be16s(obj->aabbMax[k]);
    }

    for (uint32_t j = 0; j < obj->numParts; j++) {
      T3DObjectPart* part = &obj->parts[j];
      part->vertLoadCount = be16(part->vertLoadCount);
      part->vertDestOffset = be16(part->vertDestOffset);
      part->numIndices = be16(part->numIndices);
      part->matrixIdx = be16(part->matrixIdx);
      const uint32_t idxOff = be32((uint32_t)(uintptr_t)part->indices);
      const uint32_t vertOff = be32((uint32_t)(uintptr_t)part->vert);
      part->indices = (uint8_t*)basePtrIndices + idxOff;
      part->vert = (T3DVertPacked*)((uint8_t*)basePtrVertices + vertOff);

      /* Byteswap strip index buffers (s16 BE); keep local indices (do not DMEM-convert). */
      uint8_t* stripPtr = (uint8_t*)align_ptr(part->indices + part->numIndices, 8);
      for (int s = 0; s < 4; ++s) {
        if (part->numStripIndices[s] == 0) break;
        int16_t* idx = (int16_t*)stripPtr;
        for (uint16_t k = 0; k < part->numStripIndices[s]; k++)
          idx[k] = be16s(idx[k]);
        stripPtr = (uint8_t*)align_ptr(stripPtr + (size_t)part->numStripIndices[s] * 2u, 8);
      }
    }
  }

  printf("[p64] loaded model %s verts=%u chunks=%u\n", path, (unsigned)model->totalVertCount,
         (unsigned)model->chunkCount);
  return model;
}

void t3d_model_free(T3DModel* model)
{
  if (!model) return;
  free(model);
}

bool t3d_model_iter_next(T3DModelIter* iter)
{
  if (!iter || !iter->_model) return false;
  for (; iter->_idx < iter->_model->chunkCount; iter->_idx++) {
    if (T3D_CHUNK_TYPE(iter->_model->chunkOffsets[iter->_idx]) == iter->_chunkType) {
      uint32_t offset = T3D_CHUNK_OFF(iter->_model->chunkOffsets[iter->_idx]);
      iter->chunk = (char*)iter->_model + offset;
      iter->_idx++;
      return true;
    }
  }
  iter->chunk = nullptr;
  return false;
}

void t3d_model_draw_object(const T3DObject* obj, const T3DMat4FP*)
{
  if (!obj) return;
  if (s_recording) {
    if (s_recording->count < kBlockCmdMax)
      s_recording->objs[s_recording->count++] = (T3DObject*)obj;
    return;
  }
  soft_draw_object((T3DObject*)obj);
}

void t3d_mat4fp_from_srt(T3DMat4FP* mat, const float* scale, const float* rotQuat, const float* translate)
{
  s_model = mat_from_srt(scale, rotQuat, translate);
  if (mat)
    memset(mat, 0, sizeof(*mat)); /* FP unused on soft path */
}

void t3d_matrix_set(const T3DMat4FP*, bool)
{
  /* s_model already set by t3d_mat4fp_from_srt just before */
}

void t3d_matrix_push_pos(int)
{
  if (s_stack_top + 1 < kMatStackMax)
    s_stack[++s_stack_top] = s_model;
}

void t3d_matrix_pop(int count)
{
  while (count-- > 0 && s_stack_top > 0) {
    s_model = s_stack[s_stack_top--];
  }
}

void t3d_mat4_look_at(T3DMat4* out, const T3DVec3* eye, const T3DVec3* target, const T3DVec3* up)
{
  if (!out || !eye || !target || !up) return;
  Mat4 m = mat_look_at(eye->v, target->v, up->v);
  /* fm_mat4_t is column-major float[4][4] matching our Mat4 */
  memcpy(out, &m, sizeof(Mat4));
}

void t3d_viewport_set_perspective(T3DViewport* vp, float fov, float aspect, float n, float f)
{
  (void)vp;
  s_vp.proj = mat_perspective(fov, aspect, n, f);
  s_vp.ortho = false;
}

void t3d_viewport_set_ortho(T3DViewport* vp, float left, float right, float bottom, float top, float n, float f)
{
  (void)vp;
  s_vp.proj = mat_ortho(left, right, bottom, top, n, f);
  s_vp.ortho = true;
}

void t3d_viewport_set_view_matrix(T3DViewport* vp, const T3DMat4* mat)
{
  (void)vp;
  if (mat)
    memcpy(&s_vp.view, mat, sizeof(Mat4));
}

void t3d_viewport_attach(T3DViewport* vp)
{
  s_vp_attached = &s_vp;
  if (vp) {
    s_vp.ox = (int)vp->offset[0];
    s_vp.oy = (int)vp->offset[1];
    s_vp.w = (int)vp->size[0];
    s_vp.h = (int)vp->size[1];
    if (s_vp.w <= 0) s_vp.w = 320;
    if (s_vp.h <= 0) s_vp.h = 240;
  }
}

T3DViewport* t3d_viewport_get(void) { return (T3DViewport*)s_vp_attached; /* type-pun: only used for frustum on cull path */ }

void t3d_viewport_calc_viewspace_pos(T3DViewport*, T3DVec3* out, const T3DVec3* pos)
{
  if (!out) return;
  if (!pos) { *out = {}; return; }
  *out = *pos;
}

/* Soft rspq recording — store object draws, replay with current matrices */
} // extern C

extern "C" {

rspq_block_t* p64_dc_soft_block_end(void)
{
  SoftBlock* b = s_recording;
  s_recording = nullptr;
  return (rspq_block_t*)b;
}

void p64_dc_soft_block_begin(void)
{
  SoftBlock* b = (SoftBlock*)calloc(1, sizeof(SoftBlock));
  if (!b) {
    s_recording = nullptr;
    return;
  }
  s_recording = b;
}

void p64_dc_soft_block_run(rspq_block_t* block)
{
  SoftBlock* b = (SoftBlock*)block;
  if (!b) return;
  for (int i = 0; i < b->count; i++)
    soft_draw_object(b->objs[i]);
}

void p64_dc_soft_draw_model_direct(T3DModel* model)
{
  if (!model) return;
  T3DModelIter it = t3d_model_iter_create(model, T3D_CHUNK_TYPE_OBJECT);
  while (t3d_model_iter_next(&it))
    soft_draw_object(it.object);
}

} // extern "C"
