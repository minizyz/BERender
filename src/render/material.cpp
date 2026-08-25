#include "berender/render/material.h"
#include "berender/core/logger.h"
#include <cstring>
#include <algorithm>

namespace berender {

MaterialSystem::MaterialSystem():m_nextId(0){}
MaterialSystem::~MaterialSystem()=default;

uint32_t MaterialSystem::registerMaterial(const Material& mat){
    Material m=mat; m.id=m_nextId++;
    m.gpuData.textureIndex=m.textureIndex; m.gpuData.flags=static_cast<uint32_t>(m.flags);
    m_materials.push_back(m);
    if(!m.name.empty()) m_nameToId[m.name]=m.id;
    BERENDER_DEBUG("Material registered: id=%u name='%s'",m.id,m.name.c_str());
    return m.id;
}
const Material* MaterialSystem::getMaterial(uint32_t id) const {
    if(id>=m_materials.size())return nullptr; return &m_materials[id];
}
uint32_t MaterialSystem::findMaterial(const std::string& name) const {
    auto it=m_nameToId.find(name); return it==m_nameToId.end()?UINT32_MAX:it->second;
}
void MaterialSystem::buildGPUData(std::vector<uint8_t>& outData) const {
    outData.resize(gpuDataSize()); if(m_materials.empty())return;
    uint8_t* ptr=outData.data();
    for(const auto& mat:m_materials){memcpy(ptr,&mat.gpuData,sizeof(BlockMaterial));ptr+=sizeof(BlockMaterial);}
    BERENDER_DEBUG("Material GPU data: %u materials, %u bytes",materialCount(),gpuDataSize());
}
uint32_t MaterialSystem::registerTexture(const TextureDesc& tex){m_textures.push_back(tex);return static_cast<uint32_t>(m_textures.size()-1);}
void MaterialSystem::clear(){m_materials.clear();m_textures.clear();m_nameToId.clear();m_nextId=0;}

} // namespace berender
