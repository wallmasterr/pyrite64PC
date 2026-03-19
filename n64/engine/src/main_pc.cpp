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
#include <cstring>

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
  P64::LD::init();
  asset_init_compression(2);
  dfs_init(DFS_DEFAULT_LOCATION);
  rdpq_init();
  t3d_init({});
  tpx_init({});
  joypad_init();

  P64::AssetManager::init();
  P64::AudioManager::init();

  /* N64 VI (video interface) not used on PC; display is handled by SDL. */

  {
    void* tmp = asset_load("rom:/p64/conf", nullptr);
    if (tmp) {
      std::memcpy(&s_projectConf, tmp, sizeof(ProjectConf));
      free(tmp);
    }
  }

  for (uint32_t fontIdx = 0; fontIdx < s_projectConf.autoLoadFonts.size(); fontIdx++) {
    if (s_projectConf.autoLoadFonts[fontIdx] < 0xFFFF) {
      void* font = P64::AssetManager::getByIndex(s_projectConf.autoLoadFonts[fontIdx]);
      rdpq_text_register_font(fontIdx, (rdpq_font_t*)font);
    }
  }

  P64::DrawLayer::reset();
  P64::MatrixManager::reset();
  P64::VI::SwapChain::init();

  P64::GlobalScript::callHooks(P64::GlobalScript::HookType::GAME_INIT);

  uint16_t sceneId = (sys_reset_type() == RESET_COLD)
    ? (uint16_t)s_projectConf.sceneIdOnBoot
    : (uint16_t)s_projectConf.sceneIdOnReset;
  P64::SceneManager::load(sceneId);
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
