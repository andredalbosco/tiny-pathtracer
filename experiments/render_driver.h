#pragma once
#include "renderer_common.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#ifdef _OPENMP
#include <omp.h>
#endif
#include <thread>
#include <type_traits>
#ifdef __linux__
#include <sched.h>
#endif

// PCG XSH RR, adapted from M. E. O'Neill's Apache-2.0 minimal C implementation.
// See licenses/PCG-NOTICE and licenses/Apache-2.0.txt.
struct Random {
    std::uint64_t state = 0, increment = 1;
    static constexpr const char* name() {
#if defined(RNG_XOROSHIRO128PP)
        return "xoroshiro128++";
#elif defined(RNG_XOROSHIRO128SS)
        return "xoroshiro128**";
#else
        return "pcg32";
#endif
    }
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
    // Blackman/Vigna xoroshiro128++ 1.0 and SplitMix64; public domain.
    // increment is the second state word in this optional build, retaining
    // the existing 16-byte AoS/SoA storage interface. See licenses/XOROSHIRO-NOTICE.
    static std::uint64_t rotate(std::uint64_t x, int k) { return (x<<k)|(x>>(64-k)); }
    static std::uint64_t mix(std::uint64_t x) {
        x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
        x=(x^(x>>27))*0x94d049bb133111ebULL;
        return x^(x>>31);
    }
    static std::uint64_t seed_key(std::uint64_t seed) { return mix(seed^0xd1b54a32d192ed03ULL); }
#else
    static std::uint64_t seed_key(std::uint64_t seed) { return seed; }
#endif
    std::uint32_t next() {
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
        auto first=state, second=increment;
#ifdef RNG_XOROSHIRO128PP
        auto result=rotate(first+second,17)+first;
        second^=first;
        state=rotate(first,49)^second^(second<<21);
        increment=rotate(second,28);
#else
        auto result=rotate(first*5,7)*9;
        second^=first;
        state=rotate(first,24)^second^(second<<16);
        increment=rotate(second,37);
#endif
        return std::uint32_t(result>>32);
#else
        auto previous = state;
        state = previous * 6364136223846793005ULL + increment;
        auto bits = std::uint32_t(((previous >> 18) ^ previous) >> 27);
        auto rotation = std::uint32_t(previous >> 59);
        return (bits >> rotation) | (bits << ((-rotation) & 31));
#endif
    }
    void seed(std::uint64_t value, std::uint64_t stream) {
#if defined(RNG_XOROSHIRO128PP) || defined(RNG_XOROSHIRO128SS)
        auto key=value^stream;
        state=mix(key+0x9e3779b97f4a7c15ULL);
        increment=mix(key+2*0x9e3779b97f4a7c15ULL);
        // Consecutive SplitMix inputs are distinct; their pair cannot be zero.
#else
        state = 0; increment = (stream << 1) | 1;
        next(); state += value; next();
#endif
    }
    float uniform() { return float(next() >> 8) * 0x1p-24f; }
};

Float3 sample_hemisphere(Float3 normal, Random& random) {
    float z = 2 * random.uniform() - 1;
    float phi = 2 * M_PI * random.uniform();
    float r = std::sqrt(std::max(0.0f, 1 - z * z));
#if defined(__GLIBC__)
    float sine, cosine;
    __builtin_sincosf(phi, &sine, &cosine);
    Float3 direction{r * cosine, r * sine, z};
#else
    Float3 direction{r * std::cos(phi), r * std::sin(phi), z};
#endif
    return dot(direction, normal) < 0 ? direction * -1 : direction;
}

struct Path {
    Ray ray;
    Float3 weight{1, 1, 1}, result{};
    Random random;
    bool active = false;
};

