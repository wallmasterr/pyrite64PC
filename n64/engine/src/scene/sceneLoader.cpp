/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include <libdragon.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <cstring>
#include "scene/scene.h"
#include "lib/math.h"
#include "lib/logger.h"
#include "scene/componentTable.h"

#ifdef PLATFORM_PC
#include "pc_platform.h"
#endif

namespace {
  constexpr uint32_t DATA_ALIGN = 8;

  /** When fileEnd is set, all reads must stay within [objFile, fileEnd). */
  inline bool blob_has(const uint8_t* p, size_t nbytes, const uint8_t* fileEnd)
  {
    if (!fileEnd)
      return true;
    return p + nbytes <= fileEnd;
  }

  struct ObjectEntry {
    uint16_t flags;
    uint16_t id;
    uint16_t group;
    uint16_t _padding;
    fm_vec3_t pos;
    fm_vec3_t scale;
    uint32_t packedRot;
    // data follows
  };

  struct __attribute__((packed)) ObjectEntryCamera : public ObjectEntry {
    uint16_t _padding;
    fm_vec3_t pos{};
    fm_quat_t rot{};
    float fov{};
    float near{};
    float far{};
    int16_t vpOffset[2]{};
    int16_t vpSize[2]{};
  };

  /* rom:/p64/sNNNN + suffix; snprintf avoids 4-digit / id > 999 edge cases */
  char scenePath[48]{};

  inline void formatScenePathUnderscore(uint16_t sceneId)
  {
    (void)std::snprintf(scenePath, sizeof(scenePath), "rom:/p64/s%04u_", (unsigned)sceneId);
  }

  inline void* loadSubFile(char type)
  {
    const size_t len = std::strlen(scenePath);
    if (len + 2 >= sizeof(scenePath))
      return nullptr;
    scenePath[len] = type;
    scenePath[len + 1] = '\0';
    void* r = asset_load(scenePath, nullptr);
    scenePath[len] = '\0';
    return r;
  }
}

void P64::Scene::loadSceneConfig()
{
  formatScenePathUnderscore(id);
  scenePath[std::strlen(scenePath) - 1] = '\0'; /* drop trailing '_' */

  auto applyFallback = [this]() {
#ifdef PLATFORM_PC
    p64_pc_trace("Scene_config_missing");
    p64_pc_alert_asset_missing(scenePath);
#endif
    Log::error("Scene config not found or invalid: %s (project path may be wrong; pass game project path as first argument)", scenePath);
    conf = SceneConf{};
    conf.pipeline = SceneConf::Pipeline::DEFAULT;
    conf.screenWidth = 640;
    conf.screenHeight = 480;
    conf.flags = SceneConf::FLAG_CLR_COLOR;
    conf.clearColor = RGBA32(96, 32, 128, 255);
    conf.audioFreq = 44100;
    conf.layerSetup.layerCount3D = 1; /* DrawLayer::init requires layerCount > 0 */
  };

  int sz = 0;
  void* raw = asset_load(scenePath, &sz);
  /* Scene file is shorter than sizeof(SceneConf): only layers present in the project are written.
   * Never read past the allocated buffer (assigning *SceneConf* was heap over-read -> crash on PC). */
  constexpr int kSceneHeaderBytes = 28; /* up to and including layer count quad in build output */
  if (!raw || sz < kSceneHeaderBytes) {
    if (raw)
      free(raw);
    applyFallback();
    return;
  }

  /* Zero via value-init (SceneConf is non-trivial; memset trips -Wclass-memaccess). */
  conf = SceneConf{};
  {
    const size_t ncopy = (size_t)sz < sizeof(conf) ? (size_t)sz : sizeof(conf);
    std::memcpy(static_cast<void*>(&conf), raw, ncopy);
  }
  free(raw);

  const uint32_t layerCount = (uint32_t)conf.layerSetup.layerCount3D + conf.layerSetup.layerCountPtx + conf.layerSetup.layerCount2D;
  if (layerCount == 0u) {
#ifdef PLATFORM_PC
    p64_pc_trace("Scene_config_layer_count_zero");
#endif
    applyFallback();
    return;
  }
  /* Each serialized layer is 24 bytes in sceneBuilder; trailing align(4) can add up to 3 bytes. */
  {
    const size_t need = (size_t)kSceneHeaderBytes + (size_t)layerCount * 24u;
    if ((size_t)sz < need) {
#ifdef PLATFORM_PC
      p64_pc_trace("Scene_config_truncated");
#endif
      applyFallback();
    }
  }
}

