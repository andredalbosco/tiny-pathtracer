#pragma once

// Included after Float3 and Triangle. Every owner uses fixed C arrays.
// 0: original records; 1: geometry/surface records; 2: Float3 columns;
// 3: scalar geometry columns. The intersection math and ray axis are unchanged.
#ifndef TRIANGLE_LAYOUT
#define TRIANGLE_LAYOUT 3
#endif

struct Triangles {
    static constexpr int capacity=64;
#if TRIANGLE_LAYOUT == 0
    Triangle records[capacity];
    Triangle operator[](int i) const { return records[i]; }
    void set(int i,const Triangle& t) { records[i]=t; }
#else
    struct Surface { Float3 normal,albedo,emission; };
    Surface surfaces[capacity];
#if TRIANGLE_LAYOUT == 1
    struct Geometry { Float3 p,e1,e2; };
    Geometry geometry[capacity];
    Triangle operator[](int i) const {
        Triangle t;
        t.p=geometry[i].p;t.e1=geometry[i].e1;t.e2=geometry[i].e2;
        t.normal=surfaces[i].normal;t.albedo=surfaces[i].albedo;t.emission=surfaces[i].emission;
        return t;
    }
    void set(int i,const Triangle& t) {
        geometry[i]={t.p,t.e1,t.e2};surfaces[i]={t.normal,t.albedo,t.emission};
    }
#elif TRIANGLE_LAYOUT == 2
    Float3 p[capacity],e1[capacity],e2[capacity];
    Triangle operator[](int i) const {
        Triangle t;
        t.p=p[i];t.e1=e1[i];t.e2=e2[i];
        t.normal=surfaces[i].normal;t.albedo=surfaces[i].albedo;t.emission=surfaces[i].emission;
        return t;
    }
    void set(int i,const Triangle& t) {
        p[i]=t.p;e1[i]=t.e1;e2[i]=t.e2;surfaces[i]={t.normal,t.albedo,t.emission};
    }
#elif TRIANGLE_LAYOUT == 3
    struct Float3Array {
        float x[capacity],y[capacity],z[capacity];
        Float3 get(int i) const { return {x[i],y[i],z[i]}; }
        void set(int i,Float3 v) { x[i]=v.x;y[i]=v.y;z[i]=v.z; }
    };
    Float3Array p,e1,e2;
    Triangle operator[](int i) const {
        Triangle t;
        t.p=p.get(i);t.e1=e1.get(i);t.e2=e2.get(i);
        t.normal=surfaces[i].normal;t.albedo=surfaces[i].albedo;t.emission=surfaces[i].emission;
        return t;
    }
    void set(int i,const Triangle& t) {
        p.set(i,t.p);e1.set(i,t.e1);e2.set(i,t.e2);surfaces[i]={t.normal,t.albedo,t.emission};
    }
#else
#error Unsupported TRIANGLE_LAYOUT
#endif
#endif
};