// Same estimator as pathtracer.cpp: collect emissive hits and reflect a
// uniform sphere sample into the hemisphere, with Lambertian weight 2*cosine.
void scatter(const SceneData& scene, Path& path, Hit hit) {
    if (hit.triangle < 0) { path.active = false; return; }
    const Triangle& triangle = scene.triangles[hit.triangle];
    if (dot(triangle.emission, triangle.emission) > 0) {
        path.result = path.weight * triangle.emission;
        path.active = false;
        return;
    }
    Float3 position = triangle.p + triangle.e1 * hit.u + triangle.e2 * hit.v;
    Float3 direction = sample_hemisphere(triangle.normal, path.random);
    path.weight *= triangle.albedo * (2 * std::max(0.0f, dot(triangle.normal, direction)));
    path.ray = {position + triangle.normal * 1e-4f, direction};
}

template<int B> struct PathsAoS {
    std::array<Path, B> paths;
    Path get(int i) const { return paths[i]; }
    void set(int i, const Path& path) { paths[i] = path; }
};

template<int B> struct Float3Block {
    std::array<float, B> x{}, y{}, z{};
    Float3 get(int i) const { return {x[i], y[i], z[i]}; }
    void set(int i, Float3 v) { x[i] = v.x; y[i] = v.y; z[i] = v.z; }
};

template<int B> struct PathsSoA {
    Float3Block<B> origin, direction, weight, result;
    std::array<std::uint64_t, B> state{}, increment{};
    std::array<bool, B> active{};
    Path get(int i) const {
        return {{origin.get(i), direction.get(i)}, weight.get(i), result.get(i),
                {state[i], increment[i]}, active[i]};
    }
    void set(int i, const Path& p) {
        origin.set(i, p.ray.origin); direction.set(i, p.ray.direction);
        weight.set(i, p.weight); result.set(i, p.result);
        state[i] = p.random.state; increment[i] = p.random.increment; active[i] = p.active;
    }
};

struct Options {
    std::uint64_t seed = 42;
    std::uint64_t random_seed = Random::seed_key(42);
    int samples = samples_per_pixel, width = image_width, threads = 1, batch = 1;
    std::string filename = "build/cornell_box.hdr", kernel = "flat", layout = "aos", runtime = "std", worker_report;
    std::vector<int> cpus;
    bool raw = false, refill = false, any_hit = true;
};

Options parse_options(int argc, char** argv, std::string default_kernel) {
    Options options;
    options.kernel = default_kernel;
    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("Missing value for " + arg);
            return argv[i];
        };
        if (arg == "--threads") options.threads = std::stoi(value());
        else if (arg == "--seed") options.seed = std::stoull(value());
        else if (arg == "--visibility") {
            std::string mode=value();
            if (mode != "any" && mode != "nearest") throw std::runtime_error("Visibility must be any or nearest");
            options.any_hit=mode=="any";
        }
        else if (arg == "--runtime") options.runtime = value();
        else if (arg == "--worker-report") options.worker_report = value();
        else if (arg == "--batch") options.batch = std::stoi(value());
        else if (arg == "--kernel") options.kernel = value();
        else if (arg == "--layout") options.layout = value();
        else if (arg == "--raw") options.raw = true;
        else if (arg == "--refill") options.refill = true;
        else if (arg == "--cpus") {
            std::stringstream list(value());
            for (std::string cpu; std::getline(list, cpu, ',');) options.cpus.push_back(std::stoi(cpu));
        } else if (arg.rfind("--", 0) == 0) throw std::runtime_error("Unknown option " + arg);
        else if (positional == 0) { options.samples = std::stoi(arg); ++positional; }
        else if (positional == 1) { options.width = std::stoi(arg); ++positional; }
        else if (positional == 2) { options.filename = arg; ++positional; }
        else throw std::runtime_error("Too many arguments");
    }
    if (options.samples < 1 || options.width < 1 || options.threads < 1 || options.threads > 1024)
        throw std::runtime_error("Samples, width and worker count must be positive (workers <= 1024)");
    if (options.batch != 1 && options.batch != 2 && options.batch != 4 && options.batch != 8)
        throw std::runtime_error("Batch must be 1, 2, 4 or 8");
    if (options.runtime != "std" && options.runtime != "omp") throw std::runtime_error("Runtime must be std or omp");
