#include "berender/backend/gles_backend.h"
#include "berender/core/logger.h"
#include <cstring>
#include <algorithm>
#ifdef BERENDER_PLATFORM_ANDROID
#include <EGL/egl.h>
#include <GLES3/gl31.h>
#endif
namespace berender {
namespace {
const char* VERT_OPAQUE=R"(#version 310 es
precision highp float;
layout(location=0)in vec3 aPos;layout(location=1)in vec2 aUV;layout(location=2)in uint aNormal;layout(location=3)in uint aAOLight;layout(location=4)in uint aBlockId;
layout(std140,binding=0)uniform CameraUBO{mat4 viewProj;vec3 cameraPos;float nearPlane;float farPlane;float time;uint renderFlags;};
layout(std430,binding=2)readonly buffer SectionSSBO{vec3 worldOffset[];};
out vec2 vUV;out float vAO;out float vLight;flat out uint vBlockId;
void main(){vec3 wp=aPos+worldOffset[gl_InstanceID];gl_Position=viewProj*vec4(wp,1.0);vUV=aUV;vAO=float(aAOLight&0xFu)/3.0;vLight=float((aAOLight>>4)&0xFu)/15.0;vBlockId=aBlockId;}
)";
const char* FRAG_OPAQUE=R"(#version 310 es
precision highp float;
in vec2 vUV;in float vAO;in float vLight;flat in uint vBlockId;
layout(std430,binding=1)readonly buffer MaterialSSBO{vec4 baseColor[];};
out vec4 fragColor;
void main(){vec4 c=baseColor[vBlockId];float l=vAO*(0.3+0.7*vLight);fragColor=vec4(c.rgb*l,c.a);}
)";
const char* VERT_CUTOUT=R"(#version 310 es
precision highp float;
layout(location=0)in vec3 aPos;layout(location=1)in vec2 aUV;layout(location=2)in uint aNormal;layout(location=3)in uint aAOLight;layout(location=4)in uint aBlockId;
layout(std140,binding=0)uniform CameraUBO{mat4 viewProj;vec3 cameraPos;float nearPlane;float farPlane;float time;uint renderFlags;};
layout(std430,binding=2)readonly buffer SectionSSBO{vec3 worldOffset[];};
out vec2 vUV;out float vAO;out float vLight;flat out uint vBlockId;
void main(){vec3 wp=aPos+worldOffset[gl_InstanceID];gl_Position=viewProj*vec4(wp,1.0);vUV=aUV;vAO=float(aAOLight&0xFu)/3.0;vLight=float((aAOLight>>4)&0xFu)/15.0;vBlockId=aBlockId;}
)";
const char* FRAG_CUTOUT=R"(#version 310 es
precision highp float;
in vec2 vUV;in float vAO;in float vLight;flat in uint vBlockId;
layout(std430,binding=1)readonly buffer MaterialSSBO{vec4 baseColor[];};
out vec4 fragColor;
void main(){vec4 c=baseColor[vBlockId];if(c.a<0.5)discard;float l=vAO*(0.3+0.7*vLight);fragColor=vec4(c.rgb*l,1.0);}
)";
const char* VERT_TRANSPARENT=R"(#version 310 es
precision highp float;
layout(location=0)in vec3 aPos;layout(location=1)in vec2 aUV;layout(location=2)in uint aNormal;layout(location=3)in uint aAOLight;layout(location=4)in uint aBlockId;
layout(std140,binding=0)uniform CameraUBO{mat4 viewProj;vec3 cameraPos;float nearPlane;float farPlane;float time;uint renderFlags;};
layout(std430,binding=2)readonly buffer SectionSSBO{vec3 worldOffset[];};
out vec2 vUV;out float vAO;out float vLight;flat out uint vBlockId;
void main(){vec3 wp=aPos+worldOffset[gl_InstanceID];gl_Position=viewProj*vec4(wp,1.0);vUV=aUV;vAO=float(aAOLight&0xFu)/3.0;vLight=float((aAOLight>>4)&0xFu)/15.0;vBlockId=aBlockId;}
)";
const char* FRAG_TRANSPARENT=R"(#version 310 es
precision highp float;
in vec2 vUV;in float vAO;in float vLight;flat in uint vBlockId;
layout(std430,binding=1)readonly buffer MaterialSSBO{vec4 baseColor[];};
out vec4 fragColor;
void main(){vec4 c=baseColor[vBlockId];float l=vAO*(0.3+0.7*vLight);fragColor=vec4(c.rgb*l,c.a);}
)";
}
GLESBackend::GLESBackend():m_nextBufferId(1),m_nextTextureId(1),m_nextPipelineId(1),m_currentVAO(0),m_currentProgram(0),m_eglDisplay(nullptr),m_eglSurface(nullptr),m_eglContext(nullptr),m_initialized(false),m_frameStarted(false){memset(&m_caps,0,sizeof(m_caps));m_caps.type=BackendType::OpenGLES;m_currentVertexBuffer=BufferHandle::invalid();m_currentIndexBuffer=BufferHandle::invalid();}
GLESBackend::~GLESBackend(){shutdown();}
Result GLESBackend::init(void* nativeWindow){
    BERENDER_INFO("GLESBackend::init");
#ifdef BERENDER_PLATFORM_ANDROID
    Result r=initEGL(nativeWindow);if(r!=Result::Success)return r;
#else
    BERENDER_WARN("GLES: non-Android, assuming external GL context");
#endif
    const char* renderer=(const char*)glGetString(GL_RENDERER);
    const char* version=(const char*)glGetString(GL_VERSION);
    if(renderer)strncpy(m_caps.deviceName,renderer,sizeof(m_caps.deviceName)-1);
    m_caps.multiDrawIndirect=true;m_caps.multiDrawIndirectCount=false;m_caps.storageBuffer=true;m_caps.dynamicRendering=false;m_caps.deviceGeneratedCommands=false;m_caps.minIndirectBufferOffsetAlignment=4;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,(GLint*)&m_caps.maxTextureSize);
    BERENDER_INFO("GLES initialized (renderer: %s, version: %s)",renderer?renderer:"unknown",version?version:"unknown");
    m_initialized=true;return Result::Success;
}
void GLESBackend::shutdown(){
    if(!m_initialized)return;BERENDER_INFO("GLESBackend::shutdown");
    for(auto&[id,r]:m_buffers)glDeleteBuffers(1,&r.id);m_buffers.clear();
    for(auto&[id,r]:m_textures)glDeleteTextures(1,&r.id);m_textures.clear();
    for(auto&[id,r]:m_pipelines){if(r.program)glDeleteProgram(r.program);if(r.vao)glDeleteVertexArrays(1,&r.vao);}m_pipelines.clear();
#ifdef BERENDER_PLATFORM_ANDROID
    terminateEGL();
#endif
    m_initialized=false;m_frameStarted=false;
}
Result GLESBackend::beginFrame(){
    if(!m_initialized)return Result::ErrorInitializationFailed;
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
    m_frameStarted=true;return Result::Success;
}
Result GLESBackend::endFrame(){
    if(!m_initialized||!m_frameStarted)return Result::ErrorUnknown;
#ifdef BERENDER_PLATFORM_ANDROID
    if(m_eglDisplay&&m_eglSurface)eglSwapBuffers((EGLDisplay)m_eglDisplay,(EGLSurface)m_eglSurface);
#endif
    m_frameStarted=false;return Result::Success;
}
BufferHandle GLESBackend::createBuffer(uint64_t size,BufferUsage usage,MemoryHint hint){
    GLuint b;glGenBuffers(1,&b);
    GLenum target=GL_ARRAY_BUFFER;
    if((uint32_t)usage&(uint32_t)BufferUsage::Index)target=GL_ELEMENT_ARRAY_BUFFER;
    else if((uint32_t)usage&(uint32_t)BufferUsage::Uniform)target=GL_UNIFORM_BUFFER;
    else if((uint32_t)usage&(uint32_t)BufferUsage::Storage)target=GL_SHADER_STORAGE_BUFFER;
    else if((uint32_t)usage&(uint32_t)BufferUsage::Indirect)target=GL_DRAW_INDIRECT_BUFFER;
    glBindBuffer(target,b);
    GLenum ug=GL_STATIC_DRAW;if(hint==MemoryHint::HostVisible||hint==MemoryHint::Staging)ug=GL_DYNAMIC_DRAW;
    glBufferData(target,(GLsizeiptr)size,nullptr,ug);glBindBuffer(target,0);
    uint64_t id=m_nextBufferId++;m_buffers[id]={b,size,usage,hint,false};return {id};
}
void GLESBackend::destroyBuffer(BufferHandle h){auto it=m_buffers.find(h.id);if(it==m_buffers.end())return;glDeleteBuffers(1,&it->second.id);m_buffers.erase(it);}
void* GLESBackend::mapBuffer(BufferHandle h,uint64_t offset,uint64_t size){
    auto it=m_buffers.find(h.id);if(it==m_buffers.end())return nullptr;
    GLenum target=GL_ARRAY_BUFFER;
    if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Index)target=GL_ELEMENT_ARRAY_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Uniform)target=GL_UNIFORM_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Storage)target=GL_SHADER_STORAGE_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Indirect)target=GL_DRAW_INDIRECT_BUFFER;
    glBindBuffer(target,it->second.id);
    return glMapBufferRange(target,(GLintptr)offset,(GLsizeiptr)size,GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
}
void GLESBackend::unmapBuffer(BufferHandle h){
    auto it=m_buffers.find(h.id);if(it==m_buffers.end())return;
    GLenum target=GL_ARRAY_BUFFER;
    if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Index)target=GL_ELEMENT_ARRAY_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Uniform)target=GL_UNIFORM_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Storage)target=GL_SHADER_STORAGE_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Indirect)target=GL_DRAW_INDIRECT_BUFFER;
    glBindBuffer(target,it->second.id);glUnmapBuffer(target);
}
void GLESBackend::updateBuffer(BufferHandle h,uint64_t offset,const void* data,uint64_t size){
    auto it=m_buffers.find(h.id);if(it==m_buffers.end())return;
    GLenum target=GL_ARRAY_BUFFER;
    if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Index)target=GL_ELEMENT_ARRAY_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Uniform)target=GL_UNIFORM_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Storage)target=GL_SHADER_STORAGE_BUFFER;
    else if((uint32_t)it->second.usage&(uint32_t)BufferUsage::Indirect)target=GL_DRAW_INDIRECT_BUFFER;
    glBindBuffer(target,it->second.id);glBufferSubData(target,(GLintptr)offset,(GLsizeiptr)size,data);
}
TextureHandle GLESBackend::createTexture(uint32_t w,uint32_t h,uint32_t ch,const void* data){
    GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
    GLenum fmt=GL_RGBA;if(ch==3)fmt=GL_RGB;else if(ch==1)fmt=GL_RED;
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D,0);
    uint64_t id=m_nextTextureId++;m_textures[id]={t,w,h};return {id};
}
void GLESBackend::destroyTexture(TextureHandle h){auto it=m_textures.find(h.id);if(it==m_textures.end())return;glDeleteTextures(1,&it->second.id);m_textures.erase(it);}
PipelineHandle GLESBackend::createPipeline(RenderBucket bucket,const Material* material){
    (void)material;
    const char* vs=getVertexShaderSource(bucket);const char* fs=getFragmentShaderSource(bucket);
    uint32_t vsh,fsh;if(compileShader(GL_VERTEX_SHADER,vs,vsh)!=Result::Success)return PipelineHandle::invalid();
    if(compileShader(GL_FRAGMENT_SHADER,fs,fsh)!=Result::Success){glDeleteShader(vsh);return PipelineHandle::invalid();}
    uint32_t prog;if(linkProgram(vsh,fsh,prog)!=Result::Success){glDeleteShader(vsh);glDeleteShader(fsh);return PipelineHandle::invalid();}
    glDeleteShader(vsh);glDeleteShader(fsh);
    GLuint vao;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glBindVertexArray(0);
    uint64_t id=m_nextPipelineId++;m_pipelines[id]={prog,vao,bucket,-1,-1,-1};return {id};
}
void GLESBackend::destroyPipeline(PipelineHandle h){auto it=m_pipelines.find(h.id);if(it==m_pipelines.end())return;if(it->second.program)glDeleteProgram(it->second.program);if(it->second.vao)glDeleteVertexArrays(1,&it->second.vao);m_pipelines.erase(it);}
Result GLESBackend::compileShader(uint32_t type,const char* src,uint32_t& out){
    GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);
    GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(s,sizeof(log),nullptr,log);BERENDER_ERROR("Shader compile: %s",log);glDeleteShader(s);return Result::ErrorShaderCompilation;}
    out=s;return Result::Success;
}
Result GLESBackend::linkProgram(uint32_t vs,uint32_t fs,uint32_t& out){
    GLuint p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);
    GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[1024];glGetProgramInfoLog(p,sizeof(log),nullptr,log);BERENDER_ERROR("Link: %s",log);glDeleteProgram(p);return Result::ErrorShaderCompilation;}
    out=p;return Result::Success;
}
const char* GLESBackend::getVertexShaderSource(RenderBucket b){switch(b){case RenderBucket::Opaque:return VERT_OPAQUE;case RenderBucket::Cutout:return VERT_CUTOUT;case RenderBucket::Transparent:return VERT_TRANSPARENT;default:return VERT_OPAQUE;}}
const char* GLESBackend::getFragmentShaderSource(RenderBucket b){switch(b){case RenderBucket::Opaque:return FRAG_OPAQUE;case RenderBucket::Cutout:return FRAG_CUTOUT;case RenderBucket::Transparent:return FRAG_TRANSPARENT;default:return FRAG_OPAQUE;}}
void GLESBackend::bindGlobalBuffers(BufferHandle vb,BufferHandle ib){m_currentVertexBuffer=vb;m_currentIndexBuffer=ib;auto vit=m_buffers.find(vb.id),iit=m_buffers.find(ib.id);if(vit!=m_buffers.end())glBindBuffer(GL_ARRAY_BUFFER,vit->second.id);if(iit!=m_buffers.end())glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,iit->second.id);}
void GLESBackend::bindCameraUniform(BufferHandle ubo){auto it=m_buffers.find(ubo.id);if(it==m_buffers.end())return;glBindBufferBase(GL_UNIFORM_BUFFER,0,it->second.id);}
void GLESBackend::bindMaterialStorage(BufferHandle ssbo){auto it=m_buffers.find(ssbo.id);if(it==m_buffers.end())return;glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,it->second.id);}
void GLESBackend::bindSectionStorage(BufferHandle ssbo){auto it=m_buffers.find(ssbo.id);if(it==m_buffers.end())return;glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,it->second.id);}
void GLESBackend::bindPipeline(PipelineHandle p){auto it=m_pipelines.find(p.id);if(it==m_pipelines.end())return;m_currentProgram=it->second.program;m_currentVAO=it->second.vao;glUseProgram(it->second.program);glBindVertexArray(it->second.vao);}
void GLESBackend::drawIndexedIndirect(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride){
    auto it=m_buffers.find(indirect.id);if(it==m_buffers.end())return;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,it->second.id);
    glMultiDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_SHORT,(const void*)(uintptr_t)offset,(GLsizei)count,(GLsizei)stride);
    BERENDER_DEBUG("glMultiDrawElementsIndirect: offset=%u count=%u",offset,count);
}
void GLESBackend::drawIndexedIndirectFallback(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride){
    auto it=m_buffers.find(indirect.id);if(it==m_buffers.end())return;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,it->second.id);
    for(uint32_t i=0;i<count;i++)glDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_SHORT,(const void*)(uintptr_t)(offset+i*stride));
    BERENDER_DEBUG("GLES fallback: %u draws",count);
}
void GLESBackend::drawIndexed(uint32_t ic,uint32_t inst,uint32_t first,int32_t voff,uint32_t finst){
    glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES,(GLsizei)ic,GL_UNSIGNED_SHORT,(const void*)(uintptr_t)(first*2),(GLsizei)inst,voff,finst);
}
void GLESBackend::setViewport(const Viewport& vp){glViewport((GLint)vp.x,(GLint)vp.y,(GLsizei)vp.width,(GLsizei)vp.height);glDepthRangef(vp.minDepth,vp.maxDepth);}
void GLESBackend::setScissor(const ScissorRect& r){glEnable(GL_SCISSOR_TEST);glScissor(r.x,r.y,(GLsizei)r.width,(GLsizei)r.height);}
void GLESBackend::bufferMemoryBarrier(BufferHandle,uint64_t,uint64_t){glMemoryBarrier(GL_COMMAND_BARRIER_BIT|GL_SHADER_STORAGE_BARRIER_BIT);}
#ifdef BERENDER_PLATFORM_ANDROID
Result GLESBackend::initEGL(void* nativeWindow){
    EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(d==EGL_NO_DISPLAY){BERENDER_ERROR("No EGL display");return Result::ErrorInitializationFailed;}
    EGLint major,minor;if(!eglInitialize(d,&major,&minor)){BERENDER_ERROR("EGL init failed");return Result::ErrorInitializationFailed;}
    const EGLint ca[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_BLUE_SIZE,8,EGL_GREEN_SIZE,8,EGL_RED_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
    EGLint nc;EGLConfig cfg;if(!eglChooseConfig(d,ca,&cfg,1,&nc)||nc==0){BERENDER_ERROR("EGL config failed");return Result::ErrorInitializationFailed;}
    EGLSurface s=eglCreateWindowSurface(d,cfg,nativeWindow,nullptr);if(s==EGL_NO_SURFACE){BERENDER_ERROR("EGL surface failed");return Result::ErrorInitializationFailed;}
    const EGLint cta[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    EGLContext c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,cta);if(c==EGL_NO_CONTEXT){BERENDER_ERROR("EGL context failed");eglDestroySurface(d,s);return Result::ErrorInitializationFailed;}
    if(!eglMakeCurrent(d,s,s,c)){BERENDER_ERROR("EGL make current failed");eglDestroyContext(d,c);eglDestroySurface(d,s);return Result::ErrorInitializationFailed;}
    m_eglDisplay=d;m_eglSurface=s;m_eglContext=c;BERENDER_INFO("EGL %d.%d ready",major,minor);return Result::Success;
}
void GLESBackend::terminateEGL(){
    if(m_eglDisplay){eglMakeCurrent((EGLDisplay)m_eglDisplay,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(m_eglContext)eglDestroyContext((EGLDisplay)m_eglDisplay,(EGLContext)m_eglContext);if(m_eglSurface)eglDestroySurface((EGLDisplay)m_eglDisplay,(EGLSurface)m_eglSurface);eglTerminate((EGLDisplay)m_eglDisplay);}m_eglDisplay=nullptr;m_eglSurface=nullptr;m_eglContext=nullptr;
}
#else
Result GLESBackend::initEGL(void*){return Result::Success;}
void GLESBackend::terminateEGL(){}
#endif
} // namespace berender
