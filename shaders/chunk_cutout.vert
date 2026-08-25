// BERender - Chunk Cutout Vertex Shader
#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "common.glsl"
layout(location=ATTRIB_POS) in vec3 inPos;
layout(location=ATTRIB_UV) in vec2 inUV;
layout(location=ATTRIB_NORMAL) in uint inNormal;
layout(location=ATTRIB_AO_LIGHT) in uint inAOLight;
layout(location=ATTRIB_BLOCK_ID) in uint inBlockId;
layout(set=SET_GLOBAL,binding=BINDING_CAMERA) uniform CameraUBO { CameraData camera; };
layout(set=SET_GLOBAL,binding=BINDING_SECTION,std430) readonly buffer SectionSSBO { SectionData sections[]; };
layout(location=0) out vec2 outUV;
layout(location=1) out vec3 outNormal;
layout(location=2) out float outAO;
layout(location=3) out float outBlockLight;
layout(location=4) out vec3 outWorldPos;
layout(location=5) flat out uint outBlockId;
vec3 decodeNormal(uint e){ vec3 n=vec3(0); if(e&1u)n.x+=1; if(e&2u)n.x-=1; if(e&4u)n.y+=1; if(e&8u)n.y-=1; if(e&16u)n.z+=1; if(e&32u)n.z-=1; return normalize(n); }
void main(){
    uint si=uint(gl_InstanceID); vec3 wp=inPos+sections[si].worldOffset; outWorldPos=wp;
    gl_Position=camera.viewProj*vec4(wp,1.0);
    outUV=inUV; outNormal=decodeNormal(inNormal); outAO=decodeAO(inAOLight); outBlockLight=decodeBlockLight(inAOLight); outBlockId=inBlockId;
}
