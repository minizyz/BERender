#pragma once

#include <cstdint>
#include <cstddef>

namespace berender {

enum class Result : int32_t {
    Success = 0,
    ErrorUnknown = -1,
    ErrorInvalidArgument = -2,
    ErrorOutOfMemory = -3,
    ErrorUnsupported = -4,
    ErrorDeviceLost = -5,
    ErrorInitializationFailed = -6,
    ErrorShaderCompilation = -7,
    ErrorSurfaceLost = -8,
};

enum class BackendType : uint8_t { Vulkan = 0, OpenGLES = 1 };

enum class RenderBucket : uint8_t {
    Opaque = 0, Cutout = 1, Transparent = 2, Entity = 3, Particle = 4, UI = 5, Count,
};

enum class BufferUsage : uint32_t {
    Vertex = 1<<0, Index = 1<<1, Uniform = 1<<2, Storage = 1<<3,
    Indirect = 1<<4, TransferSrc = 1<<5, TransferDst = 1<<6,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a)|static_cast<uint32_t>(b));
}

enum class MemoryHint : uint8_t { DeviceLocal, HostVisible, HostCached, Staging };

#pragma pack(push, 1)
struct ChunkVertex {
    int16_t  pos[3];
    uint16_t uv;
    uint8_t  normal;
    uint8_t  ao_light;
    uint16_t block_id;
    uint8_t  _pad[4];
};
#pragma pack(pop)
static_assert(sizeof(ChunkVertex) == 16, "ChunkVertex must be 16 bytes");

struct DrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
};
static_assert(sizeof(DrawIndexedIndirectCommand) == 20,
              "DrawIndexedIndirectCommand must match VkDrawIndexedIndirectCommand");

struct SectionInfo {
    float    worldOffset[3]; uint32_t padding0;
    float    lightColor[3];  uint32_t flags;
};
static_assert(sizeof(SectionInfo) == 32, "SectionInfo must be 32 bytes (std430)");

struct BlockMaterial {
    float baseColor[4]; float metallic; float roughness;
    float emissive[3]; uint32_t textureIndex; uint32_t flags; float _pad[2];
};
static_assert(sizeof(BlockMaterial) == 48, "BlockMaterial must be 48 bytes (std430)");

struct CameraUniform {
    float viewProj[16]; float position[3]; float nearPlane; float farPlane;
    float time; uint32_t renderFlags; float _pad[2];
};
static_assert(sizeof(CameraUniform) == 96, "CameraUniform must be 96 bytes (std140)");

struct SectionCoord {
    int32_t x, y, z;
    bool operator==(const SectionCoord& o) const { return x==o.x&&y==o.y&&z==o.z; }
    bool operator<(const SectionCoord& o) const {
        if(x!=o.x)return x<o.x; if(y!=o.y)return y<o.y; return z<o.z;
    }
};

struct Viewport { float x,y,width,height,minDepth,maxDepth; };
struct ScissorRect { int32_t x,y; uint32_t width,height; };

struct BackendCapabilities {
    BackendType type;
    bool multiDrawIndirect, multiDrawIndirectCount, storageBuffer, dynamicRendering, deviceGeneratedCommands;
    uint32_t maxTextureSize, maxUniformBufferRange, maxStorageBufferRange, minIndirectBufferOffsetAlignment;
    char deviceName[256];
};

} // namespace berender
