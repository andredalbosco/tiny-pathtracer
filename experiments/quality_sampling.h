#pragma once
// Include after experiments/render_driver.h: this changes the estimator, not intersection.
enum class Sampling { cosine, nee, mis };

template<Sampling Mode> struct QualityScene : RayScene {
    struct Scratch : EmptyScratch {
        std::array<float, 8> previous_pdf{};
        std::uint64_t shadow_rays = 0;
        explicit Scratch(std::size_t n) : EmptyScratch(n) {}
    };
    struct Basis { Float3 tangent, bitangent; };
    struct Light { int triangle; float cumulative_area; };
    struct LightSample { int triangle; Float3 point; };
    std::vector<Basis> bases;
    std::vector<Light> lights;
    float light_area = 0;
    bool any_hit = true;
    mutable std::atomic<std::uint64_t> shadow_rays{0};

    explicit QualityScene(SceneData data) : RayScene(std::move(data)) {}
    static const char* sampling_name() {
        if constexpr (Mode == Sampling::cosine) return "cosine";
        if constexpr (Mode == Sampling::nee) return "nee";
        return "mis";
    }
    void prepare() {
        RayScene::prepare();
        bases.clear(); lights.clear(); light_area = 0;
        for (int i = 0; i < int(triangles.size()); ++i) {
            const auto& t = triangles[i];
            Float3 axis = std::abs(t.normal.z) < 0.9f ? Float3{0,0,1} : Float3{0,1,0};
            Float3 tangent = normalize(cross(axis, t.normal));
            bases.push_back({tangent, cross(t.normal, tangent)});
            float area = 0.5f * std::sqrt(dot(cross(t.e1,t.e2), cross(t.e1,t.e2)));
            if (area > 0 && dot(t.emission,t.emission) > 0) {
                light_area += area;
                lights.push_back({i, light_area});
            }
        }
    }
    Float3 cosine_direction(int id, Random& random) const {
        float u = random.uniform(), phi = 2 * M_PI * random.uniform();
        float radius = std::sqrt(u), sine, cosine;
#if defined(__GLIBC__)
        __builtin_sincosf(phi, &sine, &cosine);
#else
        sine = std::sin(phi); cosine = std::cos(phi);
#endif
        return bases[id].tangent * (radius*cosine) + bases[id].bitangent * (radius*sine)
             + triangles[id].normal * std::sqrt(1-u);
    }
    LightSample sample_light(Random& random) const {
        float selector=random.uniform()*light_area;
        int id=lights.back().triangle;
        for (const auto& light:lights) if (selector<light.cumulative_area) { id=light.triangle; break; }
        const auto& light=triangles[id];
        float root=std::sqrt(random.uniform()), v=random.uniform();
        return {id,light.p+light.e1*(1-root)+light.e2*(root*v)};
    }
    float light_pdf(float distance2, float cosine) const {
        return cosine>0 && light_area>0 ? distance2/(light_area*cosine) : INFINITY;
    }
    // A straight connection meets the sampled light's plane only at its
    // endpoint. Exclude that emitter and its coplanar emissive seam neighbors;
    // keep the full finite segment for every other potential blocker.
    bool visible(Ray ray, float distance, int light_id, Scratch& scratch) const {
        ++scratch.shadow_rays;
        Hit hit;
        hit.t = distance;
        bool blocked = false;
        const auto& light = triangles[light_id];
        for (int i=0; i<int(triangles.size()); ++i) {
            const auto& triangle=triangles[i];
            if (i==light_id || (dot(triangle.emission,triangle.emission)>0
                && triangle.normal.x==light.normal.x && triangle.normal.y==light.normal.y
                && triangle.normal.z==light.normal.z
                && dot(light.normal,triangle.p-light.p)==0)) continue;
            if (triangle.intersect(ray, hit)) {
                if (any_hit) return false;
                blocked = true;
            }
        }
        return !blocked;
    }
    static float power_weight(float a, float b) {
        // Ratios avoid squaring large PDFs. This scene has finite, positive PDFs.
        if (!(a > 0)) return 0;
        float ratio = b/a;
        return 1 / (1 + ratio*ratio);
    }
    void scatter(Path& path, Hit hit, int depth, int lane, Scratch& scratch) const {
        if (hit.triangle < 0) { path.active = false; return; }
        const Triangle& triangle = triangles[hit.triangle];
        if (dot(triangle.emission, triangle.emission) > 0) {
            float weight = 1;
            if constexpr (Mode == Sampling::nee) weight = depth == 0 ? 1 : 0;
            if constexpr (Mode == Sampling::mis) {
                if (depth > 0) {
                    float cosine = std::max(0.0f, dot(triangle.normal, path.ray.direction * -1));
                    float distance2 = hit.t*hit.t*dot(path.ray.direction,path.ray.direction);
                    weight = power_weight(scratch.previous_pdf[lane], light_pdf(distance2,cosine));
                }
            }
            path.result += path.weight * triangle.emission * weight;
            path.active = false;
            return;
        }
        Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
        Float3 origin = position + triangle.normal * 1e-4f;
        if constexpr (Mode != Sampling::cosine) {
            // A sampled light consumes the next surface visit. At the last
            // diffuse visit there is no remaining visit in the original target.
            if (depth + 1 < max_depth && light_area > 0) {
                auto sampled=sample_light(path.random);
                int id=sampled.triangle;
                const auto& light = triangles[id];
                Float3 delta = sampled.point-origin;
                float distance2 = dot(delta,delta), distance = std::sqrt(distance2);
                if (distance > 0) {
                    Float3 direction = delta*(1/distance);
                    float cos_x = std::max(0.0f, dot(triangle.normal,direction));
                    float cos_l = std::max(0.0f, dot(light.normal,direction*-1));
                    if (cos_x > 0 && cos_l > 0 && visible({origin,direction},distance,id,scratch)) {
                        float pdf = light_pdf(distance2,cos_l);
                        float weight = 1;
                        if constexpr (Mode == Sampling::mis) weight=power_weight(pdf,cos_x/float(M_PI));
                        path.result += path.weight * triangle.albedo * light.emission
                                     * (cos_x * weight / (float(M_PI)*pdf));
                    }
                }
            }
        }
        Float3 direction = cosine_direction(hit.triangle,path.random);
        if constexpr (Mode == Sampling::mis)
            scratch.previous_pdf[lane] = std::max(0.0f,dot(triangle.normal,direction))/float(M_PI);
        path.weight *= triangle.albedo;
        path.ray = {origin,direction};
    }
    void finish_worker(const Scratch& scratch) const {
        shadow_rays.fetch_add(scratch.shadow_rays, std::memory_order_relaxed);
    }
};