P64::Object* P64::Scene::loadObject(uint8_t* &objFile, std::function<void(Object&)> callback, bool deferComponentInit,
                                    const uint8_t* fileEnd)
{
  auto fail = [&]() {
#ifdef PLATFORM_PC
    p64_pc_trace("loadObject_bounds_or_alloc_fail");
#endif
    if (fileEnd)
      objFile = (uint8_t*)fileEnd;
    return nullptr;
  };

  if (!blob_has(objFile, sizeof(ObjectEntry), fileEnd))
    return fail();

  ObjectEntry* objEntry = (ObjectEntry*)objFile;

  // pre-scan components to get total allocation size
  uint32_t allocSize = sizeof(Object);

  // some alignment logic below relies on an at a minimum 4-byte size
  static_assert(sizeof(Object) % 4 == 0);
  static_assert(sizeof(Object::CompRef) % 4 == 0);

  auto ptrIn = objFile + sizeof(ObjectEntry);
  uint32_t compCount = 0;
  uint32_t compDataSize = 0;
  while (true) {
    if (!blob_has(ptrIn, 2, fileEnd))
      return fail();
    if (ptrIn[1] == 0)
      break;
    const auto compId = ptrIn[0];
    const uint32_t argSize = (uint32_t)ptrIn[1] * 4u;
    if (argSize < 4u)
      return fail();
    if (!blob_has(ptrIn, argSize, fileEnd))
      return fail();

    assertf(compId < COMP_TABLE_SIZE, "Invalid component ID %d!", compId);
    const auto &compDef = COMP_TABLE[compId];
    assertf(compDef.getAllocSize != nullptr, "Component %d unknown!", compId);
    if (!blob_has(ptrIn + 4, argSize - 4u, fileEnd))
      return fail();
    compDataSize += Math::alignUp(compDef.getAllocSize(ptrIn + 4), DATA_ALIGN);
    allocSize += sizeof(Object::CompRef);

    ptrIn += argSize;
    ++compCount;
  }

  // component data must be 8-byte aligned, GCC tries to be smart
  // and some structs cuse 64-bit writes to members.
  // if it is misaligned, add spacing after the comp table
  uint32_t offsetData = (sizeof(Object::CompRef) * compCount);
  if(allocSize % 8 != 0) {
    compDataSize += 4;
    offsetData += 4;
  }

  allocSize += compDataSize;

  //debugf("Allocating object %d | comps: %d | size: %lu bytes\n", objEntry->id, compCount, allocSize);

  void* objMem;
#ifdef PLATFORM_PC
  objMem = malloc(allocSize);
  if(objMem) memset(objMem, 0, allocSize);
#else
  objMem = memalign(DATA_ALIGN, allocSize); // @TODO: custom allocator
  if (!objMem)
    return fail();
  memObjects += malloc_usable_size(objMem);
  if(allocSize < 16) {
    memset(objMem, 0, allocSize);
  } else {
    sys_hw_memset(objMem, 0, allocSize);
  }
#endif
  if(!objMem)
    return fail();

  auto objCompTablePtr = (Object::CompRef*)((char*)objMem + sizeof(Object));
  auto objCompDataPtr = (char*)(objCompTablePtr) + offsetData;

  Object* obj = new(objMem) Object();
  obj->id = objEntry->id;
  obj->group = objEntry->group;
  obj->flags = objEntry->flags;
  obj->compCount = compCount;
  obj->pos = objEntry->pos;
  obj->scale = objEntry->scale;
  obj->rot = Math::unpackQuat(objEntry->packedRot);

  if(callback)callback(*obj);

  ptrIn = objFile + sizeof(ObjectEntry);
  const size_t pendingStart = pendingCompInit.size();
  auto failAfterAlloc = [&]() -> Object* {
    if (pendingCompInit.size() > pendingStart) {
      pendingCompInit.erase(pendingCompInit.begin() + (ptrdiff_t)pendingStart, pendingCompInit.end());
    }
    obj->~Object();
    free(objMem);
    return fail();
  };
  while (ptrIn[1] != 0)
  {
    if (!blob_has(ptrIn, 2, fileEnd))
      return failAfterAlloc();
    const uint8_t compId = ptrIn[0];
    const uint32_t argSize = (uint32_t)ptrIn[1] * 4u;
    if (argSize < 4u || !blob_has(ptrIn, argSize, fileEnd))
      return failAfterAlloc();

    const auto &compDef = COMP_TABLE[compId];
    // debugf("Alloc: comp %d (arg: %d)\n", compId, argSize);

    objCompTablePtr->type = compId;
    objCompTablePtr->flags = 0;
    objCompTablePtr->offset = objCompDataPtr - (char*)obj;
    ++objCompTablePtr;

    if(deferComponentInit)
    {
      auto &pending = pendingCompInit.emplace_back();
      pending.obj = obj;
      pending.dataPtr = objCompDataPtr;
      pending.compId = compId;
      pending.initData = ptrIn + 4;
    }
    else
    {
      compDef.initDel(*obj, objCompDataPtr, ptrIn + 4);
    }

    objCompDataPtr += Math::alignUp(compDef.getAllocSize(ptrIn + 4), 8);
    ptrIn += argSize;

    // send ready event. this is deferred, so it will always happen after 'initDel'
  }
  sendEvent(obj->id, 0, EVENT_TYPE_READY, 0);

  if (!blob_has(ptrIn, 4, fileEnd))
    return failAfterAlloc();
  objFile = ptrIn + 4;

  objects.push_back(obj);
  if(obj->id < idLookup.size()) {
    idLookup[obj->id] = obj;
  }

  return obj;
}

