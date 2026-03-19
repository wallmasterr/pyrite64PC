/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>

namespace P64
{
  class Scene;
}

/**
 * Functions to manager scenes.
 */
namespace P64::SceneManager
{
  /**
   * Request loading a scene by ID.
   * Note that the actual load will happen at the end of the current frame.
   * If this function was called multiple times, the last ID will be used.
   * @param sceneId scene to load
   */
  void load(uint16_t newSceneId);

  /**
   * Returns the current scene.
   * @return scene, never NULL
   */
  Scene &getCurrent();

#ifdef PLATFORM_PC
  /** Run one frame (load scene if needed, then one update). */
  void runOneFrame(float dt);
#endif
}