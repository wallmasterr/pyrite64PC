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

static void trim_trailing_slashes(char* s)
{
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '/' || s[n - 1] == '\\')) {
        s[--n] = '\0';
    }
}

static int pc_conf_exists_at_root(const char* root)
{
    char p[2048];
    if (!root || !root[0])
        return 0;
#ifdef _WIN32
    if ((size_t)snprintf(p, sizeof p, "%s\\filesystem\\p64\\conf", root) >= sizeof p)
        return 0;
    {
        DWORD a = GetFileAttributesA(p);
        return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0);
    }
#else
    if ((size_t)snprintf(p, sizeof p, "%s/filesystem/p64/conf", root) >= sizeof p)
        return 0;
    {
        FILE* f = fopen(p, "rb");
        if (!f) return 0;
        fclose(f);
        return 1;
    }
#endif
}

#ifdef _WIN32
static int pc_get_exe_dir(char* out, size_t out_sz)
{
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_sz);
    if (n == 0 || n >= out_sz)
        return 0;
    {
        char* slash = strrchr(out, '\\');
        if (!slash) slash = strrchr(out, '/');
        if (slash) *slash = '\0';
    }
    return 1;
}

static int try_read_project_marker(const char* exe_dir, char* out, size_t out_sz)
{
    char path[MAX_PATH + 32];
    FILE* f;
    char line[1024];
    char* start;
    char* end;
    size_t len;

    if (!exe_dir || !out || out_sz < 2)
        return 0;
    if ((size_t)snprintf(path, sizeof path, "%s\\p64_project_root.txt", exe_dir) >= sizeof path)
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (!fgets(line, (int)sizeof line, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    start = line;
    while (*start && (unsigned char)*start <= 32) start++;
    end = start + strlen(start);
    while (end > start && (unsigned char)end[-1] <= 32) *--end = '\0';
    if (end == start)
        return 0;
    len = (size_t)(end - start);
    if (len >= out_sz)
        len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}
#endif

void p64_pc_set_project_path(const char* path)
{
    if (!path) {
        s_project_path[0] = '\0';
        return;
    }
    (void)snprintf(s_project_path, sizeof(s_project_path), "%.1000s", path);
    trim_trailing_slashes(s_project_path);
}

int p64_pc_discover_project_path(char* path_io, size_t path_cap)
{
#ifdef _WIN32
    char marker[1024];
    char exe_dir[MAX_PATH];
    char parent[MAX_PATH * 2];
    char canon[MAX_PATH * 2];
#endif

    if (!path_io || path_cap < 4)
        return 0;
    trim_trailing_slashes(path_io);
    if (path_io[0] && pc_conf_exists_at_root(path_io))
        return 1;

#ifdef _WIN32
    if (!pc_get_exe_dir(exe_dir, sizeof exe_dir))
        return 0;

    if (try_read_project_marker(exe_dir, marker, sizeof marker)) {
        trim_trailing_slashes(marker);
        if (marker[0] && pc_conf_exists_at_root(marker)) {
            (void)snprintf(path_io, path_cap, "%s", marker);
            return 1;
        }
    }

    if ((size_t)snprintf(parent, sizeof parent, "%s\\..", exe_dir) >= sizeof parent)
        return 0;
    if (GetFullPathNameA(parent, (DWORD)sizeof canon, canon, NULL) && canon[0]) {
        trim_trailing_slashes(canon);
        if (pc_conf_exists_at_root(canon)) {
            (void)snprintf(path_io, path_cap, "%s", canon);
            return 1;
        }
    }

    if (pc_conf_exists_at_root(exe_dir)) {
        (void)snprintf(path_io, path_cap, "%s", exe_dir);
        return 1;
    }
#endif
    return 0;
}

const char* p64_pc_get_project_path(void)
{
    return s_project_path;
}

void p64_pc_trace(const char* step)
{
    FILE* f = fopen("p64_pc_trace.log", "a");
    if (f) {
        fputs(step, f);
        fputc('\n', f);
        fclose(f);
    }
}

static int build_full_path(const char* path, char* out, size_t out_size)
{
    const char* sub = path;
    int n;
    if (!out || out_size == 0)
        return 0;
    if (strncmp(path, P64_PREFIX, P64_PREFIX_LEN) == 0)
        sub = path + P64_PREFIX_LEN;
    if (s_project_path[0] == '\0')
        return 0;
#ifdef _WIN32
    n = snprintf(out, out_size, "%s\\filesystem\\p64\\%s", s_project_path, sub);
#else
    n = snprintf(out, out_size, "%s/filesystem/p64/%s", s_project_path, sub);
#endif
    if (n < 0 || (size_t)n >= out_size)
        return 0;
    return 1;
}

#ifdef _WIN32
static void log_asset_open_fail(const char* rom_path, const char* full_path, DWORD err)
{
    FILE* f = fopen("p64_pc_asset_errors.log", "a");
    if (f) {
        fprintf(f, "FAIL rom=%s\n     disk=%s\n     GetLastError=%lu (0x%lX)\n", rom_path ? rom_path : "?",
                full_path ? full_path : "?", (unsigned long)err, (unsigned long)err);
        fclose(f);
    }
}
#endif

void p64_pc_alert_asset_missing(const char* rom_style_path)
{
#ifdef _WIN32
    char full[2048];
    char msg[2800];
    if (!rom_style_path)
        return;
    if (!build_full_path(rom_style_path, full, sizeof full)) {
        MessageBoxA(NULL,
                    "Could not build a disk path for this asset (project path empty, or full path too long).\n\n"
                    "Check p64_pc_get_project_path / run the .exe with your game folder as argv[1].",
                    "Pyrite64 PC — asset path error",
                    MB_OK | MB_ICONERROR);
        return;
    }
    (void)snprintf(msg, sizeof msg,
                   "The PC runtime could not load this file:\n\n"
                   "  %s\n\n"
                   "It tried this full path:\n\n"
                   "  %s\n\n"
                   "Typical fixes:\n"
                   "• Run \"Build\" / \"Build for PC\" in Pyrite64 so filesystem\\p64\\ is regenerated.\n"
                   "• In Project Settings, set \"scene on boot\" to an ID that exists (see data/scenes\\<id>).\n"
                   "• Run:  yourgame_pc.exe \"C:\\\\path\\\\to\\\\yourgame\"\n",
                   rom_style_path, full);
    MessageBoxA(NULL, msg, "Pyrite64 PC — scene / asset missing", MB_OK | MB_ICONERROR);
#else
    (void)rom_style_path;
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
    if (h == INVALID_HANDLE_VALUE) {
        log_asset_open_fail(path, full_path, GetLastError());
        return NULL;
    }
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
