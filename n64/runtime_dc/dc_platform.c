/**
 * Dreamcast asset / trace helpers.
 * Disc layout (mkdcdisc -D <project>/filesystem): /cd/p64/...
 * Optional romdisk: /rd/p64/...
 */
#include "dc_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define P64_PREFIX "rom:/p64/"
#define P64_PREFIX_LEN (sizeof(P64_PREFIX) - 1)

static char s_asset_root[64] = "/cd";

void p64_pc_set_project_path(const char* path)
{
  if (!path || !path[0]) {
    snprintf(s_asset_root, sizeof(s_asset_root), "%s", "/cd");
    return;
  }
  snprintf(s_asset_root, sizeof(s_asset_root), "%.60s", path);
  size_t n = strlen(s_asset_root);
  while (n > 1 && s_asset_root[n - 1] == '/') {
    s_asset_root[--n] = '\0';
  }
}

int p64_pc_discover_project_path(char* path_io, size_t path_cap)
{
  static const char* candidates[] = { "/cd", "/rd", NULL };
  char conf[128];
  for (int i = 0; candidates[i]; ++i) {
    snprintf(conf, sizeof(conf), "%s/p64/conf", candidates[i]);
    FILE* f = fopen(conf, "rb");
    if (f) {
      fclose(f);
      if (path_io && path_cap > 0)
        snprintf(path_io, path_cap, "%s", candidates[i]);
      p64_pc_set_project_path(candidates[i]);
      return 1;
    }
  }
  return 0;
}

const char* p64_pc_get_project_path(void)
{
  return s_asset_root;
}

void p64_pc_trace(const char* step)
{
  if (!step) return;
  /* Serial / emulator console */
  printf("[p64] %s\n", step);
}

void p64_pc_alert_asset_missing(const char* rom_style_path)
{
  printf("[p64] missing asset: %s (root=%s)\n",
         rom_style_path ? rom_style_path : "?", s_asset_root);
}

static int build_full_path(const char* path, char* out, size_t out_size)
{
  const char* sub = path;
  if (!out || out_size == 0 || !path)
    return 0;
  if (strncmp(path, P64_PREFIX, P64_PREFIX_LEN) == 0)
    sub = path + P64_PREFIX_LEN;
  int n = snprintf(out, out_size, "%s/p64/%s", s_asset_root, sub);
  return n > 0 && (size_t)n < out_size;
}

static void* load_file(const char* full_path, unsigned long* size_out)
{
  FILE* f = fopen(full_path, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);
  void* buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (n != (size_t)sz) {
    free(buf);
    return NULL;
  }
  ((char*)buf)[n] = '\0';
  if (size_out)
    *size_out = (unsigned long)n;
  return buf;
}

void* p64_pc_asset_load(const char* path, unsigned long* size_out)
{
  char full[512];
  if (size_out)
    *size_out = 0;
  if (!path || !build_full_path(path, full, sizeof(full)))
    return NULL;

  void* buf = load_file(full, size_out);
  if (buf)
    return buf;

  /* Fallback: try the other mount if primary fails */
  if (strcmp(s_asset_root, "/cd") == 0)
    snprintf(full, sizeof(full), "/rd/p64/%s",
             (strncmp(path, P64_PREFIX, P64_PREFIX_LEN) == 0) ? path + P64_PREFIX_LEN : path);
  else
    snprintf(full, sizeof(full), "/cd/p64/%s",
             (strncmp(path, P64_PREFIX, P64_PREFIX_LEN) == 0) ? path + P64_PREFIX_LEN : path);
  return load_file(full, size_out);
}
