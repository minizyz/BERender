#pragma once
#include "berender/backend/render_backend.h"
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace berender {

class GLESBackend : public RenderBackend {
public:
    GLESBackend(); ~GLESBackend() override;
    Result init(void* nativeWindow) override;
    void shutdown() override;
    const BackendCapabilities& capabilities() const override { return m_caps; }
    Result beginFrame() override;
    Result endFrame() override;
    BufferHandle createBuffer(uint64_t size,BufferUsage usage,MemoryHint hint) override;
    void destroyBuffer(BufferHandle handle) override;
    void* mapBuffer(BufferHandle handle,uint64_t offset,uint64_t size) override;
    void unmapBuffer(BufferHandle handle) override;
    void updateBuffer(BufferHandle handle,uint64_t offset,const void* data,uint64_t size) override;
    TextureHandle createTexture(uint32_t w,uint32_t h,uint32_t ch,const void* data) override;
    void destroyTexture(TextureHandle handle) override;
    PipelineHandle createPipeline(RenderBucket bucket,const Material* material) override;
    void destroyPipeline(PipelineHandle handle) override;
    void bindGlobalBuffers(BufferHandle vb,BufferHandle ib) override;
    void bindCameraUniform(BufferHandle ubo) override;
    void bindMaterialStorage(BufferHandle ssbo) override;
    void bindSectionStorage(BufferHandle ssbo) override;
    void bindPipeline(PipelineHandle pipeline) override;
    void drawIndexedIndirect(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride) override;
    void drawIndexedIndirectFallback(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride) override;
    void drawIndexed(uint32_t ic,uint32_t inst,uint32_t first,int32_t voff,uint32_t finst) override;
    void setViewport(const Viewport& vp) override;
    void setScissor(const ScissorRect& rect) override;
    void bufferMemoryBarrier(BufferHandle handle,uint64_t offset,uint64_t size) override;
    BackendType type() const override { return BackendType::OpenGLES; }
private:
    struct BufferResource { uint32_t id; uint64_t size; BufferUsage usage; MemoryHint hint; bool mapped; };
    struct TextureResource { uint32_t id; uint32_t w,h; };
    struct PipelineResource { uint32_t program, vao; RenderBucket bucket; int32_t uCameraLoc,uMaterialLoc,uSectionLoc; };
    std::unordered_map<uint64_t,BufferResource> m_buffers;
    std::unordered_map<uint64_t,TextureResource> m_textures;
    std::unordered_map<uint64_t,PipelineResource> m_pipelines;
    uint64_t m_nextBufferId, m_nextTextureId, m_nextPipelineId;
    uint32_t m_currentVAO, m_currentProgram;
    BufferHandle m_currentVertexBuffer, m_currentIndexBuffer;
    void* m_eglDisplay; void* m_eglSurface; void* m_eglContext;
    Result initEGL(void* nativeWindow); void terminateEGL();
    Result compileShader(uint32_t type,const char* src,uint32_t& out);
    Result linkProgram(uint32_t vs,uint32_t fs,uint32_t& out);
    static const char* getVertexShaderSource(RenderBucket bucket);
    static const char* getFragmentShaderSource(RenderBucket bucket);
    BackendCapabilities m_caps; bool m_initialized, m_frameStarted;
};

} // namespace berender
