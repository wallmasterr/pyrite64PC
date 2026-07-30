/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assets/assetManager.h"
#include "scene/object.h"
#include "scene/components/animModel.h"
#include "assets/assetManager.h"
#include <t3d/t3dmodel.h>

#include "../../renderer/bigtex/bigtex.h"
#include "renderer/material.h"
#include "scene/scene.h"
#include "scene/sceneManager.h"

#ifdef PLATFORM_DC
extern "C" void p64_dc_soft_skeleton_bind(const T3DSkeleton* skel);
#endif

namespace
{
  struct InitData
  {
    uint16_t assetIdx;
    uint8_t layer;
    uint8_t flags;
    P64::Renderer::MaterialInstance material;
  };

  void recordAnimModelMeshes(T3DModel* model)
  {
    if (!model || model->userBlock)
      return;
    rspq_block_begin();
#ifndef PLATFORM_DC
    P64::Renderer::MaterialState state{};
#endif
    auto it = t3d_model_iter_create(model, T3D_CHUNK_TYPE_OBJECT);
    while (t3d_model_iter_next(&it)) {
#ifndef PLATFORM_DC
      auto* mat = (P64::Renderer::Material*)it.object->material;
      assert(mat);
      mat->begin(state);
      auto boneSeg = (const T3DMat4FP*)t3d_segment_placeholder(T3D_SEGMENT_SKELETON);
      t3d_model_draw_object(it.object, boneSeg);
      mat->end(state);
#else
      /* Soft path: draw bind-pose mesh; skinning not implemented yet. */
      t3d_model_draw_object(it.object, nullptr);
#endif
    }
    model->userBlock = rspq_block_end();
  }
}

namespace P64::Comp
{
  void AnimModel::setMainAnim(int16_t idx) {
    animIdxMain = idx;
  }

  void AnimModel::setBlendAnim(int16_t idx) {
    if (animIdxBlend != idx) {
      t3d_anim_attach(&anims[idx], &skelAnim[idx]);
    }
    animIdxBlend = idx;
  }


  uint32_t AnimModel::getAllocSize(uint16_t* initData)
  {
    return sizeof(AnimModel) - sizeof(Renderer::MaterialInstance) + ((InitData*)initData)->material.getSize();
  }

