#include "experimental_kernels.h"
#include "render_driver.h"
#include <cassert>

struct D3 {
    double x,y,z;
    explicit D3(Float3 f):x(f.x),y(f.y),z(f.z) {}
    D3(double x,double y,double z):x(x),y(y),z(z) {}
    D3 operator-(D3 b) const {return {x-b.x,y-b.y,z-b.z};}
};
double dp(D3 a,D3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
D3 cp(D3 a,D3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Hit double_hit(const SceneData& scene,Ray ray) {
    Hit hit;double best=INFINITY;
    for (int i=0;i<int(scene.triangles.size());++i) {
        auto& tri=scene.triangles[i];D3 e1(tri.e1),e2(tri.e2),d(ray.direction);
        D3 h=cp(d,e2),s=D3(ray.origin)-D3(tri.p),q=cp(s,e1);
        double det=dp(e1,h); if (det<1e-10) continue;
        double u=dp(s,h)/det,v=dp(d,q)/det,t=dp(e2,q)/det;
        if (u>=0 && u<=1 && v>=0 && u+v<=1 && t>1e-4 && t<best) {
            best=t;hit={float(t),float(u),float(v),i};
        }
    }
    return hit;
}
template<class Scene> void compare(const char* name,const SceneData& data,const std::vector<Ray>& rays) {
    Scene scene(data);scene.prepare();typename Scene::Scratch scratch(data.triangles.size());
    ScalarScene original(data);EmptyScratch empty;
    std::uint64_t ids=0,double_ids=0,hit_presence=0;
    double max_t=0,max_uv=0;
    for (std::size_t first=0;first<rays.size();first+=8) {
        Hit hits[8];int count=std::min<std::size_t>(8,rays.size()-first);
        scene.intersect_batch(rays.data()+first,hits,count,scratch);
        for (int i=0;i<count;++i) {
            Hit old;original.intersect(rays[first+i],old,empty);auto ref=double_hit(data,rays[first+i]);auto h=hits[i];
            ids+=h.triangle!=old.triangle;double_ids+=h.triangle!=ref.triangle;
            hit_presence+=(h.triangle>=0)!=(old.triangle>=0);
            if (h.triangle>=0 && h.triangle==ref.triangle) {
                max_t=std::max(max_t,double(std::abs(h.t-ref.t)/std::max(1.f,std::abs(ref.t))));
                max_uv=std::max(max_uv,double(std::max(std::abs(h.u-ref.u),std::abs(h.v-ref.v))));
            }
        }
    }
    assert(max_t<1e-3 && max_uv<1e-3);
    std::cout << "{\"kernel\":\""<<name<<"\",\"triangles\":"<<data.triangles.size()<<",\"rays\":"<<rays.size()
              <<",\"float_id_differences\":"<<ids<<",\"double_id_differences\":"<<double_ids
              <<",\"hit_presence_differences\":"<<hit_presence<<",\"max_relative_t\":"<<max_t
              <<",\"max_barycentric_error\":"<<max_uv<<"}\n";
}
int main() {
    auto data=cornell_box();ScalarScene scalar(data);EmptyScratch scratch;
    std::vector<Ray> rays{{{},{}},{{278,278,-800},{0,0,1}}};
    // Camera and real bounce rays preserve the render's workload distribution.
    for (int y=0;y<64;++y) for (int x=0;x<64;++x) for (int s=0;s<4;++s) {
        Path p;p.active=true;p.random.seed(42,((std::uint64_t(y)*64+x)<<32)|s);
        Float2 uv=(Float2{float(x),float(y)}+Float2{p.random.uniform(),p.random.uniform()}*.5f)*(1.f/64);
        auto& c=data.camera;
        p.ray={c.position,normalize(c.u*(2*uv.x-1)+c.v*(-2*uv.y+1)+c.w*(1/std::tan(c.fovy*.5f*M_PI/180)))};
        for (int depth=0;depth<max_depth && p.active;++depth) {
            rays.push_back(p.ray);Hit hit;scalar.intersect(p.ray,hit,scratch);scatter(data,p,hit);
        }
    }
    Random rng;rng.seed(728,23);
    for (int i=0;i<20000;++i) {
        Float3 origin{rng.uniform()*1000-200,rng.uniform()*1000-200,rng.uniform()*1000-200};
        rays.push_back({origin,sample_hemisphere({0,0,1},rng)});
    }
    for (int n:{0,1,7,8,9,17,36}) {
        auto subset=data;subset.triangles.erase(subset.triangles.begin()+n,subset.triangles.end());
        compare<NumeratorScene>("numerator",subset,rays);compare<PlaneScene>("plane",subset,rays);
    }
    SceneData unit;
    unit.triangles.emplace_back(Float3{0,0,1},Float3{0,1,1},Float3{1,0,1},Float3{1,1,1});
    unit.triangles.push_back(unit.triangles.front()); // exact tie: retain the first
    std::vector<Ray> boundary;
    for (float u:{0.f,1e-7f,.25f,.5f,1.f}) for(float v:{0.f,1e-7f,.25f,.5f,1.f})
        for(float z:{0.f,.9998f,.9999f,std::nextafter(.9999f,1.f),1.f})
            boundary.push_back({{u,v,z},{0,0,1}});
    boundary.push_back({{.2f,.2f,0},{1,0,1e-12f}});boundary.push_back({{}, {}});
    compare<NumeratorScene>("numerator-boundary",unit,boundary);compare<PlaneScene>("plane-boundary",unit,boundary);
    // Demonstrate the known numerator cross-product overflow limitation.
    SceneData huge;
    huge.triangles.emplace_back(Float3{0,0,2},Float3{0,2e15f,2},Float3{2e15f,0,2},Float3{});
    huge.triangles.emplace_back(Float3{0,0,1},Float3{0,2e15f,1},Float3{2e15f,0,1},Float3{});
    NumeratorScene experiment(huge);experiment.prepare();Hit h;Ray ray{{1,1,0},{0,0,1}};
    experiment.intersect_batch(&ray,&h,1,scratch);
    assert(h.triangle==0 && double_hit(huge,ray).triangle==1);
    std::cout << "{\"known_extreme_overflow_reproduced\":true}\n";
}