#ifndef _OPENMP
    if (options.runtime == "omp") throw std::runtime_error("Build with -fopenmp to use OpenMP");
#endif
    if (options.layout != "aos" && options.layout != "soa") throw std::runtime_error("Layout must be aos or soa");
    if (!options.cpus.empty() && int(options.cpus.size()) != options.threads)
        throw std::runtime_error("Provide one CPU for each worker");
    for (int cpu : options.cpus) if (cpu < 0 || cpu >= 1024) throw std::runtime_error("Invalid CPU");
    options.random_seed=Random::seed_key(options.seed);
    return options;
}

template<class Scene, class = void> struct HasBatch : std::false_type {};
template<class Scene> struct HasBatch<Scene, std::void_t<decltype(
    std::declval<const Scene&>().intersect_batch((const Ray*)nullptr, (Hit*)nullptr,
                                               1, std::declval<typename Scene::Scratch&>()))>> : std::true_type {};

template<class Scene, class = void> struct HasSampling : std::false_type {};
template<class Scene> struct HasSampling<Scene, std::void_t<decltype(
    std::declval<const Scene&>().scatter(std::declval<Path&>(), Hit{}, 0, 0,
                                       std::declval<typename Scene::Scratch&>()))>> : std::true_type {};

template<class Scene>
void scatter_path(const Scene& scene, Path& path, Hit hit, int depth, int lane,
                  typename Scene::Scratch& scratch) {
    if constexpr (HasSampling<Scene>::value) scene.scatter(path, hit, depth, lane, scratch);
    else scatter(scene, path, hit);
}

struct alignas(64) Statistics {
    std::uint64_t rays = 0, slots = 0, batches = 0, tiles = 0;
    double started = 0, finished = 0, cpu_seconds = 0;
    int actual_cpu = -1;
#ifdef COUNT_WORK
    std::uint64_t triangle_tests = 0, box_tests = 0, fallback_queries = 0, vector_divides = 0;
#endif
};

// A pixel owns its sample sum. Scheduling never changes the RNG stream or sum order.
template<class Scene, int B, template<int> class Storage>
void render_pixel(const Scene& scene, const Options& options, int x, int y,
                  float* pixels, typename Scene::Scratch& scratch, Statistics& stats) {
    Storage<B> paths;
    const Camera& camera = scene.camera;
    float focal = 1 / std::tan(camera.fovy * 0.5f * M_PI / 180);
    Float3 color{};
    for (int first = 0; first < options.samples; first += B) {
        int count = std::min(B, options.samples - first);
        for (int r = 0; r < B; ++r) {
            Path p;
            p.active = r < count;
            if (p.active) {
                auto pixel = std::uint64_t(y) * options.width + x;
                p.random.seed(options.random_seed, (pixel << 32) | std::uint32_t(first + r));
                Float2 uv = (Float2{float(x), float(y)} + Float2{p.random.uniform(), p.random.uniform()} * 0.5f)
                          * Float2{1.0f / options.width, 1.0f / options.width};
                p.ray = {camera.position, normalize(camera.u * (2 * uv.x - 1) * camera.aspect
                         + camera.v * (-2 * uv.y + 1) + camera.w * focal)};
            }
            paths.set(r, p);
        }
        for (int depth = 0; depth < max_depth; ++depth) {
            std::array<Ray, B> rays{};
            std::array<Hit, B> hits{};
            int live = 0;
            for (int r = 0; r < count; ++r) {
                Path p = paths.get(r);
                if (p.active) { rays[r] = p.ray; ++live; }
            }
            if (!live) break;
            stats.rays += live;
            if constexpr (HasBatch<Scene>::value) {
                scene.intersect_batch(rays.data(), hits.data(), count, scratch);
                stats.slots += count; ++stats.batches;
            } else {
                for (int r = 0; r < count; ++r) {
                    if (paths.get(r).active) scene.intersect(rays[r], hits[r], scratch);
                }
                stats.slots += live;
            }
            for (int r = 0; r < count; ++r) {
                Path p = paths.get(r);
                if (p.active) { scatter_path(scene, p, hits[r], depth, r, scratch); paths.set(r, p); }
            }
        }
        for (int r = 0; r < count; ++r) color += paths.get(r).result;
    }
    color = color * (1.0f / options.samples);
    std::memcpy(pixels + 3 * (std::size_t(y) * options.width + x), &color, sizeof(color));
}

