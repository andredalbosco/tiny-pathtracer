#include "simple_kernels.h"
#include "render_driver.h"
#include "quality_sampling.h"
#include <cassert>

int main() {
    auto data=cornell_box();
    QualityScene<Sampling::mis> scene(data); scene.prepare();
    Random random; random.seed(4321,7);
    double z=0,z2=0;
    for (int i=0;i<200000;++i) {
        int id=i%int(scene.triangles.size());
        auto d=scene.cosine_direction(id,random);
        float c=dot(d,scene.triangles[id].normal);
        assert(c>=-1e-6f && std::abs(dot(d,d)-1)<2e-6f);
        z+=c; z2+=c*c;
    }
    assert(std::abs(z/200000-2.0/3)<0.003);
    assert(std::abs(z2/200000-0.5)<0.003);
    assert(scene.light_area==13650);
    QualityScene<Sampling::mis>::Scratch scratch(data.triangles.size());
    // Visible emitter center from the floor, and a segment blocked by a box.
    Float3 origin{278,0.001f,278}, point{278,554,278};
    Float3 delta=point-origin; float distance=std::sqrt(dot(delta,delta));
    assert(scene.visible({origin,delta*(1/distance)},distance,10,scratch));
    delta=Float3{370,80,400}-Float3{370,80,-50}; distance=std::sqrt(dot(delta,delta));
    assert(!scene.visible({{370,80,-50},delta*(1/distance)},distance,10,scratch));
    // A blocker only 0.0005 units before the emitter must still occlude it.
    auto near_data=data;
    add_quad(near_data,{213,553.9995f,227},{130,0,0},{0,0,105},{0.7f,0.7f,0.7f});
    QualityScene<Sampling::mis> near(near_data); near.prepare();
    delta=point-origin;distance=std::sqrt(dot(delta,delta));
    assert(!near.visible({origin,delta*(1/distance)},distance,10,scratch));
    auto hit=Hit{10,0.25f,0.25f,4}; // floor, diffuse
    Path p; p.active=true; p.random.seed(5,6); p.result={1,2,3};
    scene.scatter(p,hit,max_depth-1,0,scratch);
    assert(p.result.x==1 && p.result.y==2 && p.result.z==3); // no extra visit
    // Emitter handling: primary always has weight one; continuation NEE is suppressed.
    QualityScene<Sampling::nee> nee(data); nee.prepare();
    QualityScene<Sampling::nee>::Scratch ns(data.triangles.size());
    hit.triangle=10;
    p={};p.active=true;p.weight={2,2,2};nee.scatter(p,hit,0,0,ns);assert(p.result.x==30 && !p.active);
    p={};p.active=true;p.weight={2,2,2};nee.scatter(p,hit,1,0,ns);assert(p.result.x==0 && !p.active);
    // Empty light list and complementary MIS weights.
    for (auto& triangle:data.triangles) triangle.emission={};
    QualityScene<Sampling::mis> dark(data);dark.prepare();
    p={};p.active=true;p.random.seed(5,6);hit.triangle=4;
    dark.scatter(p,hit,0,0,scratch);assert(p.result.x==0);
    assert(std::abs(scene.power_weight(3,2)+scene.power_weight(2,3)-1)<1e-6f);
    // A stale lane PDF must never affect camera-visible emission.
    scratch.previous_pdf[0]=1e-20f; p={};p.active=true;hit.triangle=10;
    scene.scatter(p,hit,0,0,scratch);assert(p.result.x==15);
    // Recover the light PDF from an actual continuation hit on the same connection.
    delta=point-origin;distance=std::sqrt(dot(delta,delta));Ray connection{origin,delta*(1/distance)};
    Hit emitter;EmptyScratch es;ScalarScene original(cornell_box());original.intersect(connection,emitter,es);
    assert(emitter.triangle==10 || emitter.triangle==11);
    float cl=dot(scene.triangles[emitter.triangle].normal,connection.direction*-1);
    assert(std::abs(scene.light_pdf(dot(delta,delta),cl)-scene.light_pdf(emitter.t*emitter.t*dot(connection.direction,connection.direction),cl))<1e-4f);
    // Unequal emitter areas: selection frequencies must follow their areas.
    SceneData unequal;
    unequal.triangles.emplace_back(Float3{0,1,0},Float3{2,1,0},Float3{0,1,1},Float3{},Float3{1,1,1});
    unequal.triangles.emplace_back(Float3{4,1,0},Float3{8,1,0},Float3{4,1,1},Float3{},Float3{1,1,1});
    QualityScene<Sampling::mis> us(unequal);us.prepare();int first=0;
    for(int i=0;i<100000;++i) first+=us.sample_light(random).triangle==0;
    assert(us.light_area==3 && std::abs(first/100000.0-1.0/3)<0.006);
    std::cout << "cosine moments, lengths, visibility, primary emission, finite depth, empty lights, MIS weights passed\n";
}
