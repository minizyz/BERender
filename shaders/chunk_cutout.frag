// BERender - Chunk Cutout Fragment Shader (alpha discard)
#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "common.glsl"
layout(location=0) in vec2 inUV;
layout(location=1) in vec3 inNormal;
layout(location=2) in float inAO;
layout(location=3) in float inBlockLight;
layout(location=4) in vec3 inWorldPos;
layout(location=5) flat in uint inBlockId;
layout(set=SET_GLOBAL,binding=BINDING_MATERIAL,std430) readonly buffer MaterialSSBO { MaterialData materials[]; };
layout(set=SET_GLOBAL,binding=BINDING_CAMERA) uniform CameraUBO { CameraData camera; };
layout(location=0) out vec4 outColor;
void main(){
    MaterialData mat=materials[inBlockId];
    if(mat.baseColor.a<0.5) discard;
    float ambient=0.3; float lighting=inAO*(ambient+(1.0-ambient)*max(1.0,inBlockLight));
    vec3 color=mat.baseColor.rgb*lighting+mat.emissive;
    float dist=length(inWorldPos-camera.position); float fog=clamp(1.0-exp(-dist*0.008),0.0,1.0);
    color=mix(color,vec3(0.6,0.75,0.9),fog); outColor=vec4(color,1.0);
}