// Keep independent paths in flight. Results are stored by sample ID, then summed
// in the original order; completion order cannot change the image.
template<class Scene, int B, template<int> class Storage>
void render_tile_refill(const Scene& scene, const Options& options, int x0, int y0,
                        int x1, int y1, float* pixels, std::vector<Float3>& results,
                        typename Scene::Scratch& scratch, Statistics& stats) {
    int tile_width = x1 - x0;
    int jobs = tile_width * (y1 - y0) * options.samples;
    Storage<B> paths;
    std::array<int, B> job{}, depth{};
    int next_job = 0;
    int sample = 0, x = x0, y = y0;
    const Camera& camera = scene.camera;
    float focal = 1 / std::tan(camera.fovy * 0.5f * M_PI / 180);
    auto refill = [&](int r) {
        Path p;
        p.active = next_job < jobs;
        if (p.active) {
            job[r] = next_job++;
            depth[r] = 0;
            p.random.seed(options.random_seed, ((std::uint64_t(y) * options.width + x) << 32)
                                        | std::uint32_t(sample));
            Float2 uv = (Float2{float(x), float(y)} + Float2{p.random.uniform(), p.random.uniform()} * 0.5f)
                      * Float2{1.0f / options.width, 1.0f / options.width};
            p.ray = {camera.position, normalize(camera.u * (2 * uv.x - 1) * camera.aspect
                     + camera.v * (-2 * uv.y + 1) + camera.w * focal)};
            if (++sample == options.samples) {
                sample = 0;
                if (++x == x1) { x = x0; ++y; }
            }
        }
        paths.set(r, p);
    };
    for (int r = 0; r < B; ++r) refill(r);
    for (;;) {
        std::array<Ray, B> rays{};
        std::array<Hit, B> hits{};
        int live = 0;
        for (int r = 0; r < B; ++r) {
            Path p = paths.get(r);
            if (p.active) { rays[r] = p.ray; ++live; }
        }
        if (!live) break;
        stats.rays += live;
        if constexpr (HasBatch<Scene>::value) {
            scene.intersect_batch(rays.data(), hits.data(), B, scratch);
            stats.slots += B; ++stats.batches;
        } else {
            for (int r = 0; r < B; ++r) if (paths.get(r).active) scene.intersect(rays[r], hits[r], scratch);
            stats.slots += live;
        }
        for (int r = 0; r < B; ++r) {
            Path p = paths.get(r);
            if (!p.active) continue;
            scatter_path(scene, p, hits[r], depth[r], r, scratch);
            if (++depth[r] == max_depth) p.active = false;
            if (!p.active) { results[job[r]] = p.result; refill(r); }
            else paths.set(r, p);
        }
    }
    for (int pixel = 0; pixel < jobs / options.samples; ++pixel) {
        Float3 color{};
        for (int sample = 0; sample < options.samples; ++sample) color += results[pixel * options.samples + sample];
        color = color * (1.0f / options.samples);
        int x = x0 + pixel % tile_width, y = y0 + pixel / tile_width;
        std::memcpy(pixels + 3 * (std::size_t(y) * options.width + x), &color, sizeof(color));
    }
}

