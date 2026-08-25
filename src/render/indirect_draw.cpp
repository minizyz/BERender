#include "berender/render/indirect_draw.h"
#include "berender/core/logger.h"
#include "berender/core/math.h"
#include <cstring>
#include <algorithm>

namespace berender {

IndirectDrawBuilder::IndirectDrawBuilder():m_indirectAlignment(4),m_softwareFallback(false){
    for(auto& c:m_drawCounts)c=0; for(auto& o:m_byteOffsets)o=0;
}
IndirectDrawBuilder::~IndirectDrawBuilder()=default;

void IndirectDrawBuilder::init(uint32_t indirectAlignment){
    m_indirectAlignment=std::max(4u,indirectAlignment);
    BERENDER_INFO("IndirectDrawBuilder init alignment=%u",m_indirectAlignment);
}
void IndirectDrawBuilder::reset(){
    for(auto& c:m_commands)c.clear(); for(auto& c:m_drawCounts)c=0; for(auto& o:m_byteOffsets)o=0;
}

void IndirectDrawBuilder::build(const FrameRegistry& registry){
    reset();
    uint32_t currentOffset=0;
    for(int b=0;b<static_cast<int>(RenderBucket::Count);++b){
        RenderBucket bucket=static_cast<RenderBucket>(b);
        const auto& sections=registry.sections(bucket);
        m_byteOffsets[b]=currentOffset;
        if(sections.empty()){m_drawCounts[b]=0;continue;}
        auto& cmds=m_commands[b]; cmds.reserve(sections.size());
        uint32_t visible=0;
        for(const auto& req:sections){
            DrawIndexedIndirectCommand cmd;
            cmd.indexCount=req.indexCount;
            cmd.instanceCount=req.visible?1u:0u;
            cmd.firstIndex=req.indexOffset;
            cmd.vertexOffset=static_cast<int32_t>(req.vertexOffset);
            cmd.firstInstance=req.sectionIndex;
            cmds.push_back(cmd);
            if(req.visible)++visible;
        }
        m_drawCounts[b]=static_cast<uint32_t>(cmds.size());
        uint32_t bucketSize=m_drawCounts[b]*sizeof(DrawIndexedIndirectCommand);
        currentOffset+=bucketSize;
        currentOffset=math::alignUp(currentOffset,m_indirectAlignment);
        BERENDER_DEBUG("Bucket %d: %d sections (%d visible)",b,(int)sections.size(),visible);
    }
    if(m_softwareFallback) BERENDER_WARN("Software fallback: MDI as individual draws");
}

const std::vector<DrawIndexedIndirectCommand>& IndirectDrawBuilder::commands(RenderBucket bucket) const {
    return m_commands[static_cast<size_t>(bucket)];
}
uint32_t IndirectDrawBuilder::drawCount(RenderBucket bucket) const { return m_drawCounts[static_cast<size_t>(bucket)]; }
uint32_t IndirectDrawBuilder::totalDrawCount() const { uint32_t t=0; for(auto c:m_drawCounts)t+=c; return t; }
uint32_t IndirectDrawBuilder::bucketByteOffset(RenderBucket bucket) const { return m_byteOffsets[static_cast<size_t>(bucket)]; }
uint32_t IndirectDrawBuilder::totalBufferSize() const {
    uint32_t maxOff=0;
    for(int b=0;b<static_cast<int>(RenderBucket::Count);++b){
        uint32_t end=m_byteOffsets[b]+m_drawCounts[b]*sizeof(DrawIndexedIndirectCommand);
        maxOff=std::max(maxOff,end);
    }
    return math::alignUp(maxOff,m_indirectAlignment);
}
void IndirectDrawBuilder::serialize(uint8_t* dst,uint32_t dstSize) const {
    if(!dst)return;
    uint32_t req=totalBufferSize();
    if(dstSize<req){BERENDER_ERROR("serialize: buffer too small (%u<%u)",dstSize,req);return;}
    memset(dst,0,dstSize);
    for(int b=0;b<static_cast<int>(RenderBucket::Count);++b){
        if(m_drawCounts[b]==0)continue;
        uint32_t sz=m_drawCounts[b]*sizeof(DrawIndexedIndirectCommand);
        memcpy(dst+m_byteOffsets[b],m_commands[b].data(),sz);
    }
}

} // namespace berender
