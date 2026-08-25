#include "berender/render/frame_registry.h"
#include <algorithm>

namespace berender {

FrameRegistry::FrameRegistry() { m_camera={}; m_viewport={0,0,1280,720,0,1}; }
FrameRegistry::~FrameRegistry() = default;

void FrameRegistry::reset() {
    for(auto& s:m_sections) s.clear();
    m_entities.clear(); m_particles.clear();
}
void FrameRegistry::addSection(const SectionRenderRequest& req) {
    if(req.bucket>=RenderBucket::Count)return;
    m_sections[static_cast<size_t>(req.bucket)].push_back(req);
}
void FrameRegistry::addEntity(const EntityRenderRequest& req){m_entities.push_back(req);}
void FrameRegistry::addParticle(const ParticleRenderRequest& req){m_particles.push_back(req);}
void FrameRegistry::setCamera(const CameraUniform& camera){m_camera=camera;}
void FrameRegistry::setViewport(const Viewport& vp){m_viewport=vp;}
const std::vector<SectionRenderRequest>& FrameRegistry::sections(RenderBucket bucket) const {
    return m_sections[static_cast<size_t>(bucket)];
}
uint32_t FrameRegistry::totalSectionCount() const {
    uint32_t t=0; for(const auto& s:m_sections)t+=static_cast<uint32_t>(s.size()); return t;
}
uint32_t FrameRegistry::visibleSectionCount() const {
    uint32_t v=0; for(const auto& s:m_sections)for(const auto& r:s)if(r.visible)++v; return v;
}

} // namespace berender