template<class Scene, int B, template<int> class Storage>
Statistics render_image(const Scene& scene, const Options& options, float* pixels) {
    constexpr int tile_width = 16, tile_height = 8;
    int columns = (options.width + tile_width - 1) / tile_width;
    int rows = (options.width + tile_height - 1) / tile_height;
    std::atomic<int> next{0};
    std::atomic<bool> affinity_failed{false};
    std::vector<Statistics> statistics(options.threads);
    auto launch = std::chrono::steady_clock::now();
    bool report = !options.worker_report.empty();
    auto worker = [&](int id) {
        timespec cpu_start{};
        if (report) {
            statistics[id].started = std::chrono::duration<double>(std::chrono::steady_clock::now() - launch).count();
#ifdef __linux__
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_start);
#endif
        }
#ifdef __linux__
        if (!options.cpus.empty()) {
            cpu_set_t set;
            CPU_ZERO(&set); CPU_SET(options.cpus[id], &set);
            if (sched_setaffinity(0, sizeof(set), &set)) affinity_failed = true;
        }
#endif
        typename Scene::Scratch scratch(scene.triangles.size());
        std::vector<Float3> results(options.refill ? std::size_t(std::min(tile_width, options.width)) * std::min(tile_height, options.width) * options.samples : 0);
        for (;;) {
            int tile = next.fetch_add(1, std::memory_order_relaxed);
            if (tile >= columns * rows) break;
            if (report) ++statistics[id].tiles;
            int x0 = tile % columns * tile_width, y0 = tile / columns * tile_height;
            int x1 = std::min(x0 + tile_width, options.width), y1 = std::min(y0 + tile_height, options.width);
            if (options.refill) render_tile_refill<Scene, B, Storage>(scene, options, x0, y0, x1, y1, pixels, results, scratch, statistics[id]);
            else for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x)
                render_pixel<Scene, B, Storage>(scene, options, x, y, pixels, scratch, statistics[id]);
        }
        if constexpr (HasSampling<Scene>::value) scene.finish_worker(scratch);
        if (report) {
            statistics[id].finished = std::chrono::duration<double>(std::chrono::steady_clock::now() - launch).count();
#ifdef __linux__
            timespec cpu_end{};
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu_end);
            statistics[id].cpu_seconds = (cpu_end.tv_sec - cpu_start.tv_sec) + (cpu_end.tv_nsec - cpu_start.tv_nsec) * 1e-9;
            statistics[id].actual_cpu = sched_getcpu();
#endif
        }
#ifdef COUNT_WORK
        statistics[id].triangle_tests = scratch.triangle_tests;
        statistics[id].box_tests = scratch.box_tests;
        statistics[id].fallback_queries = scratch.fallback_queries;
        statistics[id].vector_divides = scratch.vector_divides;
#endif
    };
    if (options.runtime == "omp") {
#ifdef _OPENMP
        omp_set_dynamic(0);
        std::atomic<bool> wrong_team{false};
#pragma omp parallel num_threads(options.threads)
        {
            if (omp_get_num_threads() != options.threads) wrong_team = true;
            worker(omp_get_thread_num());
        }
        if (wrong_team) throw std::runtime_error("OpenMP created fewer workers than requested");
#endif
    } else if (options.threads == 1) worker(0);
    else {
        std::vector<std::thread> workers;
        for (int i = 0; i < options.threads; ++i) workers.emplace_back(worker, i);
        for (auto& thread : workers) thread.join();
    }
    if (affinity_failed) throw std::runtime_error("CPU affinity failed");
    if (report) {
        std::ofstream output(options.worker_report);
        if (!output) throw std::runtime_error("Cannot write worker report");
        output << std::setprecision(9) << "[";
        for (int id = 0; id < options.threads; ++id) {
            const auto& r = statistics[id];
            output << (id ? "," : "") << "{\"worker\":" << id << ",\"cpu\":" << r.actual_cpu
                   << ",\"tiles\":" << r.tiles << ",\"rays\":" << r.rays
                   << ",\"started_seconds\":" << r.started << ",\"finished_seconds\":" << r.finished
                   << ",\"thread_cpu_seconds\":" << r.cpu_seconds << "}";
        }
        output << "]\n";
        if (!output) throw std::runtime_error("Cannot write worker report");
    }
    Statistics total;
    for (auto stats : statistics) {
        total.rays += stats.rays; total.slots += stats.slots; total.batches += stats.batches;
#ifdef COUNT_WORK
        total.triangle_tests += stats.triangle_tests; total.box_tests += stats.box_tests;
        total.fallback_queries += stats.fallback_queries; total.vector_divides += stats.vector_divides;
#endif
    }
    return total;
}

