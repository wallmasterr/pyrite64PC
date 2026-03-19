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
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern "C" void p64_pc_trace(const char* step);

extern "C" void p64_engine_init(void);
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
    }
  }
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
#endif
