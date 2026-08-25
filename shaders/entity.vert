// BERender - Entity Vertex Shader (instanced, no MDI)
#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "common.glsl"
layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inUV;
layout(location=2) in vec3 inNormal;
layout(location=3) in mat4 inWorldMatrix;
layout(location=7) in uint inMaterialId;
layout(set=SET_GLOBAL,binding=BINDING_CAMERA) uniform CameraUBO { CameraData camera; };
layout(location=0) out vec2 outUV;
layout(location=1) out vec3 outNormal;
layout(location=2) out vec3 outWorldPos;
layout(location=3) flat out uint outMaterialId;
void main(){
    vec4 wp=inWorldMatrix*vec4(inPos,1.0); outWorldPos=wp.xyz;
    gl_Position=camera.viewProj*wp;
    outUV=inUV; outNormal=mat3(inWorldMatrix)*inNormal; outMaterialId=inMaterialId;
}
