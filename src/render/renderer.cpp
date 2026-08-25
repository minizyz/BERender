#include "berender/render/renderer.h"
#include "berender/backend/render_backend.h"
#include "berender/backend/vulkan_backend.h"
#include "berender/backend/gles_backend.h"
#include "berender/core/logger.h"
#include "berender/core/math.h"
#include <cstring>
#include <algorithm>
#include <chrono>

namespace berender {

Renderer::Renderer():m_activeBackend(BackendType::Vulkan),m_initialized(false),m_frameInProgress(false),m_nextSectionSlot(0){
    memset(&m_config,0,sizeof(m_config));memset(&m_stats,0,sizeof(m_stats));
    m_config.preferredBackend=BackendType::Vulkan;m_config.maxSections=4096;m_config.maxEntities=1024;
    m_config.maxParticles=8192;m_config.renderDistance=12;m_config.enableFrustumCulling=true;
    m_config.enableAO=true;m_config.enablePBR=false;m_config.logLevel=LogLevel::Info;
}
Renderer::~Renderer(){shutdown();}

Result Renderer::init(const RendererConfig& config,void* nativeWindow){
    if(m_initialized){BERENDER_WARN("Renderer already init");shutdown();}
    m_config=config;Logger::instance().setLevel(config.logLevel);
    BERENDER_INFO("BERenderer init (backend: %s)",config.preferredBackend==BackendType::Vulkan?"Vulkan":"GLES");
    if(config.preferredBackend==BackendType::Vulkan){
#ifdef BERENDER_HAS_VULKAN
        auto vk=std::make_unique<VulkanBackend>();
        if(vk->init(nativeWindow)==Result::Success){m_backend=std::move(vk);m_activeBackend=BackendType::Vulkan;BERENDER_INFO("Vulkan backend ok");}
        else BERENDER_WARN("Vulkan init failed, fallback GLES");
#else
        BERENDER_WARN("Vulkan not compiled, fallback GLES");
#endif
    }
    if(!m_backend){
#ifdef BERENDER_HAS_GLES
        auto gles=std::make_unique<GLESBackend>();
        if(gles->init(nativeWindow)!=Result::Success){BERENDER_ERROR("GLES init failed");return Result::ErrorInitializationFailed;}
        m_backend=std::move(gles);m_activeBackend=BackendType::OpenGLES;BERENDER_INFO("GLES backend ok");
#else
        BERENDER_ERROR("No backend available");return Result::ErrorUnsupported;
#endif
    }
    const auto& caps=m_backend->capabilities();
    if(!caps.multiDrawIndirect){BERENDER_WARN("No MDI support, software fallback");m_indirectBuilder.setSoftwareFallback(true);}
    m_indirectBuilder.init(caps.minIndirectBufferOffsetAlignment);
    m_sectionResources.resize(m_config.maxSections);for(auto& r:m_sectionResources)r.valid=false;
    m_initialized=true;
    BERENDER_INFO("BERenderer ready (device: %s)",caps.deviceName);
    return Result::Success;
}
void Renderer::shutdown(){
    if(!m_initialized)return;BERENDER_INFO("Renderer shutdown");
    if(m_backend){m_backend->shutdown();m_backend.reset();}
    m_frameRegistry.reset();m_indirectBuilder.reset();m_materials.clear();m_sectionResources.clear();
    m_initialized=false;m_frameInProgress=false;
}
Result Renderer::beginFrame(){
    if(!m_initialized)return Result::ErrorInitializationFailed;
    if(m_frameInProgress){BERENDER_WARN("frame already in progress");return Result::ErrorUnknown;}
    Result r=m_backend->beginFrame();if(r!=Result::Success)return r;
    m_frameRegistry.reset();m_frameInProgress=true;return Result::Success;
}
Result Renderer::renderFrame(){
    if(!m_initialized||!m_frameInProgress)return Result::ErrorUnknown;
    auto t0=std::chrono::high_resolution_clock::now();
    if(m_config.enableFrustumCulling)performFrustumCulling();
    buildIndirectBuffers();submitRenderCommands();
    auto t1=std::chrono::high_resolution_clock::now();
    m_stats.frameTimeMs=std::chrono::duration<float,std::milli>(t1-t0).count();
    return Result::Success;
}
Result Renderer::endFrame(){
    if(!m_initialized||!m_frameInProgress)return Result::ErrorUnknown;
    Result r=m_backend->endFrame();m_frameInProgress=false;return r;
}
void Renderer::setCamera(const CameraUniform& c){m_frameRegistry.setCamera(c);}
void Renderer::setViewport(const Viewport& vp){m_frameRegistry.setViewport(vp);if(m_backend)m_backend->setViewport(vp);}
const BackendCapabilities& Renderer::capabilities() const {static BackendCapabilities e{};return m_backend?m_backend->capabilities():e;}
uint32_t Renderer::uploadSectionMesh(const ChunkMesh& mesh){
    if(!m_initialized)return UINT32_MAX;
    uint32_t slot=UINT32_MAX;for(uint32_t i=0;i<m_sectionResources.size();i++)if(!m_sectionResources[i].valid){slot=i;break;}
    if(slot==UINT32_MAX){slot=(uint32_t)m_sectionResources.size();m_sectionResources.push_back({});}
    auto& r=m_sectionResources[slot];r.vertexOffset=0;r.indexOffset=0;
    r.vertexCount=mesh.vertexCount(RenderBucket::Opaque)+mesh.vertexCount(RenderBucket::Cutout)+mesh.vertexCount(RenderBucket::Transparent);
    r.indexCount=mesh.indexCount(RenderBucket::Opaque)+mesh.indexCount(RenderBucket::Cutout)+mesh.indexCount(RenderBucket::Transparent);
    r.valid=true;return slot;
}
void Renderer::invalidateSection(uint32_t s){if(s<m_sectionResources.size())m_sectionResources[s].valid=false;}
void Renderer::performFrustumCulling(){
    uint32_t vis=0,cul=0;
    for(int b=0;b<(int)RenderBucket::Count;b++)for(const auto& r:m_frameRegistry.sections((RenderBucket)b)){if(r.visible)++vis;else++cul;}
    m_stats.sectionsVisible=vis;m_stats.sectionsCulled=cul;
    BERENDER_DEBUG("Culling: %u visible %u culled",vis,cul);
}
void Renderer::buildIndirectBuffers(){m_indirectBuilder.build(m_frameRegistry);m_stats.drawCalls=m_indirectBuilder.totalDrawCount();}
void Renderer::submitRenderCommands(){
    if(!m_backend)return;uint32_t dc=0;
    for(int b=0;b<(int)RenderBucket::Count;b++){
        uint32_t cnt=m_indirectBuilder.drawCount((RenderBucket)b);if(cnt==0)continue;
        dc+=m_indirectBuilder.softwareFallback()?cnt:1;
    }
    dc+=(uint32_t)m_frameRegistry.entities().size();
    if(!m_frameRegistry.particles().empty())dc+=1;
    m_stats.drawCalls=dc;m_stats.entitiesRendered=(uint32_t)m_frameRegistry.entities().size();
}
void Renderer::updateStats(){}
} // namespace berender
