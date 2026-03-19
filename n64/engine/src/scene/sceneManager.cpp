/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>

#include "scene/sceneManager.h"

#include "scene/scene.h"
#include "script/globalScript.h"
#include "vi/swapChain.h"

namespace {
  constinit P64::Scene* currScene{nullptr};
  constinit uint32_t sceneId{0};
  constinit uint32_t nextSceneId{0};
}

void P64::SceneManager::load(uint16_t newSceneId) {
  nextSceneId = newSceneId;
}

P64::Scene& P64::SceneManager::getCurrent() {
  return *currScene;
}

// "Private" methods only used in main.cpp
namespace P64::SceneManager
{
  void run()
  {
    GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_LOAD);

    sceneId = nextSceneId;
    currScene = new P64::Scene(sceneId, &currScene);

    GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_LOAD);

    while(sceneId == nextSceneId) {
      currScene->update(VI::SwapChain::getDeltaTime());
    }
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
      unload();
      GlobalScript::callHooks(GlobalScript::HookType::SCENE_PRE_LOAD);
      sceneId = nextSceneId;
      currScene = new P64::Scene(sceneId, &currScene);
      GlobalScript::callHooks(GlobalScript::HookType::SCENE_POST_LOAD);
    }
    if (currScene)
      currScene->update(dt);
  }
#endif
}