#pragma once
#include "berender/core/types.h"
#include "berender/render/frame_registry.h"
#include "berender/render/indirect_draw.h"
#include "berender/render/material.h"
#include "berender/render/chunk_mesh.h"
#include <memory>

namespace berender {

class RenderBackend;

struct RendererConfig {
    BackendType preferredBackend;
    uint32_t maxSections, maxEntities, maxParticles, renderDistance;
    bool enableFrustumCulling, enableAO, enablePBR;
    LogLevel logLevel;
};

struct RendererStats {
    uint32_t drawCalls, trianglesRendered, sectionsVisible, sectionsCulled, entitiesRendered;
    float frameTimeMs, gpuTimeMs;
};

class Renderer {
public:
    Renderer(); ~Renderer();
    Result init(const RendererConfig& config, void* nativeWindow);
    void shutdown();
    Result beginFrame();
    Result renderFrame();
    Result endFrame();
    FrameRegistry& frameRegistry() { return m_frameRegistry; }
    MaterialSystem& materials() { return m_materials; }
    IndirectDrawBuilder& indirectBuilder() { return m_indirectBuilder; }
    void setCamera(const CameraUniform& camera);
    void setViewport(const Viewport& vp);
    const RendererStats& stats() const { return m_stats; }
    const BackendCapabilities& capabilities() const;
    uint32_t uploadSectionMesh(const ChunkMesh& mesh);
    void invalidateSection(uint32_t sectionIndex);
private:
    void performFrustumCulling();
    void buildIndirectBuffers();
    void submitRenderCommands();
    void updateStats();
    RendererConfig m_config;
    FrameRegistry m_frameRegistry;
    IndirectDrawBuilder m_indirectBuilder;
    MaterialSystem m_materials;
    RendererStats m_stats;
    std::unique_ptr<RenderBackend> m_backend;
    BackendType m_activeBackend;
    bool m_initialized, m_frameInProgress;
    struct SectionGPUResource { uint32_t vertexOffset,indexOffset,vertexCount,indexCount; bool valid; };
    std::vector<SectionGPUResource> m_sectionResources;
    uint32_t m_nextSectionSlot;
};

} // namespace berender