template<class Scene, template<int> class Storage>
Statistics dispatch_batch(const Scene& scene, const Options& options, float* pixels) {
    switch (options.batch) {
    case 1: return render_image<Scene, 1, Storage>(scene, options, pixels);
    case 2: return render_image<Scene, 2, Storage>(scene, options, pixels);
    case 4: return render_image<Scene, 4, Storage>(scene, options, pixels);
    default: return render_image<Scene, 8, Storage>(scene, options, pixels);
    }
}

template<class Scene> int render_main(const Options& options) {
    Scene scene(cornell_box());
    scene.prepare();
    if constexpr (HasSampling<Scene>::value) scene.any_hit=options.any_hit;
    std::vector<float> pixels(std::size_t(options.width) * options.width * 3);
    auto start = std::chrono::steady_clock::now();
    auto stats = options.layout == "aos" ? dispatch_batch<Scene, PathsAoS>(scene, options, pixels.data())
                                         : dispatch_batch<Scene, PathsSoA>(scene, options, pixels.data());
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (options.raw) {
        FILE* file = std::fopen(options.filename.c_str(), "wb");
        if (!file) throw std::runtime_error("Cannot write " + options.filename);
        auto written = std::fwrite(pixels.data(), sizeof(float), pixels.size(), file);
        auto closed = std::fclose(file);
        if (written != pixels.size() || closed) throw std::runtime_error("Cannot write " + options.filename);
    } else if (!stbi_write_hdr(options.filename.c_str(), options.width, options.width, 3, pixels.data()))
        throw std::runtime_error("Cannot write " + options.filename);
    auto completed_paths = std::uint64_t(options.width) * options.width * options.samples;
    std::cerr << std::setprecision(9) << "{\"rng\":\"" << Random::name() << "\",\"seed\":" << options.seed
              << ",\"kernel\":\"" << options.kernel << "\",\"layout\":\"" << options.layout
              << "\",\"runtime\":\"" << options.runtime << "\",\"threads\":" << options.threads << ",\"batch\":" << options.batch
              << ",\"refill\":" << (options.refill ? "true" : "false") << ",\"render_seconds\":" << seconds << ",\"rays\":" << stats.rays
              << ",\"completed_paths\":" << completed_paths << ",\"paths_per_second\":" << completed_paths / seconds
              << ",\"ray_segments_per_second\":" << stats.rays / seconds
              << ",\"ray_slots\":" << stats.slots << ",\"batch_calls\":" << stats.batches;
#ifdef COUNT_WORK
    std::cerr << ",\"triangle_tests\":" << stats.triangle_tests << ",\"box_tests\":" << stats.box_tests
              << ",\"fallback_queries\":" << stats.fallback_queries << ",\"vector_divides\":" << stats.vector_divides;
#endif
    if constexpr (HasSampling<Scene>::value)
        std::cerr << ",\"sampling\":\"" << Scene::sampling_name() << "\",\"shadow_rays\":" << scene.shadow_rays.load()
                  << ",\"visibility\":\"" << (options.any_hit ? "any" : "nearest") << "\"";
    std::cerr << "}\n";
    return 0;
}
