#pragma once
#include "berender/core/types.h"
#include "berender/render/frame_registry.h"
#include "berender/render/indirect_draw.h"
#include "berender/render/material.h"
#include <cstdint>
#include <cstddef>

namespace berender {

struct BufferHandle { uint64_t id; bool valid() const {return id!=0;} static BufferHandle invalid(){return {0};} };
struct TextureHandle { uint64_t id; bool valid() const {return id!=0;} static TextureHandle invalid(){return {0};} };
struct PipelineHandle { uint64_t id; bool valid() const {return id!=0;} static PipelineHandle invalid(){return {0};} };

class RenderBackend {
public:
    virtual ~RenderBackend()=default;
    virtual Result init(void* nativeWindow)=0;
    virtual void shutdown()=0;
    virtual const BackendCapabilities& capabilities() const=0;
    virtual Result beginFrame()=0;
    virtual Result endFrame()=0;
    virtual BufferHandle createBuffer(uint64_t size,BufferUsage usage,MemoryHint hint)=0;
    virtual void destroyBuffer(BufferHandle handle)=0;
    virtual void* mapBuffer(BufferHandle handle,uint64_t offset,uint64_t size)=0;
    virtual void unmapBuffer(BufferHandle handle)=0;
    virtual void updateBuffer(BufferHandle handle,uint64_t offset,const void* data,uint64_t size)=0;
    virtual TextureHandle createTexture(uint32_t w,uint32_t h,uint32_t channels,const void* data)=0;
    virtual void destroyTexture(TextureHandle handle)=0;
    virtual PipelineHandle createPipeline(RenderBucket bucket,const Material* material)=0;
    virtual void destroyPipeline(PipelineHandle handle)=0;
    virtual void bindGlobalBuffers(BufferHandle vb,BufferHandle ib)=0;
    virtual void bindCameraUniform(BufferHandle ubo)=0;
    virtual void bindMaterialStorage(BufferHandle ssbo)=0;
    virtual void bindSectionStorage(BufferHandle ssbo)=0;
    virtual void bindPipeline(PipelineHandle pipeline)=0;
    virtual void drawIndexedIndirect(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride)=0;
    virtual void drawIndexedIndirectFallback(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride)=0;
    virtual void drawIndexed(uint32_t indexCount,uint32_t instanceCount,uint32_t firstIndex,int32_t vertexOffset,uint32_t firstInstance)=0;
    virtual void setViewport(const Viewport& vp)=0;
    virtual void setScissor(const ScissorRect& rect)=0;
    virtual void bufferMemoryBarrier(BufferHandle handle,uint64_t offset,uint64_t size)=0;
    virtual BackendType type() const=0;
};

} // namespace berender
