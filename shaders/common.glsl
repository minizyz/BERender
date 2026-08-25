// BERender - Common shader definitions
#ifndef BERENDER_COMMON_GLSL
#define BERENDER_COMMON_GLSL
#define ATTRIB_POS 0
#define ATTRIB_UV 1
#define ATTRIB_NORMAL 2
#define ATTRIB_AO_LIGHT 3
#define ATTRIB_BLOCK_ID 4
#define SET_GLOBAL 0
#define BINDING_CAMERA 0
#define BINDING_MATERIAL 1
#define BINDING_SECTION 2
#define BUCKET_OPAQUE 0
#define BUCKET_CUTOUT 1
#define BUCKET_TRANSPARENT 2
#define BUCKET_ENTITY 3
#define BUCKET_PARTICLE 4
#define BUCKET_UI 5
struct CameraData { mat4 viewProj; vec3 position; float nearPlane; float farPlane; float time; uint renderFlags; };
struct SectionData { vec3 worldOffset; uint padding0; vec3 lightColor; uint flags; };
struct MaterialData { vec4 baseColor; float metallic; float roughness; vec3 emissive; uint textureIndex; uint flags; };
float decodeAO(uint packed){ return float(packed & 0xFu)/3.0; }
float decodeBlockLight(uint packed){ return float((packed>>4)&0xFu)/15.0; }
vec3 lambertLight(vec3 n,vec3 ld,vec3 lc,float amb){ float ndl=max(dot(normalize(n),normalize(ld)),0.0); return lc*(amb+ndl*(1.0-amb)); }
vec3 gammaCorrect(vec3 c,float g){ return pow(c,vec3(1.0/g)); }
#endif
