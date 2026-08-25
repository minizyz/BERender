#pragma once
#include "berender/core/types.h"
#include "berender/core/math.h"
#include <vector>
#include <cstdint>

namespace berender {

struct SectionRenderRequest {
    SectionCoord coord;
    RenderBucket bucket;
    uint32_t vertexOffset, indexOffset, indexCount, sectionIndex;
    bool visible;
};

struct EntityRenderRequest {
    uint32_t entityId, materialId, meshId;
    float worldMatrix[16];
    bool visible;
};

struct ParticleRenderRequest {
    float position[3], velocity[3], color[4], size;
    uint32_t textureIndex;
};

class FrameRegistry {
public:
    FrameRegistry(); ~FrameRegistry();
    void reset();
    void addSection(const SectionRenderRequest& req);
    void addEntity(const EntityRenderRequest& req);
    void addParticle(const ParticleRenderRequest& req);
    void setCamera(const CameraUniform& camera);
    void setViewport(const Viewport& vp);
    const std::vector<SectionRenderRequest>& sections(RenderBucket bucket) const;
    const std::vector<EntityRenderRequest>& entities() const { return m_entities; }
    const std::vector<ParticleRenderRequest>& particles() const { return m_particles; }
    const CameraUniform& camera() const { return m_camera; }
    const Viewport& viewport() const { return m_viewport; }
    uint32_t totalSectionCount() const;
    uint32_t visibleSectionCount() const;
private:
    std::vector<SectionRenderRequest> m_sections[static_cast<size_t>(RenderBucket::Count)];
    std::vector<EntityRenderRequest> m_entities;
    std::vector<ParticleRenderRequest> m_particles;
    CameraUniform m_camera;
    Viewport m_viewport;
};

} // namespace berender
