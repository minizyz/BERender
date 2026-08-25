#pragma once
#include "berender/core/types.h"
#include <vector>
#include <cstdint>

namespace berender {

enum class BlockFace : uint8_t { PositiveX=0,NegativeX=1,PositiveY=2,NegativeY=3,PositiveZ=4,NegativeZ=5,Count };

struct BlockData {
    uint16_t id; uint8_t light, ao; bool transparent, cutout;
};

class ChunkMesh {
public:
    static constexpr int SECTION_SIZE=16;
    static constexpr int SECTION_VOLUME=SECTION_SIZE*SECTION_SIZE*SECTION_SIZE;
    ChunkMesh(); ~ChunkMesh();
    void build(const BlockData* blocks, const SectionCoord& coord);
    void clear();
    const std::vector<ChunkVertex>& vertices(RenderBucket bucket) const;
    const std::vector<uint16_t>& indices(RenderBucket bucket) const;
    uint32_t vertexCount(RenderBucket bucket) const;
    uint32_t indexCount(RenderBucket bucket) const;
    bool isEmpty() const;
    const SectionCoord& coord() const { return m_coord; }
    void computeBounds(float& minX,float& minY,float& minZ,float& maxX,float& maxY,float& maxZ) const;
private:
    bool isFaceHidden(const BlockData* blocks,int x,int y,int z,BlockFace face) const;
    const BlockData* getNeighbor(const BlockData* blocks,int x,int y,int z,BlockFace face) const;
    void addQuad(RenderBucket bucket,int x,int y,int z,BlockFace face,const BlockData& block,const uint8_t ao[4]);
    uint8_t computeVertexAO(const BlockData* blocks,int x,int y,int z,BlockFace face,int vertexIndex) const;
    std::vector<ChunkVertex> m_vertices[static_cast<size_t>(RenderBucket::Count)];
    std::vector<uint16_t> m_indices[static_cast<size_t>(RenderBucket::Count)];
    SectionCoord m_coord;
    bool m_empty;
};

} // namespace berender
