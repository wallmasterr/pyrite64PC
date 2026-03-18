/**
 * PC platform layer implementation.
 * Loads assets from project_path/filesystem/p64/ (same layout as rom:/p64/).
 */
#include "pc_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define P64_PREFIX "rom:/p64/"
#define P64_PREFIX_LEN (sizeof(P64_PREFIX) - 1)

static char s_project_path[1024] = { 0 };

void p64_pc_set_project_path(const char* path)
{
    if (!path) {
        s_project_path[0] = '\0';
        return;
    }
    (void)snprintf(s_project_path, sizeof(s_project_path), "%.1000s", path);
}

const char* p64_pc_get_project_path(void)
{
    return s_project_path;
}

static int build_full_path(const char* path, char* out, size_t out_size)
{
    const char* sub = path;
    if (strncmp(path, P64_PREFIX, P64_PREFIX_LEN) == 0)
        sub = path + P64_PREFIX_LEN;
    if (s_project_path[0] == '\0')
        return 0;
#ifdef _WIN32
    return snprintf(out, out_size, "%s\\filesystem\\p64\\%s", s_project_path, sub) >= 0;
#else
    return snprintf(out, out_size, "%s/filesystem/p64/%s", s_project_path, sub) >= 0;
#endif
}

void* p64_pc_asset_load(const char* path, unsigned long* size_out)
{
    char full_path[2048];
    if (size_out)
        *size_out = 0;
    if (!path || !build_full_path(path, full_path, sizeof(full_path)))
        return NULL;

#ifdef _WIN32
    HANDLE h = CreateFileA(full_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    LARGE_INTEGER li;
    if (!GetFileSizeEx(h, &li) || li.QuadPart > 0x7FFFFFFF) {
        CloseHandle(h);
        return NULL;
    }
    size_t size = (size_t)li.QuadPart;
    void* buf = malloc(size + 1);
    if (!buf) {
        CloseHandle(h);
        return NULL;
    }
    DWORD read;
    if (!ReadFile(h, buf, (DWORD)size, &read, NULL) || read != (DWORD)size) {
        free(buf);
        CloseHandle(h);
        return NULL;
    }
    CloseHandle(h);
    ((char*)buf)[size] = '\0';
    if (size_out)
        *size_out = (unsigned long)size;
    return buf;
#else
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
    (void)fseek(f, 0, SEEK_SET);
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
#endif
}
