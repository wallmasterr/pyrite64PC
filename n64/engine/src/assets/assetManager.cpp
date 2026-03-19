/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#ifdef PLATFORM_PC
#include <pc_compat.h>
#include <cstdlib>
#endif
#include "assets/assetManager.h"

#include <libdragon.h>
#ifdef PLATFORM_PC
extern "C" wav64_t* wav64_load(const char* path, int* sz);
#endif

#include "assets/assetTypes.h"
#include "lib/logger.h"
#include "scene/components/model.h"

namespace P64::NodeGraph
{
  void* load(const char* path);
}

namespace
{
#ifdef PLATFORM_PC
  /* PC: 64-bit pointers; table blob is 4+4 bytes per entry (path offset, packed type/flags). */
  struct AssetEntry
  {
    constexpr static uint8_t FLAG_KEEP_LOADED = 1 << 0;
    const char* path{};
    uint32_t packed_{};  /* type in high nibble, flags etc. from table file */
    void* data_{};

    uint32_t getFlags() const { return (packed_ >> 24) & 0x0F; }
    uint32_t getType() const { return (packed_ >> 28) & 0x0F; }
    void* getPointer() const { return data_; }
    void setPointer(void* ptr) { data_ = ptr; }
  };
  struct AssetTable { uint32_t count{}; AssetEntry entries[1]; };
#else
  struct AssetEntry
  {
    constexpr static uint8_t FLAG_KEEP_LOADED = 1 << 0;
    const char* path{};
    void* data{};
    uint32_t getFlags() const { return ((uint32_t)data >> (32-8)) & 0x0F; }
    uint32_t getType() const { return (uint32_t)data >> (32-4); }
    void* getPointer() const { return (void*)((uint32_t)data & 0x00FF'FFFF); }
    void setPointer(void* ptr) {
      uint32_t ptrMasked = (uint32_t)ptr & 0x00FF'FFFF;
      uint32_t typeMasked = (uint32_t)data & 0xFF00'0000;
      data = (void*)(ptrMasked | typeMasked);
    }
  };
  struct AssetTable { uint32_t count{}; AssetEntry entries[]; };
#endif

  typedef void* (*LoadFunc)(const char* path);
  typedef void (*FreeFunc)(void* ref);

  struct AssetHandler
  {
    LoadFunc fnLoad{};
    FreeFunc fnFree{};
  };

  namespace AssetType = P64::Assets::Type;

  wav64_t* wav64Load(const char* path) {
    return wav64_load(path, nullptr);
  }
  void* assetLoad(const char* path) {
    return asset_load(path, nullptr);
  }

  AssetHandler assetHandler[] = {
    [AssetType::UNKNOWN]     = {(LoadFunc)assetLoad,      (FreeFunc)free          },
    [AssetType::IMAGE]       = {(LoadFunc)sprite_load,    (FreeFunc)sprite_free   },
    [AssetType::AUDIO]       = {(LoadFunc)wav64Load,      (FreeFunc)wav64_close   },
    [AssetType::FONT]        = {(LoadFunc)rdpq_font_load, (FreeFunc)rdpq_font_free},
    [AssetType::MODEL_3D]    = {(LoadFunc)t3d_model_load, (FreeFunc)t3d_model_free},
    [AssetType::CODE_OBJ]    = {nullptr,                  nullptr                 },
    [AssetType::CODE_GLOBAL] = {nullptr,                  nullptr                 },
    [AssetType::PREFAB]      = {(LoadFunc)assetLoad,      (FreeFunc)free          },
    [AssetType::NODE_GRAPH]  = {P64::NodeGraph::load,     (FreeFunc)free          },
  };

  constinit AssetTable* assetTable{nullptr};
  constinit bool isInit{false};
#ifdef PLATFORM_PC
  constinit void* s_assetBlob{nullptr};
#endif
}

void P64::AssetManager::init() {
  if (isInit)return;
  isInit = true;

#ifdef PLATFORM_PC
  s_assetBlob = asset_load("rom:/p64/a", nullptr);
  const uint32_t count = *(uint32_t*)s_assetBlob;
  assetTable = (AssetTable*)malloc(sizeof(uint32_t) + count * sizeof(AssetEntry));
  assetTable->count = count;
  for (uint32_t i = 0; i < count; ++i) {
    const char* base = (const char*)s_assetBlob;
    uint32_t pathOffset = *(const uint32_t*)(base + 4 + i * 8);
    uint32_t packed = *(const uint32_t*)(base + 4 + i * 8 + 4);
    assetTable->entries[i].path = base + pathOffset;
    assetTable->entries[i].packed_ = packed;
    assetTable->entries[i].data_ = nullptr;
  }
#else
  assetTable = (AssetTable*)asset_load("rom:/p64/a", nullptr);
  for (uint32_t i = 0; i < assetTable->count; ++i) {
    auto &entry = assetTable->entries[i];
    uint32_t offset = (uint32_t)entry.path;
    entry.path = (char*)assetTable + offset;
  }
#endif
}

void P64::AssetManager::freeAll() {
  for (uint32_t i = 0; i < assetTable->count; ++i)
  {
    auto &entry = assetTable->entries[i];
    if(entry.getPointer())
    {
      auto flags = entry.getFlags();
      if(flags & AssetEntry::FLAG_KEEP_LOADED)continue;

      auto type = entry.getType();
      const auto &loader = assetHandler[type];
      void *data = entry.getPointer();
#ifndef PLATFORM_PC
      data = (void*)((uint32_t)data | 0x8000'0000);
#endif
      loader.fnFree(data);
      entry.setPointer(nullptr);
    }
  }
}

void* P64::AssetManager::getByIndex(uint32_t idx) {
  if (idx >= assetTable->count) {
    return nullptr;
  }

  auto &entry = assetTable->entries[idx];

  void* res = entry.getPointer();
  if (!res) {
    auto type = entry.getType();
    const auto &loader = assetHandler[type];
    assertf(loader.fnLoad != nullptr, "No asset loader for type: %lu, %lu:%s", type, idx, entry.path);
    res = loader.fnLoad(entry.path);
    entry.setPointer(res);
    //debugf("Load Asset: %s | %lu\n", entry.path, type);
  } else {
#ifndef PLATFORM_PC
    res = (void*)((uint32_t)res | 0x8000'0000);
#endif
  }

  return res;
}

/*void* P64::AssetManager::getByFilePath(const std::string &path)
{
  for (uint32_t i = 0; i < assetTable->count; ++i) {
    auto &entry = assetTable->entries[i];
    if (path == entry.path) {
      return getByIndex(i);
    }
  }
  return nullptr;
}*/
