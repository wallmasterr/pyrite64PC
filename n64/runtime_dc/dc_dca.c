/**
 * Libdragon DCA5 → raw bytes (LZ4 algo 1). Used by DC asset_load.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4.h"

static uint64_t dca_read_varint(const uint8_t** p)
{
  uint64_t x = 0;
  int s = 0;
  for (;;) {
    uint8_t b = *(*p)++;
    x |= (uint64_t)(b & 0x7Fu) << s;
    if (!(b & 0x80u))
      return x;
    s += 7;
  }
}

/**
 * If `in` is DCA5/LZ4, decompress into a new buffer and free `in`.
 * Otherwise return `in` unchanged. Sets *out_size to final size.
 */
void* p64_dc_maybe_decompress_asset(void* in, unsigned long in_size, unsigned long* out_size)
{
  if (out_size)
    *out_size = in_size;
  if (!in || in_size < 8)
    return in;

  const uint8_t* d = (const uint8_t*)in;
  if (d[0] != 'D' || d[1] != 'C' || d[2] != 'A' || d[3] != '5')
    return in;

  const uint8_t flags = d[4];
  const int algo = (flags >> 4) & 3;
  if (algo != 1) /* only LZ4 for now */
    return in;

  const uint8_t* p = d + 5;
  const uint32_t cmp_size = (uint32_t)dca_read_varint(&p);
  const uint32_t orig_size = (uint32_t)dca_read_varint(&p);
  (void)dca_read_varint(&p); /* margin */
  if (((size_t)(p - d) & 1u) != 0)
    p++;

  const int payload = (int)(in_size - (unsigned long)(p - d));
  if (payload <= 0 || (uint32_t)payload < cmp_size)
    return in;

  uint8_t* out = (uint8_t*)malloc((size_t)orig_size + 16u);
  if (!out)
    return in;

  const int n = LZ4_decompress_safe((const char*)p, (char*)out, (int)cmp_size, (int)orig_size);
  if (n <= 0 || (uint32_t)n != orig_size) {
    free(out);
    return in;
  }

  free(in);
  if (out_size)
    *out_size = (unsigned long)n;
  return out;
}
