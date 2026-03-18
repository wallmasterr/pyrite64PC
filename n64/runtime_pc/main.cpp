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

/** Called every frame with delta time in seconds. Plug engine update here (step 3). */
static void p64_pc_update(float dt) {
    (void)dt;
    /* No-op until engine is wired in. */
}

/** Called every frame after update. Do all rendering and present. Plug engine draw here (step 3). */
static void p64_pc_draw(void) {
    glClearColor(0.15f, 0.2f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Placeholder until engine/rendering is wired in. */
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
    printf("OpenGL context created. Game loop running (delta time + update/draw). Close window or ESC to exit.\n");

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

    SDL_GL_DestroyContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
