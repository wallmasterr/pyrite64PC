/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#ifdef PLATFORM_PC
#include <pc_compat.h>
#endif
#include <libdragon.h>

#include "assets/assetManager.h"

namespace P64
{
  class Object;
}

namespace P64::NodeGraph
{
  typedef void (*GraphFunc)(void* arg);

  struct GraphDef;
  struct NodeDef;

  class Instance
  {
    private:
      GraphDef* graphDef{};
      coroutine_t *corot{};

    public:
      Object *object{};
      uint32_t args[2]{};
      uint16_t asset{};
      uint8_t repeatable{};

      Instance() = default;
      ~Instance();

      void load(uint16_t assetIdx);
      bool update(float deltaTime);
  };

  typedef int(*UserFunc)(uint32_t);

  void registerFunction(uint32_t strCRC32, UserFunc fn);
  UserFunc getFunction(uint64_t uuid);
}
