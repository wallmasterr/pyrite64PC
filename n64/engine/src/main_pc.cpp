/**
 * PC build entry: init, run one frame, shutdown. Called from runtime_pc main.
 */
#ifdef PLATFORM_PC
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/tpx.h>
#include "../include/scene/sceneManager.h"
#include "vi/swapChain.h"
#include "lib/logger.h"
#include "lib/matrixManager.h"
#include "scene/globalState.h"
#include "assets/assetManager.h"
#include "audio/audioManager.h"
#include "audio/audioManagerPrivate.h"
#include "libdragon/utils.h"
#include "renderer/drawLayer.h"
#include "script/globalScript.h"
#include "scene/globalState.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "pc_platform.h"

extern "C" void p64_pc_trace(const char* step);

extern "C" void p64_engine_init(void);
extern "C" void p64_pc_get_clear_color_rgba8(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a);
extern "C" void p64_pc_get_display_buffer(uint8_t** out_ptr, int* out_w, int* out_h, int* out_stride);
extern "C" void p64_engine_run_frame(float dt);
extern "C" void p64_engine_shutdown(void);

P64::GlobalState P64::state{};

namespace {
  struct ProjectConf {
    uint32_t sceneIdOnBoot{};
    uint32_t sceneIdOnReset{};
    std::array<uint16_t, 16> autoLoadFonts{};
  };
  static ProjectConf s_projectConf{};
}

void p64_engine_init(void)
{
  /* Fresh trace file each run; last line = step before crash */
  { FILE* f = fopen("p64_pc_trace.log", "w"); if (f) fclose(f); }
  p64_pc_trace("init_start");
  P64::LD::init();
  p64_pc_trace("LD_init");
  asset_init_compression(2);
  dfs_init(DFS_DEFAULT_LOCATION);
  rdpq_init();
  t3d_init({});
  tpx_init({});
  joypad_init();
  p64_pc_trace("joypad_init");

  P64::AssetManager::init();
  p64_pc_trace("AssetManager_init");
  P64::AudioManager::init();
  p64_pc_trace("AudioManager_init");

  {
    void* tmp = asset_load("rom:/p64/conf", nullptr);
    if (tmp) {
      std::memcpy(&s_projectConf, tmp, sizeof(ProjectConf));
      free(tmp);
    } else {
      /* Matches src/project/project.h default when conf is missing (avoids scene 0 / s0000 vs editor scenes starting at 1). */
      p64_pc_trace("conf_missing_scene_defaults_1");
      s_projectConf.sceneIdOnBoot = 1;
      s_projectConf.sceneIdOnReset = 1;
    }
  }
  /* Editor creates scenes under data/scenes/<id> starting at 1; conf with boot/reset 0 looks for s0000 which is never built. */
  if (s_projectConf.sceneIdOnBoot == 0u) {
    p64_pc_trace("conf_boot_scene_0_clamped_to_1");
    s_projectConf.sceneIdOnBoot = 1u;
  }
  if (s_projectConf.sceneIdOnReset == 0u)
    s_projectConf.sceneIdOnReset = 1u;

  p64_pc_trace("conf_load");

  for (uint32_t fontIdx = 0; fontIdx < s_projectConf.autoLoadFonts.size(); fontIdx++) {
    if (s_projectConf.autoLoadFonts[fontIdx] < 0xFFFF) {
      void* font = P64::AssetManager::getByIndex(s_projectConf.autoLoadFonts[fontIdx]);
      if (font)
        rdpq_text_register_font(fontIdx, (rdpq_font_t*)font);
    }
  }
  p64_pc_trace("fonts");

  P64::DrawLayer::reset();
  P64::MatrixManager::reset();
  P64::VI::SwapChain::init();
  p64_pc_trace("SwapChain_init");

  P64::GlobalScript::callHooks(P64::GlobalScript::HookType::GAME_INIT);
  p64_pc_trace("GAME_INIT_hooks");

  uint16_t sceneId = (sys_reset_type() == RESET_COLD)
    ? (uint16_t)s_projectConf.sceneIdOnBoot
    : (uint16_t)s_projectConf.sceneIdOnReset;
  if (sceneId == 0u)
    sceneId = 1u; /* match SceneManager::load(); id 0 has no s0000 asset */
  {
    char line[96];
    std::snprintf(line, sizeof line, "SceneManager_load_scene_%u", (unsigned)sceneId);
    p64_pc_trace(line);
  }
  P64::SceneManager::load(sceneId);
  p64_pc_trace("init_done");
}

void p64_engine_run_frame(float dt)
{
  P64::VI::SwapChain::setPCDeltaTime(dt);
  P64::SceneManager::runOneFrame(dt);
  P64::VI::SwapChain::drain();
}

void p64_engine_shutdown(void)
{
  (void)0; /* TODO: SceneManager::unload when exposed for PC */
}

void p64_pc_get_clear_color_rgba8(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a)
{
  if (!r || !g || !b || !a) return;
  P64::Scene* sc = P64::SceneManager::getCurrentSceneOrNull();
  if (!sc) {
    *r = *g = *b = 0;
    *a = 255;
    return;
  }
  color_t c = sc->getConf().clearColor;
  *r = c.r;
  *g = c.g;
  *b = c.b;
  *a = c.a;
}

void p64_pc_get_display_buffer(uint8_t** out_ptr, int* out_w, int* out_h, int* out_stride)
{
  if (!out_ptr) return;
  *out_ptr = nullptr;
  P64::VI::SwapChain::getDisplayBuffer(out_ptr, out_w, out_h, out_stride);
}
#endif
