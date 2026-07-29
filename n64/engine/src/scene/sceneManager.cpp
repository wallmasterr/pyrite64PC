/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>

#include "scene/sceneManager.h"

#include <cstdio>

#include "scene/scene.h"
#include "script/globalScript.h"
#include "vi/swapChain.h"

#ifdef PLATFORM_PC
extern "C" void p64_pc_trace(const char* step);
#endif

namespace {
  constinit P64::Scene* currScene{nullptr};
  constinit uint32_t sceneId{0};
  constinit uint32_t nextSceneId{0};
  constinit uint8_t forceReload{0};
}

// "Private" methods only used in main.cpp
namespace P64::SceneManager
{
  void load(uint16_t newSceneId) {
    /* Editor builds filesystem/p64/s0001+ from data/scenes/<id>; id 0 => s0000 is never emitted. */
    if (newSceneId == 0) {
#ifdef PLATFORM_PC
      p64_pc_trace("SceneManager_load_0_clamped_to_1");
#endif
      std::fprintf(stderr, "[Pyrite64] SceneManager::load(0): no s0000; using scene 1. Fix boot scene or scripts.\n");
      newSceneId = 1;
    }
    nextSceneId = newSceneId;
  }

  void reload() {
    nextSceneId = sceneId;
    forceReload = 1;
  }

  Scene& getCurrent() {
    return *currScene;
  }

  void run()
  {
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_LOAD);

    sceneId = nextSceneId;
    currScene = new P64::Scene(sceneId, &currScene);

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_LOAD);

    while(sceneId == nextSceneId && !forceReload) {
      currScene->update(VI::SwapChain::getDeltaTime());
    }
    forceReload = 0;
  }

  void unload()
  {
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_UNLOAD);
    delete currScene;
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_UNLOAD);
    currScene = nullptr;
  }

#ifdef PLATFORM_PC
  /** Run one frame (PC build): load scene if needed, then one update(dt). */
  void runOneFrame(float dt)
  {
    if (nextSceneId != sceneId || !currScene) {
      p64_pc_trace("frame_unload");
      unload();
      p64_pc_trace("frame_PRE_LOAD_hooks");
      GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_LOAD);
      sceneId = nextSceneId;
      p64_pc_trace("frame_new_Scene");
      currScene = new P64::Scene(sceneId, &currScene);
      p64_pc_trace("frame_POST_LOAD_hooks");
      GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_LOAD);
    }
    p64_pc_trace("frame_update");
    if (currScene)
      currScene->update(dt);
    p64_pc_trace("frame_done");
  }

  Scene* getCurrentSceneOrNull() { return currScene; }
#endif
}
