# Porting the Pyrite64 engine to the Windows (PC) build

This document outlines steps to hook the rest of the N64 game engine to the PC game export, so that "Build for PC" produces a runnable Windows build that loads and runs the same game (scenes, assets, scripts) using SDL3 + OpenGL instead of libdragon/N64.

## Steps (in order)

1. **Stub or implement N64 APIs for PC** ✅  
   Provide a PC implementation so engine code can load assets and use system services:
   - Map `asset_load("rom:/p64/...")` → `p64_pc_asset_load(path)` (load from `project_path/filesystem/p64/...`).
   - Add `p64_pc_set_project_path()` / `p64_pc_get_project_path()` and use them in the PC runtime main.
   - Later: stub or implement other libdragon/N64 symbols as needed (e.g. `sprite_load`, `get_ticks`, `joypad_*`, `rdpq_*`, etc.).

2. **PC SwapChain / game loop** ✅  
   - Run a draw callback each frame and present (SDL/OpenGL).
   - Drive delta time from SDL (e.g. `SDL_GetTicks()` or a timer) so the engine’s update loop runs at the right rate.

3. **Add engine (and game) sources to the PC build** ✅  
   - In the PC CMake, add the engine sources (and project-specific `src/p64`, etc.) with `PLATFORM_PC` (or similar) defined so N64-only code is excluded or stubbed.
   - Add a PC entry that runs the same high-level loop as N64 `main()` (init → load scene → update/render loop → shutdown).

4. **Rendering**  
   - Start with no-op `rdpq_*` and a single clear + present so the exe links and the game loop runs.
   - Optionally add real OpenGL rendering step by step (e.g. scene/entity geometry, then textures, then effects).

---

After each step you can build for PC from the editor and run the generated exe to verify (e.g. window opens, then scene loads, then rendering improves).
