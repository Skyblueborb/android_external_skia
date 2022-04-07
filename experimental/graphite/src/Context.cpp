/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/include/Context.h"

#include "experimental/graphite/include/BackendTexture.h"
#include "experimental/graphite/include/Recorder.h"
#include "experimental/graphite/include/Recording.h"
#include "experimental/graphite/include/TextureInfo.h"
#include "experimental/graphite/src/Caps.h"
#include "experimental/graphite/src/CommandBuffer.h"
#include "experimental/graphite/src/GlobalCache.h"
#include "experimental/graphite/src/Gpu.h"
#include "experimental/graphite/src/GraphicsPipelineDesc.h"
#include "experimental/graphite/src/Renderer.h"
#include "experimental/graphite/src/ResourceProvider.h"
#include "include/core/SkPathTypes.h"
#include "src/core/SkKeyContext.h"
#include "src/core/SkKeyHelpers.h"
#include "src/core/SkShaderCodeDictionary.h"
#include "src/gpu/RefCntedCallback.h"

#ifdef SK_METAL
#include "experimental/graphite/src/mtl/MtlTrampoline.h"
#endif

namespace skgpu {

Context::Context(sk_sp<Gpu> gpu, BackendApi backend)
        : fGpu(std::move(gpu))
        , fGlobalCache(sk_make_sp<GlobalCache>())
        , fBackend(backend) {
}
Context::~Context() {}

#ifdef SK_METAL
std::unique_ptr<Context> Context::MakeMetal(const graphite::MtlBackendContext& backendContext) {
    sk_sp<Gpu> gpu = graphite::MtlTrampoline::MakeGpu(backendContext);
    if (!gpu) {
        return nullptr;
    }

    return std::unique_ptr<Context>(new Context(std::move(gpu), BackendApi::kMetal));
}
#endif

std::unique_ptr<Recorder> Context::makeRecorder() {
    return std::unique_ptr<Recorder>(new Recorder(fGpu, fGlobalCache));
}

void Context::insertRecording(const InsertRecordingInfo& info) {
    sk_sp<RefCntedCallback> callback;
    if (info.fFinishedProc) {
        callback = RefCntedCallback::Make(info.fFinishedProc, info.fFinishedContext);
    }

    SkASSERT(info.fRecording);
    if (!info.fRecording) {
        if (callback) {
            callback->setFailureResult();
        }
        return;
    }

    SkASSERT(!fCurrentCommandBuffer);
    // For now we only allow one CommandBuffer. So we just ref it off the InsertRecordingInfo and
    // hold onto it until we submit.
    fCurrentCommandBuffer = info.fRecording->fCommandBuffer;
    if (callback) {
        fCurrentCommandBuffer->addFinishedProc(std::move(callback));
    }
}

void Context::submit(SyncToCpu syncToCpu) {
    SkASSERT(fCurrentCommandBuffer);

    fGpu->submit(std::move(fCurrentCommandBuffer));

    fGpu->checkForFinishedWork(syncToCpu);
}

void Context::checkAsyncWorkCompletion() {
    fGpu->checkForFinishedWork(SyncToCpu::kNo);
}

void Context::preCompile(const PaintCombo& paintCombo) {
    static const Renderer* kRenderers[] = {
            &Renderer::StencilTessellatedCurvesAndTris(SkPathFillType::kWinding),
            &Renderer::StencilTessellatedCurvesAndTris(SkPathFillType::kEvenOdd),
            &Renderer::StencilTessellatedCurvesAndTris(SkPathFillType::kInverseWinding),
            &Renderer::StencilTessellatedCurvesAndTris(SkPathFillType::kInverseEvenOdd),
            &Renderer::StencilTessellatedWedges(SkPathFillType::kWinding),
            &Renderer::StencilTessellatedWedges(SkPathFillType::kEvenOdd),
            &Renderer::StencilTessellatedWedges(SkPathFillType::kInverseWinding),
            &Renderer::StencilTessellatedWedges(SkPathFillType::kInverseEvenOdd)
    };

    SkShaderCodeDictionary* dict = fGlobalCache->shaderCodeDictionary();
    SkKeyContext keyContext(dict);

    SkPaintParamsKeyBuilder builder(dict, SkBackend::kGraphite);

    for (auto bm: paintCombo.fBlendModes) {
        for (auto& shaderCombo: paintCombo.fShaders) {
            for (auto shaderType: shaderCombo.fTypes) {
                for (auto tm: shaderCombo.fTileModes) {
                    auto uniqueID = CreateKey(keyContext, &builder, shaderType, tm, bm);

                    GraphicsPipelineDesc desc;

                    for (const Renderer* r : kRenderers) {
                        for (auto&& s : r->steps()) {
                            if (s->performsShading()) {
                                desc.setProgram(s, uniqueID);
                            }
                            // TODO: Combine with renderpass description set to generate full
                            // GraphicsPipeline and MSL program. Cache that compiled pipeline on
                            // the resource provider in a map from desc -> pipeline so that any
                            // later desc created from equivalent RenderStep + Combination get it.
                        }
                    }
                }
            }
        }
    }
    // TODO: Iterate over the renderers and make descriptions for the steps that don't perform
    // shading, and just use ShaderType::kNone.
}

BackendTexture Context::createBackendTexture(SkISize dimensions, const TextureInfo& info) {
    if (!info.isValid() || info.backend() != this->backend()) {
        return {};
    }
    return fGpu->createBackendTexture(dimensions, info);
}

void Context::deleteBackendTexture(BackendTexture& texture) {
    if (!texture.isValid() || texture.backend() != this->backend()) {
        return;
    }
    fGpu->deleteBackendTexture(texture);
}

} // namespace skgpu