void P64::Scene::runPendingComponentInit()
{
  for(auto &pending : pendingCompInit)
  {
    const auto &compDef = COMP_TABLE[pending.compId];
    compDef.initDel(*pending.obj, pending.dataPtr, pending.initData);
  }
  pendingCompInit.clear();
}

void P64::Scene::loadScene() {
  formatScenePathUnderscore(id);
  scenePath[std::strlen(scenePath) - 1] = '\0';

  cameras.clear();

  //debugf("Objects: %lu\n", conf.objectCount);
  if(conf.objectCount)
  {
#ifdef PLATFORM_PC
    p64_pc_trace("Scene_loadScene_objects");
#endif
    const size_t sl = std::strlen(scenePath);
    scenePath[sl] = 'o';
    scenePath[sl + 1] = '\0';
    int objBlobSz = 0;
    auto *objFileStart = (uint8_t*)asset_load(scenePath, &objBlobSz);
    scenePath[sl] = '\0';

    if (!objFileStart) {
      Log::error("Scene object file not found (rom:/p64/s%04uo). Skipping objects.", id);
    } else {
      const uint8_t* objFileEnd = objBlobSz > 0 ? objFileStart + (size_t)objBlobSz : objFileStart;
      auto objFile = objFileStart;
      bool parseStoppedEarly = false;
      for (uint32_t i = 0; i < conf.objectCount; ++i) {
        if (!loadObject(objFile, {}, true, objFileEnd)) {
          Log::error("Scene object load failed at %u / %u (truncated or corrupt s%04u o file?)", i, conf.objectCount, id);
#ifdef PLATFORM_PC
          p64_pc_trace("Scene_loadScene_obj_stopped_early");
#endif
          parseStoppedEarly = true;
          break;
        }
      }

      std::function<void(const Object* parent, Object& obj)> updateStates = [&](const Object* parent, Object& obj)
      {
        obj.setFlag(ObjectFlags::PARENTS_ACTIVE, parent ? parent->isEnabled() : true);
        iterObjectChildren(obj.id, [&](Object* child) {
          updateStates(&obj, *child);
        });
      };

      // Resolve effective active state for the full hierarchy before deferred
      // component init so disabled parents/groups do not register physics data.
      for(auto obj : objects)
      {
        if(obj->group != 0)continue;
        updateStates(nullptr, *obj);
      }

#ifdef PLATFORM_PC
      p64_pc_trace("Scene_loadScene_pending_init");
#endif
      // Some components assume a complete object graph; on truncated blobs this can crash.
      // Fail-safe: skip deferred init if parsing stopped early.
      if (!parseStoppedEarly) {
        runPendingComponentInit();
      } else {
#ifdef PLATFORM_PC
        p64_pc_trace("Scene_loadScene_skip_pending_init_after_parse_error");
#endif
        pendingCompInit.clear();
      }

      free(objFileStart);
    }
  }
#ifdef PLATFORM_PC
  p64_pc_trace("Scene_loadScene_done");
#endif
}
