#pragma once
#include "simple_kernels.h"

// Keep candidate tests as numerators. Divide only the closest accepted hit.
// Algebraically equivalent to Moller-Trumbore, with different float rounding.
struct NumeratorScene : RayScene {
    using RayScene::RayScene;
    void intersect_batch(const Ray* rays, Hit* hits, int count, Scratch&) const {
        float ox[8]{}, oy[8]{}, oz[8]{}, dx[8]{}, dy[8]{}, dz[8]{};
        float ts[8], ds[8], us[8]{}, vs[8]{};
        int ids[8];
        for (int r = 0; r < count; ++r) {
            ox[r] = rays[r].origin.x; oy[r] = rays[r].origin.y; oz[r] = rays[r].origin.z;
            dx[r] = rays[r].direction.x; dy[r] = rays[r].direction.y; dz[r] = rays[r].direction.z;
            ts[r] = INFINITY; ds[r] = 1; ids[r] = -1;
        }
        for (int i = 0; i < int(triangles.size()); ++i) {
            const Triangle& triangle = triangles[i];
            for (int r = 0; r < count; ++r) {
                Float3 direction{dx[r], dy[r], dz[r]};
                Float3 h = cross(direction, triangle.e2);
                float determinant = dot(triangle.e1, h);
                Float3 offset = Float3{ox[r], oy[r], oz[r]} - triangle.p;
                float u = dot(offset, h);
                Float3 q = cross(offset, triangle.e1);
                float v = dot(direction, q);
                float t = dot(triangle.e2, q);
                bool valid = (determinant >= 1e-10f) & (u >= 0) & (u <= determinant)
                           & (v >= 0) & (u + v <= determinant) & (t > 1e-4f * determinant)
                           & (t * ds[r] < ts[r] * determinant);
                ts[r] = valid ? t : ts[r];
                ds[r] = valid ? determinant : ds[r];
                us[r] = valid ? u : us[r];
                vs[r] = valid ? v : vs[r];
                ids[r] = valid ? i : ids[r];
            }
        }
        for (int r = 0; r < count; ++r) {
            float inverse = 1 / ds[r];
            hits[r] = {ts[r] * inverse, us[r] * inverse, vs[r] * inverse, ids[r]};
        }
    }
};

// Precompute the triangle plane and its two barycentric coordinate gradients.
// Reuses geometry work across all rays; preserves mathematical triangle tests.
struct PlaneScene : RayScene {
    using RayScene::RayScene;
    struct Coefficients { Float3 p, n, u, v; };
    std::vector<Coefficients> prepared;
    void prepare() {
        prepared.clear();
        for (const auto& t : triangles) {
            double ax=t.e1.x, ay=t.e1.y, az=t.e1.z, bx=t.e2.x, by=t.e2.y, bz=t.e2.z;
            double nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            double norm=nx*nx+ny*ny+nz*nz;
            Coefficients p{t.p, {float(nx),float(ny),float(nz)}, {}, {}};
            if (norm > 0) {
                p.u={float((by*nz-bz*ny)/norm),float((bz*nx-bx*nz)/norm),float((bx*ny-by*nx)/norm)};
                p.v={float((ny*az-nz*ay)/norm),float((nz*ax-nx*az)/norm),float((nx*ay-ny*ax)/norm)};
            }
            prepared.push_back(p);
        }
    }
    void intersect_batch(const Ray* rays, Hit* hits, int count, Scratch&) const {
        float ox[8]{}, oy[8]{}, oz[8]{}, dx[8]{}, dy[8]{}, dz[8]{};
        float ts[8], us[8]{}, vs[8]{};
        int ids[8];
        for (int r = 0; r < count; ++r) {
            ox[r]=rays[r].origin.x; oy[r]=rays[r].origin.y; oz[r]=rays[r].origin.z;
            dx[r]=rays[r].direction.x; dy[r]=rays[r].direction.y; dz[r]=rays[r].direction.z;
            ts[r]=INFINITY; ids[r]=-1;
        }
        for (int i=0; i<int(prepared.size()); ++i) {
            const auto& triangle=prepared[i];
            for (int r=0; r<count; ++r) {
                Float3 direction{dx[r],dy[r],dz[r]};
                Float3 offset=Float3{ox[r],oy[r],oz[r]}-triangle.p;
                float determinant=-dot(triangle.n,direction);
                float inverse=1/std::max(determinant,1e-10f);
                float t=dot(triangle.n,offset)*inverse;
                Float3 point=offset+direction*t;
                float u=dot(triangle.u,point), v=dot(triangle.v,point);
                bool valid=(determinant>=1e-10f)&(u>=0)&(u<=1)&(v>=0)&(u+v<=1)&(t>1e-4f)&(t<ts[r]);
                ts[r]=valid?t:ts[r]; us[r]=valid?u:us[r]; vs[r]=valid?v:vs[r]; ids[r]=valid?i:ids[r];
            }
        }
        for (int r=0; r<count; ++r) hits[r]={ts[r],us[r],vs[r],ids[r]};
    }
};
