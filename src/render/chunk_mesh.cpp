#include "berender/render/chunk_mesh.h"
#include "berender/core/logger.h"
#include <cstring>
#include <algorithm>

namespace berender {
namespace {
constexpr int8_t FACE_VERTS[6][4][3] = {
    {{1,0,0},{1,1,0},{1,1,1},{1,0,1}}, {{0,0,1},{0,1,1},{0,1,0},{0,0,0}},
    {{0,1,1},{1,1,1},{1,1,0},{0,1,0}}, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
    {{1,0,1},{1,1,1},{0,1,1},{0,0,1}}, {{0,0,0},{0,1,0},{1,1,0},{1,0,0}}
};
constexpr int8_t FACE_NORMALS[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
constexpr uint8_t FACE_UVS[4][2] = {{0,1},{1,1},{1,0},{0,0}};
constexpr uint8_t QUAD_INDICES[6] = {0,1,2,0,2,3};
uint8_t encodeNormal(int8_t x,int8_t y,int8_t z){uint8_t c=0;if(x>0)c|=1;if(x<0)c|=2;if(y>0)c|=4;if(y<0)c|=8;if(z>0)c|=16;if(z<0)c|=32;return c;}
uint16_t packUV(uint8_t u,uint8_t v){return (uint16_t)u|((uint16_t)v<<8);}
uint8_t packAOLight(uint8_t ao,uint8_t l){return (ao&0xF)|((l&0xF)<<4);}
}

ChunkMesh::ChunkMesh():m_empty(true){m_coord={0,0,0};}
ChunkMesh::~ChunkMesh()=default;
void ChunkMesh::clear(){for(auto& v:m_vertices)v.clear();for(auto& i:m_indices)i.clear();m_empty=true;}
bool ChunkMesh::isEmpty() const {return m_empty;}

const BlockData* ChunkMesh::getNeighbor(const BlockData* blocks,int x,int y,int z,BlockFace face) const {
    switch(face){case BlockFace::PositiveX:++x;break;case BlockFace::NegativeX:--x;break;case BlockFace::PositiveY:++y;break;case BlockFace::NegativeY:--y;break;case BlockFace::PositiveZ:++z;break;case BlockFace::NegativeZ:--z;break;default:return nullptr;}
    if(x<0||x>=SECTION_SIZE||y<0||y>=SECTION_SIZE||z<0||z>=SECTION_SIZE)return nullptr;
    return &blocks[x+y*SECTION_SIZE+z*SECTION_SIZE*SECTION_SIZE];
}
bool ChunkMesh::isFaceHidden(const BlockData* blocks,int x,int y,int z,BlockFace face) const {
    const BlockData* nb=getNeighbor(blocks,x,y,z,face);if(!nb)return false;if(nb->id==0)return false;
    const BlockData& cur=blocks[x+y*SECTION_SIZE+z*SECTION_SIZE*SECTION_SIZE];
    if(cur.transparent&&!nb->transparent)return true;if(!cur.transparent&&nb->transparent)return false;
    if(cur.transparent&&nb->transparent)return cur.id==nb->id;return true;
}
uint8_t ChunkMesh::computeVertexAO(const BlockData*,int,int,int,BlockFace,int) const {return 3;}

void ChunkMesh::addQuad(RenderBucket bucket,int x,int y,int z,BlockFace face,const BlockData& block,const uint8_t ao[4]){
    auto& verts=m_vertices[(size_t)bucket]; auto& inds=m_indices[(size_t)bucket];
    uint32_t base=(uint32_t)verts.size(); int fi=(int)face;
    for(int v=0;v<4;v++){
        ChunkVertex vt;
        vt.pos[0]=(int16_t)(x+FACE_VERTS[fi][v][0]);vt.pos[1]=(int16_t)(y+FACE_VERTS[fi][v][1]);vt.pos[2]=(int16_t)(z+FACE_VERTS[fi][v][2]);
        vt.uv=packUV(FACE_UVS[v][0],FACE_UVS[v][1]);
        vt.normal=encodeNormal(FACE_NORMALS[fi][0],FACE_NORMALS[fi][1],FACE_NORMALS[fi][2]);
        vt.ao_light=packAOLight(ao[v],block.light&0xF);vt.block_id=block.id;memset(vt._pad,0,4);
        verts.push_back(vt);
    }
    for(int i=0;i<6;i++)inds.push_back((uint16_t)(base+QUAD_INDICES[i]));
    m_empty=false;
}

void ChunkMesh::build(const BlockData* blocks,const SectionCoord& coord){
    clear();m_coord=coord;if(!blocks)return;
    for(int y=0;y<SECTION_SIZE;y++)for(int z=0;z<SECTION_SIZE;z++)for(int x=0;x<SECTION_SIZE;x++){
        const BlockData& b=blocks[x+y*SECTION_SIZE+z*256];if(b.id==0)continue;
        RenderBucket bucket=b.cutout?RenderBucket::Cutout:(b.transparent?RenderBucket::Transparent:RenderBucket::Opaque);
        for(int f=0;f<(int)BlockFace::Count;f++){
            BlockFace face=(BlockFace)f;if(isFaceHidden(blocks,x,y,z,face))continue;
            uint8_t ao[4];for(int v=0;v<4;v++)ao[v]=computeVertexAO(blocks,x,y,z,face,v);
            addQuad(bucket,x,y,z,face,b,ao);
        }
    }
    BERENDER_DEBUG("ChunkMesh (%d,%d,%d): opaque=%u cutout=%u transparent=%u",coord.x,coord.y,coord.z,vertexCount(RenderBucket::Opaque),vertexCount(RenderBucket::Cutout),vertexCount(RenderBucket::Transparent));
}
const std::vector<ChunkVertex>& ChunkMesh::vertices(RenderBucket b) const {return m_vertices[(size_t)b];}
const std::vector<uint16_t>& ChunkMesh::indices(RenderBucket b) const {return m_indices[(size_t)b];}
uint32_t ChunkMesh::vertexCount(RenderBucket b) const {return (uint32_t)m_vertices[(size_t)b].size();}
uint32_t ChunkMesh::indexCount(RenderBucket b) const {return (uint32_t)m_indices[(size_t)b].size();}
void ChunkMesh::computeBounds(float& minX,float& minY,float& minZ,float& maxX,float& maxY,float& maxZ) const {
    minX=(float)m_coord.x*16;minY=(float)m_coord.y*16;minZ=(float)m_coord.z*16;maxX=minX+16;maxY=minY+16;maxZ=minZ+16;
}
} // namespace berender
