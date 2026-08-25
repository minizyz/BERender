#pragma once
#include "berender/core/types.h"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace berender {

enum class MaterialFlag : uint32_t {
    None=0, Transparent=1<<0, Cutout=1<<1, Emissive=1<<2, DoubleSided=1<<3, PBR=1<<4,
};
inline MaterialFlag operator|(MaterialFlag a,MaterialFlag b){return static_cast<MaterialFlag>(static_cast<uint32_t>(a)|static_cast<uint32_t>(b));}

struct TextureDesc { std::string path; uint32_t width,height,channels; bool generateMipmaps; };

struct Material {
    uint32_t id;
    std::string name;
    BlockMaterial gpuData;
    MaterialFlag flags;
    uint32_t textureIndex;
    RenderBucket defaultBucket;
};

class MaterialSystem {
public:
    MaterialSystem(); ~MaterialSystem();
    uint32_t registerMaterial(const Material& mat);
    const Material* getMaterial(uint32_t id) const;
    uint32_t findMaterial(const std::string& name) const;
    void buildGPUData(std::vector<uint8_t>& outData) const;
    uint32_t materialCount() const { return static_cast<uint32_t>(m_materials.size()); }
    uint32_t gpuDataSize() const { return materialCount() * sizeof(BlockMaterial); }
    uint32_t registerTexture(const TextureDesc& tex);
    uint32_t textureCount() const { return static_cast<uint32_t>(m_textures.size()); }
    void clear();
private:
    std::vector<Material> m_materials;
    std::vector<TextureDesc> m_textures;
    std::unordered_map<std::string,uint32_t> m_nameToId;
    uint32_t m_nextId;
};

} // namespace berender
