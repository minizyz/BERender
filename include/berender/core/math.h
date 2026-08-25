#pragma once
#include "types.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace berender { namespace math {

struct Vec2 { float x,y; };
struct Vec3 { float x,y,z; };
struct Vec4 { float x,y,z,w; };

struct Mat4 {
    float m[16];
    static Mat4 identity() { Mat4 r; memset(r.m,0,sizeof(r.m)); r.m[0]=r.m[5]=r.m[10]=r.m[15]=1; return r; }
    static Mat4 perspective(float fovY,float aspect,float nearZ,float farZ) {
        Mat4 r=identity(); float t=tan(fovY*0.5f);
        r.m[0]=1/(aspect*t); r.m[5]=1/t; r.m[10]=farZ/(nearZ-farZ); r.m[11]=-1;
        r.m[14]=(nearZ*farZ)/(nearZ-farZ); r.m[15]=0; return r;
    }
    static Mat4 lookAt(const Vec3& eye,const Vec3& center,const Vec3& up) {
        Vec3 f={center.x-eye.x,center.y-eye.y,center.z-eye.z};
        float l=sqrt(f.x*f.x+f.y*f.y+f.z*f.z); f.x/=l;f.y/=l;f.z/=l;
        Vec3 s={f.y*up.z-f.z*up.y,f.z*up.x-f.x*up.z,f.x*up.y-f.y*up.x};
        l=sqrt(s.x*s.x+s.y*s.y+s.z*s.z); s.x/=l;s.y/=l;s.z/=l;
        Vec3 u={s.y*f.z-s.z*f.y,s.z*f.x-s.x*f.z,s.x*f.y-s.y*f.x};
        Mat4 r=identity();
        r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z; r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;
        r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
        r.m[12]=-(s.x*eye.x+s.y*eye.y+s.z*eye.z);
        r.m[13]=-(u.x*eye.x+u.y*eye.y+u.z*eye.z);
        r.m[14]=(f.x*eye.x+f.y*eye.y+f.z*eye.z); return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r; for(int c=0;c<4;c++)for(int row=0;row<4;row++){
            float s=0; for(int k=0;k<4;k++)s+=m[k*4+row]*o.m[c*4+k]; r.m[c*4+row]=s;
        } return r;
    }
};

struct Frustum {
    Vec4 planes[6];
    static Frustum fromViewProj(const Mat4& vp) {
        Frustum f; const float* m=vp.m;
        f.planes[0]={m[3]+m[0],m[7]+m[4],m[11]+m[8],m[15]+m[12]};
        f.planes[1]={m[3]-m[0],m[7]-m[4],m[11]-m[8],m[15]-m[12]};
        f.planes[2]={m[3]+m[1],m[7]+m[5],m[11]+m[9],m[15]+m[13]};
        f.planes[3]={m[3]-m[1],m[7]-m[5],m[11]-m[9],m[15]-m[13]};
        f.planes[4]={m[3]+m[2],m[7]+m[6],m[11]+m[10],m[15]+m[14]};
        f.planes[5]={m[3]-m[2],m[7]-m[6],m[11]-m[10],m[15]-m[14]};
        for(int i=0;i<6;i++){float l=sqrt(f.planes[i].x*f.planes[i].x+f.planes[i].y*f.planes[i].y+f.planes[i].z*f.planes[i].z);if(l>0){f.planes[i].x/=l;f.planes[i].y/=l;f.planes[i].z/=l;f.planes[i].w/=l;}}
        return f;
    }
    bool testAABB(const Vec3& min,const Vec3& max) const {
        for(int i=0;i<6;i++){const Vec4& p=planes[i];float px=(p.x>=0)?max.x:min.x,py=(p.y>=0)?max.y:min.y,pz=(p.z>=0)?max.z:min.z;if(p.x*px+p.y*py+p.z*pz+p.w<0)return false;}return true;
    }
    bool testSection(int32_t sx,int32_t sy,int32_t sz) const {
        Vec3 min={(float)sx*16,(float)sy*16,(float)sz*16}; Vec3 max={min.x+16,min.y+16,min.z+16}; return testAABB(min,max);
    }
};

inline float clamp(float v,float lo,float hi){return std::max(lo,std::min(hi,v));}
inline uint32_t alignUp(uint32_t v,uint32_t a){return (v+a-1)&~(a-1);}
inline uint64_t alignUp64(uint64_t v,uint64_t a){return (v+a-1)&~(a-1);}

}} // namespace berender::math
