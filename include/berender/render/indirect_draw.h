#pragma once
#include "berender/core/types.h"
#include "berender/render/frame_registry.h"
#include <vector>
#include <cstdint>

namespace berender {

class IndirectDrawBuilder {
public:
    IndirectDrawBuilder(); ~IndirectDrawBuilder();
    void init(uint32_t indirectAlignment);
    void build(const FrameRegistry& registry);
    void reset();
    const std::vector<DrawIndexedIndirectCommand>& commands(RenderBucket bucket) const;
    uint32_t drawCount(RenderBucket bucket) const;
    uint32_t totalDrawCount() const;
    uint32_t bucketByteOffset(RenderBucket bucket) const;
    uint32_t totalBufferSize() const;
    void serialize(uint8_t* dst, uint32_t dstSize) const;
    void setSoftwareFallback(bool enabled) { m_softwareFallback = enabled; }
    bool softwareFallback() const { return m_softwareFallback; }
private:
    uint32_t m_indirectAlignment;
    bool m_softwareFallback;
    std::vector<DrawIndexedIndirectCommand> m_commands[static_cast<size_t>(RenderBucket::Count)];
    uint32_t m_drawCounts[static_cast<size_t>(RenderBucket::Count)];
    uint32_t m_byteOffsets[static_cast<size_t>(RenderBucket::Count)];
};

} // namespace berender