  void AnimModel::initDelete([[maybe_unused]] Object& obj, AnimModel* data, void* initData_)
  {
    auto *initData = (InitData*)initData_;
    if (initData == nullptr) {
#ifdef PLATFORM_DC
      /* Soft path may never allocate skeleton/anim tables. */
      if (data->anims && data->skelAnim && data->model) {
        auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
        uint32_t i = 0;
        while (t3d_model_iter_next(&it)) {
          t3d_anim_destroy(&data->anims[i]);
          t3d_skeleton_destroy(&data->skelAnim[i]);
          ++i;
        }
      }
      if (data->skelMain.bones || data->skelMain.boneMatricesFP)
        t3d_skeleton_destroy(&data->skelMain);
      free(data->skelAnim);
      free(data->anims);
#else
      auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
      uint32_t i=0;
      while(t3d_model_iter_next(&it)) {
        t3d_anim_destroy(&data->anims[i]);
        t3d_skeleton_destroy(&data->skelAnim[i]);
        ++i;
      }
      t3d_skeleton_destroy(&data->skelMain);
      free(data->skelAnim);
      free(data->anims);
#endif

      data->~AnimModel();
      return;
    }

    new(data) AnimModel();

    data->model = (T3DModel*)AssetManager::getByIndex(initData->assetIdx);
#ifdef PLATFORM_DC
    if (!data->model) {
      printf("[p64] AnimModel init: asset %u failed to load\n", (unsigned)initData->assetIdx);
      return;
    }
#else
    assert(data->model != nullptr);
#endif
    data->layerIdx = initData->layer;
    data->flags = initData->flags;

    // struct has move/copy removed for safety and to avoid accidental copies.
    // but we still need to memcpy here, the warning is wrong anyways as it's still a trivial type
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
      memcpy(&data->material, &initData->material, initData->material.getSize());
    #pragma GCC diagnostic pop

    data->material.init();

#ifdef PLATFORM_DC
    /* Soft Tiny3D: bind-pose skeleton + RSP-style vertex cache in dc_soft3d. */
    data->skelMain = t3d_skeleton_create_buffered(data->model, 1);
    if (!data->skelMain.skeletonRef) {
      printf("[p64] AnimModel: no skeleton in model (asset %u)\n", (unsigned)initData->assetIdx);
    }
    recordAnimModelMeshes(data->model);
    return;
#else
    /*bool isBigTex = SceneManager::getCurrent().getConf().pipeline == SceneConf::Pipeline::BIG_TEX_256;

    if(isBigTex) {
      Renderer::BigTex::patchT3DM(*data->model);
      return;
    }*/

    // @TODO: all of this is rather hacky and unoptimized
    // @TODO: refactor and rethink animations here

    auto animCount = t3d_model_get_animation_count(data->model);

    // one main skeleton for drawing, others for potential blending
    data->skelMain = t3d_skeleton_create_buffered(data->model, 3); // @TODO: take from scene settings once added
    data->skelAnim = static_cast<T3DSkeleton*>(malloc(sizeof(T3DSkeleton) * animCount));
    data->anims = static_cast<T3DAnim*>(malloc(sizeof(T3DAnim) * animCount));

    t3d_skeleton_update(&data->skelMain);

    auto it = t3d_model_iter_create(data->model, T3D_CHUNK_TYPE_ANIM);
    uint32_t i = 0;

    // @TODO: handles names vs indices in the public API

    //debugf("AnimModel: count=%lu\n", animCount);
    while(t3d_model_iter_next(&it)) {
      data->skelAnim[i] = t3d_skeleton_clone(&data->skelMain, false);
      data->anims[i] = t3d_anim_create(data->model, it.anim->name); // @TOOD: add  create by-index to t3d API
      t3d_anim_attach(&data->anims[i], &data->skelMain); // by default assuming anything is attached to the main skeleton
      //debugf(" - %s: %lu\n", it.anim->name, i);
      ++i;
    }

    recordAnimModelMeshes(data->model);
#endif
  }

  void AnimModel::update(Object&obj, AnimModel* data, float deltaTime) {
#ifdef PLATFORM_DC
    (void)obj;
    (void)data;
    (void)deltaTime;
    return;
#else
    if (data->animIdxMain >= 0) {
      t3d_anim_update(&data->anims[data->animIdxMain], deltaTime);
    }
    if (data->animIdxBlend >= 0) {
      t3d_anim_update(&data->anims[data->animIdxBlend], deltaTime);

      t3d_skeleton_blend(
        &data->skelMain,
        &data->skelMain,
        &data->skelAnim[data->animIdxBlend],
        data->blendFactor
      );
    }

    t3d_skeleton_update(&data->skelMain);
#endif
  }

  void AnimModel::draw(Object &obj, AnimModel* data, float deltaTime)
  {
    (void)deltaTime;
    if (!data->model || !data->model->userBlock)
      return;

    auto mat = data->matFP.getNext();
    t3d_mat4fp_from_srt(mat, obj.scale, obj.rot, obj.pos);

    if(data->layerIdx)DrawLayer::use3D(data->layerIdx);

    data->material.begin(obj);

#ifdef PLATFORM_DC
    p64_dc_soft_skeleton_bind(&data->skelMain);
#else
    t3d_skeleton_use(&data->skelMain);
#endif
    t3d_matrix_set(mat, true);
    rspq_block_run(data->model->userBlock);

    data->material.end();
    if(data->layerIdx)DrawLayer::useDefault();
  }
}
