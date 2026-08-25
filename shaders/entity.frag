// BERender - Entity Fragment Shader
#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "common.glsl"
layout(location=0) in vec2 inUV;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec3 inWorldPos;
layout(location=3) flat in uint inMaterialId;
layout(set=SET_GLOBAL,binding=BINDING_MATERIAL,std430) readonly buffer MaterialSSBO { MaterialData materials[]; };
layout(location=0) out vec4 outColor;
void main(){
    MaterialData mat=materials[inMaterialId];
    vec3 lightDir=normalize(vec3(0.5,1.0,0.3));
    float ndl=max(dot(normalize(inNormal),lightDir),0.0);
    vec3 lighting=vec3(0.3)+vec3(0.7)*ndl;
    vec3 color=mat.baseColor.rgb*lighting+mat.emissive;
    outColor=vec4(color,mat.baseColor.a);
}
