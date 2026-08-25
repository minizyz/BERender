// BERender - Example Application
#include "berender/render/renderer.h"
#include "berender/render/chunk_mesh.h"
#include "berender/render/material.h"
#include "berender/core/logger.h"
#include "berender/core/math.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
using namespace berender;

static void generateFlatWorld(BlockData* blocks,int sx,int sz,int groundY){
    for(int z=0;z<sz;z++)for(int x=0;x<sx;x++)for(int y=0;y<16;y++){
        BlockData& b=blocks[x+y*16+z*256];
        if(y<groundY){b.id=1;b.light=0xF0;b.ao=3;b.transparent=false;b.cutout=false;}
        else if(y==groundY){b.id=2;b.light=0xF0;b.ao=3;b.transparent=false;b.cutout=false;}
        else{b.id=0;b.light=0;b.ao=0;b.transparent=true;b.cutout=false;}
    }
}

static void registerMaterials(MaterialSystem& m){
    Material stone={};stone.name="stone";stone.defaultBucket=RenderBucket::Opaque;stone.flags=MaterialFlag::None;
    stone.gpuData.baseColor[0]=0.5f;stone.gpuData.baseColor[1]=0.5f;stone.gpuData.baseColor[2]=0.5f;stone.gpuData.baseColor[3]=1;stone.gpuData.roughness=0.8f;m.registerMaterial(stone);
    Material grass={};grass.name="grass";grass.defaultBucket=RenderBucket::Opaque;
    grass.gpuData.baseColor[0]=0.2f;grass.gpuData.baseColor[1]=0.6f;grass.gpuData.baseColor[2]=0.2f;grass.gpuData.baseColor[3]=1;grass.gpuData.roughness=0.9f;m.registerMaterial(grass);
    Material leaves={};leaves.name="leaves";leaves.defaultBucket=RenderBucket::Cutout;leaves.flags=MaterialFlag::Cutout;
    leaves.gpuData.baseColor[0]=0.15f;leaves.gpuData.baseColor[1]=0.45f;leaves.gpuData.baseColor[2]=0.1f;leaves.gpuData.baseColor[3]=0.8f;leaves.gpuData.roughness=1;m.registerMaterial(leaves);
    Material water={};water.name="water";water.defaultBucket=RenderBucket::Transparent;water.flags=MaterialFlag::Transparent;
    water.gpuData.baseColor[0]=0.2f;water.gpuData.baseColor[1]=0.4f;water.gpuData.baseColor[2]=0.8f;water.gpuData.baseColor[3]=0.6f;water.gpuData.roughness=0.1f;m.registerMaterial(water);
}

int main(int argc,char* argv[]){
    (void)argc;(void)argv;
    BERENDER_INFO("=== BERender Example ===");
    RendererConfig cfg={};cfg.preferredBackend=BackendType::Vulkan;cfg.maxSections=1024;
    cfg.maxEntities=256;cfg.maxParticles=4096;cfg.renderDistance=8;cfg.enableFrustumCulling=true;
    cfg.enableAO=true;cfg.enablePBR=false;cfg.logLevel=LogLevel::Debug;
    Renderer renderer;
    void* nativeWindow=nullptr; // pass ANativeWindow*/HWND/etc in real app
    Result r=renderer.init(cfg,nativeWindow);
    if(r!=Result::Success){BERENDER_ERROR("Renderer init failed (%d)",(int)r);return 1;}
    const auto& caps=renderer.capabilities();
    BERENDER_INFO("Backend: %s Device: %s MDI: %s",caps.type==BackendType::Vulkan?"Vulkan":"GLES",caps.deviceName,caps.multiDrawIndirect?"yes":"no(fallback)");
    registerMaterials(renderer.materials());
    constexpr int WX=4,WZ=4,GY=4;
    std::vector<ChunkMesh> meshes;meshes.reserve(WX*WZ);
    for(int sz=0;sz<WZ;sz++)for(int sx=0;sx<WX;sx++){
        BlockData blocks[16*16*16];generateFlatWorld(blocks,16,16,GY);
        SectionCoord coord={sx,0,sz};ChunkMesh mesh;mesh.build(blocks,coord);
        if(!mesh.isEmpty())meshes.push_back(std::move(mesh));
    }
    BERENDER_INFO("Generated %zu sections",meshes.size());
    constexpr int FRAMES=10;
    for(int frame=0;frame<FRAMES;frame++){
        r=renderer.beginFrame();if(r!=Result::Success)break;
        float angle=(float)frame*0.1f;float radius=40;
        math::Vec3 eye={cosf(angle)*radius,20,sinf(angle)*radius};
        math::Vec3 center={16,8,16},up={0,1,0};
        math::Mat4 view=math::Mat4::lookAt(eye,center,up);
        math::Mat4 proj=math::Mat4::perspective(60*3.14159f/180,16.0f/9,0.1f,1000);
        math::Mat4 vp=proj*view;
        CameraUniform cam={};memcpy(cam.viewProj,vp.m,sizeof(cam.viewProj));
        cam.position[0]=eye.x;cam.position[1]=eye.y;cam.position[2]=eye.z;
        cam.nearPlane=0.1f;cam.farPlane=1000;cam.time=(float)frame/60;
        renderer.setCamera(cam);
        Viewport vp2={0,0,1280,720,0,1};renderer.setViewport(vp2);
        FrameRegistry& reg=renderer.frameRegistry();
        for(size_t i=0;i<meshes.size();i++){
            const ChunkMesh& mesh=meshes[i];
            for(int b=0;b<(int)RenderBucket::Count;b++){
                RenderBucket bucket=(RenderBucket)b;if(mesh.indexCount(bucket)==0)continue;
                SectionRenderRequest req={};req.coord=mesh.coord();req.bucket=bucket;
                req.vertexOffset=0;req.indexOffset=0;req.indexCount=mesh.indexCount(bucket);
                req.sectionIndex=(uint32_t)i;req.visible=true;reg.addSection(req);
            }
        }
        r=renderer.renderFrame();if(r!=Result::Success)break;
        r=renderer.endFrame();if(r!=Result::Success)break;
        const auto& s=renderer.stats();
        BERENDER_INFO("Frame %d: drawCalls=%u sections=%u/%u entities=%u time=%.2fms",frame,s.drawCalls,s.sectionsVisible,s.sectionsVisible+s.sectionsCulled,s.entitiesRendered,s.frameTimeMs);
    }
    renderer.shutdown();
    BERENDER_INFO("=== BERender Example finished ===");
    return 0;
}
