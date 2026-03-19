/**
 * Minimal PC runtime for Pyrite64 projects - SDL3 + OpenGL (Windows).
 * Uses desktop OpenGL on Windows (no GLES2 headers required). Loads project data from P64_PROJECT_PATH.
 */
#define SDL_MAIN_HANDLED  /* we use our own main(), not SDL's WinMain */
#include "pc_platform.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_hints.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef P64_PROJECT_PATH
#define P64_PROJECT_PATH "."
#endif

static void showError(const char* title, const char* msg) {
    fprintf(stderr, "%s: %s\n", title, msg);
#ifdef _WIN32
    MessageBoxA(nullptr, msg, title, MB_OK | MB_ICONERROR);
#else
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, msg, nullptr);
#endif
}

extern "C" void p64_engine_init(void);
extern "C" void p64_engine_run_frame(float dt);
extern "C" void p64_engine_shutdown(void);

/** Called every frame with delta time in seconds. Runs engine update. */
static void p64_pc_update(float dt) {
    p64_engine_run_frame(dt);
}

static GLuint s_displayTex = 0;

/** Upload engine display buffer and draw fullscreen quad. Fallback: clear + triangle. */
static void p64_pc_draw(void) {
    unsigned char* buf = NULL;
    int w = 0, h = 0, stride = 0;
    p64_pc_get_display_buffer(&buf, &w, &h, &stride);

    if (buf && w > 0 && h > 0) {
        if (s_displayTex == 0)
            glGenTextures(1, &s_displayTex);
        glBindTexture(GL_TEXTURE_2D, s_displayTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (stride == w * 4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
        } else {
            /* copy row-by-row if stride != w*4 */
            unsigned char* row = (unsigned char*)malloc((size_t)(w * 4));
            if (row) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++)
                        ((uint32_t*)row)[x] = ((uint32_t*)(buf + (size_t)y * stride))[x];
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, (GLsizei)w, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
                }
                free(row);
            }
        }
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, s_displayTex);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 640, 480, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(640.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(640.0f, 480.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 480.0f);
        glEnd();
        return;
    }

    /* Fallback: clear to scene color + test triangle */
    unsigned char r = 0, g = 0, b = 0, a = 255;
    p64_pc_get_clear_color_rgba8(&r, &g, &b, &a);
    if (r == 0 && g == 0 && b == 0) { r = 51; g = 89; b = 128; a = 255; }
    glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.4f, 0.2f);
    glVertex2f(40.0f, 440.0f);
    glVertex2f(40.0f, 400.0f);
    glVertex2f(80.0f, 420.0f);
    glEnd();
}

int main(int argc, char* argv[])
{
    /* Required when using SDL_MAIN_HANDLED: tell SDL the app entry point is ready. */
    SDL_SetMainReady();

    const char* projectPath = P64_PROJECT_PATH;
    if (argc > 1)
        projectPath = argv[1];
    p64_pc_set_project_path(projectPath);

#ifdef _WIN32
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "windows");
#endif

    /* Single init: skip SDL_Init(0) which can fail with no error set on some setups. */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        char errbuf[384];
        const char* e = SDL_GetError();
        if (e && e[0]) {
            (void)snprintf(errbuf, sizeof(errbuf), "%.350s", e);
        } else {
#ifdef _WIN32
            (void)snprintf(errbuf, sizeof(errbuf), "SDL_Init(VIDEO) failed (no error set). GetLastError=0x%08lX", (unsigned long)GetLastError());
#else
            (void)snprintf(errbuf, sizeof(errbuf), "SDL_Init(VIDEO) failed (no error set).");
#endif
        }
        showError("SDL_Init failed", errbuf);
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("Pyrite64 PC", 640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        showError("SDL_CreateWindow failed", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        showError("OpenGL context failed", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    printf("Project path: %s\n", projectPath);
    p64_engine_init();
    printf("Engine inited. Game loop running. Close window or ESC to exit.\n");

    /* Game loop: delta time from SDL, then update(dt) and draw() each frame. */
    Uint64 lastTicks = SDL_GetTicks();
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = false;
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE)
                running = false;
        }

        Uint64 now = SDL_GetTicks();
        float dt = (float)(now - lastTicks) / 1000.0f;
        lastTicks = now;
        /* Cap delta so one long frame (e.g. after pause) doesn't jump the sim. */
        if (dt > 0.25f)
            dt = 0.25f;

        p64_pc_update(dt);
        p64_pc_draw();
        SDL_GL_SwapWindow(window);
    }

    p64_engine_shutdown();
    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
