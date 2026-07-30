#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lz4.h"

static uint64_t read_varint(const uint8_t** p)
{
  uint64_t x = 0;
  int s = 0;
  for (;;) {
    uint8_t b = *(*p)++;
    x |= (uint64_t)(b & 0x7f) << s;
    if (!(b & 0x80)) return x;
    s += 7;
  }
}

int main(void)
{
  FILE* f = fopen("C:/Users/PC/Documents/pyrite64/testdc/filesystem/box.t3dm", "rb");
  if (!f) { perror("open"); return 1; }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  rewind(f);
  uint8_t* d = (uint8_t*)malloc((size_t)n);
  fread(d, 1, (size_t)n, f);
  fclose(f);

  const uint8_t* p = d + 5;
  uint32_t cmp = (uint32_t)read_varint(&p);
  uint32_t orig = (uint32_t)read_varint(&p);
  (void)read_varint(&p);
  if (((p - d) & 1)) p++;
  int cmp_len = (int)(d + n - p);
  uint8_t* out = (uint8_t*)malloc(orig + 64);
  int r = LZ4_decompress_safe((const char*)p, (char*)out, cmp_len, (int)orig);
  printf("cmp=%u orig=%u hdr=%ld payload=%d r=%d\n", cmp, orig, (long)(p - d), cmp_len, r);
  if (r > 0) {
    printf("magic=%.4s\n", (char*)out);
    FILE* o = fopen("C:/Users/PC/Documents/pyrite64/testdc/build-dc/box_raw.t3dm", "wb");
    fwrite(out, 1, (size_t)r, o);
    fclose(o);
  }
  return r > 0 ? 0 : 1;
}
