CXX ?= c++
CPPFLAGS ?= -isystem . -isystem external/stb -Iexperiments
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
ARCH_FLAGS ?= -march=native -ffp-contract=off
SIMD_FLAGS ?= -mavx2
RENDER_ARGS ?=

.PHONY: all optim simd render render-optim render-simd test benchmark clean
all: build/pathtracer
bvh: build/pathtracer_bvh
EIGEN_INCLUDE ?= external/eigen
build/pathtracer_bvh build/test_bvh build/raw_bvh build/raw_bvh_linear: CPPFLAGS += -isystem $(EIGEN_INCLUDE)
build/pathtracer build/pathtracer_o3 build/raw_scalar: utils_linalg.hpp utils_image.hpp
build/pathtracer_bvh build/test_bvh build/raw_bvh: utils_image.hpp
.PHONY: bvh test-bvh

build/pathtracer_bvh: pathtracer_bvh.cpp utils_obj.hpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_bvh: tests/test_bvh.cpp pathtracer_bvh.cpp utils_obj.hpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_bvh: tests/raw_render.cpp pathtracer_bvh.cpp utils_obj.hpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DBVH $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_bvh_linear: tests/raw_render.cpp experiments/archive/pathtracer_bvh_median.cpp utils_obj.hpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DBVH_LINEAR $< $(LDFLAGS) $(LDLIBS) -o $@

test-bvh: build/test_bvh build/raw_bvh build/raw_bvh_linear
	./build/test_bvh
	./build/raw_bvh_linear > build/bvh-linear.f32
	./build/raw_bvh > build/bvh.f32
	cmp build/bvh-linear.f32 build/bvh.f32
optim: build/pathtracer_optim
simd: build/pathtracer_simd

build:
	mkdir -p $@

build/pathtracer: pathtracer.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_simd: experiments/pathtracer_simd.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_o3: pathtracer.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_optim: experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_optim_o3: experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render: build/pathtracer
	./build/pathtracer $(RENDER_ARGS)

render-simd: build/pathtracer_simd
	./build/pathtracer_simd $(RENDER_ARGS)

render-optim: build/pathtracer_optim
	./build/pathtracer_optim $(RENDER_ARGS)

build/test_simd: tests/test_intersect.cpp experiments/pathtracer_simd.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_optim: tests/test_intersect.cpp experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_optim_o3: tests/test_intersect.cpp experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_scalar: tests/raw_render.cpp pathtracer.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_simd: tests/raw_render.cpp experiments/pathtracer_simd.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) -DSIMD $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_optim: tests/raw_render.cpp experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_optim_o3: tests/raw_render.cpp experiments/pathtracer_optim.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

test: build/test_simd build/test_optim build/test_optim_o3 build/raw_scalar build/raw_simd build/raw_optim build/raw_optim_o3
	./build/test_simd
	./build/test_optim
	./build/test_optim_o3
	./build/raw_scalar > build/scalar.f32
	./build/raw_simd > build/simd.f32
	./build/raw_optim > build/optim.f32
	./build/raw_optim_o3 > build/optim-o3.f32
	cmp build/scalar.f32 build/simd.f32
	cmp build/scalar.f32 build/optim.f32
	cmp build/scalar.f32 build/optim-o3.f32

benchmark: build/pathtracer build/pathtracer_o3 build/pathtracer_optim build/pathtracer_optim_o3 build/pathtracer_simd
	python3 benchmarks/benchmark.py $(BENCH_ARGS)

clean:
	rm -f build/cloud_viewer build/cloud_viewer.ptx build/test_atmosphere build/test_cloud_transport build/export_cloud_pbrt
	rm -f build/pathtracer_cloud build/pathtracer_cloud.ptx build/pathtracer_cloud_clang build/pathtracer_cloud_clang.ptx
	rm -f build/nvdb_viewer build/nvdb_viewer_clang
	rm -f build/pathtracer_nvdb build/pathtracer_nvdb_clang
	rm -f build/pathtracer_nvdb_optix build/pathtracer_nvdb_optix.ptx build/pathtracer_nvdb_optix_clang build/pathtracer_nvdb_optix_clang.ptx
	rm -f build/pathtracer_optix build/pathtracer_optix.ptx
	rm -f build/pathtracer_optix_clang build/pathtracer_optix_clang.ptx
	rm -f build/pathtracer_bvh build/test_bvh build/raw_bvh build/raw_bvh_linear
	rm -f $(CUDA_DESIGN_TARGETS) build/pathtracer_cuda_optix.ptx
	rm -f build/pathtracer_cuda
	rm -f build/pathtracer build/pathtracer_o3 build/pathtracer_simd build/pathtracer_optim build/pathtracer_optim_o3
	rm -f build/test_simd build/test_optim build/test_optim_o3 build/raw_scalar build/raw_simd build/raw_optim build/raw_optim_o3

# This pass uses one identical PCG stream per pixel/sample, with FMA isolated
# from the strict builds used to test exact layout/scheduling equivalence.
OMP_FLAGS ?= -fopenmp
# GCC's outer-loop unroll-and-jam adds spills in the eight-ray intersection
# loop. Override with PLAIN_FLAGS= when using a compiler without this option.
PLAIN_FLAGS ?= -fno-loop-unroll-and-jam
COMPUTE_FLAGS ?= -march=native -ffp-contract=fast -pthread $(OMP_FLAGS)
COMPUTE_HEADERS = experiments/renderer_common.h experiments/simple_kernels.h experiments/render_driver.h external/stb/stb_image_write.h

.PHONY: explore quality quality-rng quality-pgo test-quality test-explore test-rng
plain: build/pathtracer_plain

.PHONY: plain
# Triangle storage experiments use the identical STL-free computation.
# Separate filenames make comparisons reproducible when changing layouts.
TRIANGLE_VARIANTS = aos split fields soa
TRIANGLE_LAYOUT_aos = 0
TRIANGLE_LAYOUT_split = 1
TRIANGLE_LAYOUT_fields = 2
TRIANGLE_LAYOUT_soa = 3
TRIANGLE_TARGETS = $(addprefix build/pathtracer_triangles_,$(TRIANGLE_VARIANTS))
TRIANGLE_CLANG_TARGETS = $(addprefix build/pathtracer_triangles_clang_,$(TRIANGLE_VARIANTS))
TRIANGLE_STRICT_TARGETS = $(addprefix build/pathtracer_triangles_strict_,$(TRIANGLE_VARIANTS))
TRIANGLE_CLANG_STRICT_TARGETS = $(addprefix build/pathtracer_triangles_clang_strict_,$(TRIANGLE_VARIANTS))
TRIANGLE_HEADERS = experiments/triangle_storage.h external/stb/stb_image_write.h
.PHONY: triangles triangles-clang
triangles: build/pathtracer_triangles_soa
triangles-clang: build/pathtracer_triangles_clang_soa

$(TRIANGLE_TARGETS): build/pathtracer_triangles_%: experiments/pathtracer_triangles.cpp $(TRIANGLE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) $(PLAIN_FLAGS) -DTRIANGLE_LAYOUT=$(TRIANGLE_LAYOUT_$*) $< $(LDFLAGS) $(LDLIBS) -o $@

$(TRIANGLE_CLANG_TARGETS): build/pathtracer_triangles_clang_%: experiments/pathtracer_triangles.cpp $(TRIANGLE_HEADERS) Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) -DTRIANGLE_LAYOUT=$(TRIANGLE_LAYOUT_$*) $< $(LDFLAGS) $(LDLIBS) -o $@

$(TRIANGLE_STRICT_TARGETS): build/pathtracer_triangles_strict_%: experiments/pathtracer_triangles.cpp $(TRIANGLE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) $(PLAIN_FLAGS) -DTRIANGLE_LAYOUT=$(TRIANGLE_LAYOUT_$*) $< $(LDFLAGS) $(LDLIBS) -o $@

$(TRIANGLE_CLANG_STRICT_TARGETS): build/pathtracer_triangles_clang_strict_%: experiments/pathtracer_triangles.cpp $(TRIANGLE_HEADERS) Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) -DTRIANGLE_LAYOUT=$(TRIANGLE_LAYOUT_$*) $< $(LDFLAGS) $(LDLIBS) -o $@

# Keep Clang outputs separate so compiler comparisons use frozen binaries.
CLANGXX ?= clang++-20
.PHONY: clang render-clang test-clang
clang: build/pathtracer_clang build/pathtracer_plain_clang build/pathtracer_quality_clang

build/pathtracer_clang: experiments/pathtracer_clang.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_clang_strict: experiments/pathtracer_clang.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render-clang: build/pathtracer_clang
	./build/pathtracer_clang $(RENDER_ARGS)

build/raw_clang: tests/raw_clang.cpp experiments/pathtracer_clang.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_clang_strict: tests/raw_clang.cpp experiments/pathtracer_clang.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

test-clang: build/pathtracer_clang build/pathtracer_clang_strict build/raw_clang_strict build/pathtracer_plain_clang_strict
	python3 tests/validate_clang.py

build/pathtracer_plain_clang: experiments/pathtracer_plain.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_plain_clang_strict: experiments/pathtracer_plain.cpp external/stb/stb_image_write.h Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_clang: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_clang_strict: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_plain: experiments/pathtracer_plain.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=fast $(OMP_FLAGS) $(PLAIN_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_plain_strict: experiments/pathtracer_plain.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off $(OMP_FLAGS) $(PLAIN_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@
explore: build/pathtracer_explore
quality: build/pathtracer_quality
quality-rng: build/pathtracer_quality_xoroshiro_pp build/pathtracer_quality_xoroshiro_ss
quality-pgo: build/pathtracer_quality_pgo

build/pathtracer_explore: experiments/pathtracer_explore.cpp experiments/experimental_kernels.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_xoroshiro_pp: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) -DRNG_XOROSHIRO128PP $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_xoroshiro_ss: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) -DRNG_XOROSHIRO128SS $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_strict: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_quality_pgo: experiments/pathtracer_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) benchmarks/build_pgo.py Makefile | build
	python3 benchmarks/build_pgo.py --source experiments/pathtracer_quality.cpp --sampling nee --binary pathtracer_quality_pgo --cxx "$(CXX)" --flags "$(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS)"

build/test_quality: tests/test_quality.cpp experiments/quality_sampling.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_experimental: tests/test_experimental.cpp experiments/experimental_kernels.h $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

test-quality: build/test_quality
	./build/test_quality

test-explore: build/test_experimental
	./build/test_experimental

build/test_rng_pcg: tests/test_rng.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_rng_pp: tests/test_rng.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -pthread $(OMP_FLAGS) -DRNG_XOROSHIRO128PP $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_rng_ss: tests/test_rng.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -pthread $(OMP_FLAGS) -DRNG_XOROSHIRO128SS $< $(LDFLAGS) $(LDLIBS) -o $@

test-rng: build/test_rng_pcg build/test_rng_pp build/test_rng_ss
	./build/test_rng_pcg
	./build/test_rng_pp
	./build/test_rng_ss

.PHONY: simple advanced pgo render-simple render-advanced test-compute test-sincos clean-compute
simple: build/pathtracer_simple
advanced: build/pathtracer_advanced
pgo: build/pathtracer_simple_pgo

build/pathtracer_simple_pgo: experiments/pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile benchmarks/build_pgo.py | build
	python3 benchmarks/build_pgo.py --cxx "$(CXX)" --flags "$(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS)"

build/test_sincos: tests/test_sincos.cpp Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

test-sincos: build/test_sincos
	./build/test_sincos

build/pathtracer_simple: experiments/pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_simple_strict: experiments/pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_advanced: experiments/pathtracer_advanced.cpp experiments/advanced_kernels.h experiments/renderer_common.h experiments/render_driver.h external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_advanced_work: experiments/pathtracer_advanced.cpp experiments/advanced_kernels.h experiments/renderer_common.h experiments/render_driver.h external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) -DCOUNT_WORK $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_simple: tests/test_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_advanced: tests/test_advanced.cpp experiments/advanced_kernels.h experiments/renderer_common.h external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render-simple: build/pathtracer_simple
	./build/pathtracer_simple $(RENDER_ARGS)

render-advanced: build/pathtracer_advanced
	./build/pathtracer_advanced $(RENDER_ARGS)

test-compute: build/test_simple build/test_advanced build/pathtracer_simple build/pathtracer_simple_strict build/pathtracer_advanced
	./build/test_simple
	./build/test_advanced
	python3 tests/validate_render.py

clean-compute:
	rm -f build/pathtracer_clang build/pathtracer_clang_strict build/raw_clang build/raw_clang_strict
	rm -f $(TRIANGLE_TARGETS) $(TRIANGLE_CLANG_TARGETS) $(TRIANGLE_STRICT_TARGETS) $(TRIANGLE_CLANG_STRICT_TARGETS)
	rm -f build/pathtracer_simple build/pathtracer_simple_strict build/pathtracer_advanced build/pathtracer_advanced_work build/test_simple build/test_advanced
	rm -f build/pathtracer_simple_pgo build/test_sincos
	rm -f build/pathtracer_explore build/pathtracer_quality build/pathtracer_quality_strict build/pathtracer_quality_pgo build/pathtracer_quality_xoroshiro_pp build/pathtracer_quality_xoroshiro_ss
	rm -f build/test_quality build/test_experimental build/test_rng_pcg build/test_rng_pp build/test_rng_ss
	rm -f build/pathtracer_plain build/pathtracer_plain_strict
	rm -f build/pathtracer_plain_clang build/pathtracer_plain_clang_strict build/pathtracer_quality_clang build/pathtracer_quality_clang_strict

# Minimal CUDA renderer; override CUDA_HOST_CXX for your toolkit's host compiler.
NVCC ?= nvcc
CUDA_HOST_CXX ?= g++-13
CUDA_FLAGS ?= -O2 -std=c++17 -I. -Iexternal/stb
.PHONY: cuda render-cuda
cuda: build/pathtracer_cuda

build/pathtracer_cuda: experiments/pathtracer_cuda.cu external/stb/stb_image_write.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) $< -o $@

render-cuda: build/pathtracer_cuda
	./build/pathtracer_cuda $(RENDER_ARGS)

# Standalone OptiX hello triangle (no shared renderer headers).
OPTIX_TAG ?= v9.0.0
OPTIX_INCLUDE ?= external/optix-dev-$(OPTIX_TAG)/include
CLANG_CUDA_CXX ?= clang++
CLANG_CUDA_FLAGS ?= -O2 -std=c++17 -I. -Iexternal/stb
CUDA_ARCH ?= sm_89
CUDA_LDLIBS ?= -lcudart -ldl
.PHONY: optix optix-clang compdb
optix: build/pathtracer_optix
optix-clang: build/pathtracer_optix_clang

build/pathtracer_optix_clang.ptx: pathtracer_optix.cu pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) --cuda-gpu-arch=$(CUDA_ARCH) -I$(OPTIX_INCLUDE) --cuda-device-only -S $< -o $@

build/pathtracer_optix_clang: pathtracer_optix.cpp pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_optix_clang.ptx Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

# Extract the expanded build recipes; compiler flags have one source of truth.
COMPDB_TARGETS ?= all optim simd bvh optix-clang nvdb-clang nvdb_viewer cloud-clang cloud-viewer build/export_cloud_pbrt
compdb:
	$(MAKE) --no-print-directory --dry-run --always-make CXX=clang++ $(COMPDB_TARGETS) | python3 tools/compile_commands.py

build/pathtracer_optix.ptx: pathtracer_optix.cu pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) --ptx $< -o $@

build/pathtracer_optix: pathtracer_optix.cpp pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_optix.ptx Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

NANOVDB_INCLUDE ?= ../volume-assets/.cache/vdb/root/usr/include
NVDB_HEADERS = $(NANOVDB_INCLUDE)/nanovdb/NanoVDB.h $(NANOVDB_INCLUDE)/nanovdb/util/HDDA.h $(NANOVDB_INCLUDE)/nanovdb/util/Ray.h $(NANOVDB_INCLUDE)/nanovdb/util/IO.h
.PHONY: nvdb nvdb-clang nvdb-optix nvdb-optix-clang
nvdb: build/pathtracer_nvdb
nvdb-clang: build/pathtracer_nvdb_clang
nvdb-optix: build/pathtracer_nvdb_optix
nvdb-optix-clang: build/pathtracer_nvdb_optix_clang

build/pathtracer_nvdb: experiments/pathtracer_nvdb.cpp experiments/nvdb_renderer.h $(NVDB_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -isystem $(NANOVDB_INCLUDE) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_nvdb_clang: experiments/pathtracer_nvdb.cpp experiments/nvdb_renderer.h $(NVDB_HEADERS) Makefile | build
	$(CLANGXX) $(CPPFLAGS) $(CXXFLAGS) -isystem $(NANOVDB_INCLUDE) $< $(LDFLAGS) $(LDLIBS) -o $@

RAYLIB_VERSION ?= 6.2
RAYLIB_DIR ?= external/raylib-$(RAYLIB_VERSION)_linux_amd64
RAYLIB_LDLIBS ?= $(RAYLIB_DIR)/lib/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11
.PHONY: nvdb_viewer
nvdb_viewer: build/nvdb_viewer

$(RAYLIB_DIR)/lib/libraylib.a: | build
	curl -fL https://github.com/raysan5/raylib/releases/download/$(RAYLIB_VERSION)/raylib-$(RAYLIB_VERSION)_linux_amd64.tar.gz -o build/raylib.tar.gz
	mkdir -p $(RAYLIB_DIR)
	tar -xzf build/raylib.tar.gz --strip-components=1 -C $(RAYLIB_DIR)

build/nvdb_viewer: experiments/nvdb_viewer.cpp experiments/nvdb_renderer.h $(NVDB_HEADERS) $(RAYLIB_DIR)/lib/libraylib.a Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -isystem $(NANOVDB_INCLUDE) -isystem $(RAYLIB_DIR)/include $< $(LDFLAGS) $(RAYLIB_LDLIBS) $(LDLIBS) -o $@

# Pinned Dear ImGui plus the existing raylib backend. Cache the GUI objects for watch.sh.
IMGUI_VERSION ?= 1.92.7
IMGUI_DIR ?= external/imgui-$(IMGUI_VERSION)
RLIMGUI_REF ?= 1550009359ad975927f7f0e4a3f47e3f27123ea9
RLIMGUI_DIR ?= external/rlimgui-$(RLIMGUI_REF)
IMGUI_SOURCES = $(addprefix $(IMGUI_DIR)/,imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp)
IMGUI_OBJECTS = $(addprefix build/imgui/,$(notdir $(IMGUI_SOURCES:.cpp=.o))) build/rlimgui/rlImGui.o
IMGUI_LIBRARY = build/libcloud_imgui.a
IMGUI_INCLUDES = -isystem $(IMGUI_DIR) -isystem $(RLIMGUI_DIR)

$(IMGUI_SOURCES) $(IMGUI_DIR)/imgui.h &: | build
	curl -fL https://github.com/ocornut/imgui/archive/refs/tags/v$(IMGUI_VERSION).tar.gz -o build/imgui.tar.gz
	mkdir -p $(IMGUI_DIR)
	tar -xzf build/imgui.tar.gz --strip-components=1 -C $(IMGUI_DIR)

$(RLIMGUI_DIR)/rlImGui.cpp $(RLIMGUI_DIR)/rlImGui.h &: | build
	curl -fL https://github.com/raylib-extras/rlImGui/archive/$(RLIMGUI_REF).tar.gz -o build/rlimgui.tar.gz
	mkdir -p $(RLIMGUI_DIR)
	tar -xzf build/rlimgui.tar.gz --strip-components=1 -C $(RLIMGUI_DIR)

build/imgui/%.o: $(IMGUI_DIR)/%.cpp $(IMGUI_DIR)/imgui.h Makefile
	mkdir -p $(@D)
	$(CXX) -O2 -std=c++17 $(IMGUI_INCLUDES) -c $< -o $@

build/rlimgui/rlImGui.o: $(RLIMGUI_DIR)/rlImGui.cpp $(RLIMGUI_DIR)/rlImGui.h $(IMGUI_DIR)/imgui.h $(RAYLIB_DIR)/lib/libraylib.a Makefile
	mkdir -p $(@D)
	$(CXX) -O2 -std=c++17 -DNO_FONT_AWESOME $(IMGUI_INCLUDES) -isystem $(RAYLIB_DIR)/include -c $< -o $@

$(IMGUI_LIBRARY): $(IMGUI_OBJECTS)
	$(AR) rcs $@ $^

# One C++ source, compiled for OptiX device programs and the raylib host.
.PHONY: cloud-viewer test-atmosphere test-cloud-transport test-cloud-transport-gpu
cloud-viewer: build/cloud_viewer

# Reuse the installed PBRT-v4 renderer; this target only exports/runs scenes.
PBRT_DIR ?= ../volume-assets/.cache/pbrt-v4
PBRT_ARGS ?= --width 256 --spp 1024 --optix-spp 1024
.PHONY: cloud-pbrt
cloud-pbrt: build/export_cloud_pbrt build/cloud_viewer
	python3 tools/render_cloud_pbrt.py --pbrt-dir $(PBRT_DIR) $(PBRT_ARGS)

build/export_cloud_pbrt: tools/export_cloud_pbrt.cpp experiments/cloud_viewer.cpp $(NVDB_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OMP_FLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(LDFLAGS) $(LDLIBS) -o $@

test-atmosphere: build/test_atmosphere
	./build/test_atmosphere

build/test_atmosphere: tests/test_atmosphere.cpp experiments/cloud_viewer.cpp $(NVDB_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(LDFLAGS) $(LDLIBS) -o $@

test-cloud-transport: build/test_cloud_transport
	./build/test_cloud_transport

test-cloud-transport-gpu: build/cloud_viewer
	LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s '-screen 0 1024x768x24' python3 tools/validate_cloud_transport.py

build/test_cloud_transport: tests/test_cloud_transport.cpp experiments/cloud_viewer.cpp $(NVDB_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(LDFLAGS) $(LDLIBS) -o $@

build/cloud_viewer.ptx: experiments/cloud_viewer.cpp $(NVDB_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -arch=$(CUDA_ARCH) -x cu --ptx -DCLOUD_DEVICE -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< -o $@

build/cloud_viewer: experiments/cloud_viewer.cpp $(NVDB_HEADERS) $(OPTIX_INCLUDE)/optix.h $(RAYLIB_DIR)/lib/libraylib.a $(IMGUI_LIBRARY) build/cloud_viewer.ptx Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $(IMGUI_INCLUDES) -isystem $(RAYLIB_DIR)/include $< $(LDFLAGS) $(IMGUI_LIBRARY) $(RAYLIB_LDLIBS) $(CUDA_LDLIBS) $(LDLIBS) -o $@

NVDB_OPTIX_HEADERS = experiments/pathtracer_nvdb_optix.h external/cuda/helper_math.h $(NVDB_HEADERS)

build/pathtracer_nvdb_optix.ptx: experiments/pathtracer_nvdb_optix.cu $(NVDB_OPTIX_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) --ptx $< -o $@

build/pathtracer_nvdb_optix: experiments/pathtracer_nvdb_optix.cpp $(NVDB_OPTIX_HEADERS) $(OPTIX_INCLUDE)/optix.h build/pathtracer_nvdb_optix.ptx Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(CUDA_LDLIBS) -o $@

build/pathtracer_nvdb_optix_clang.ptx: experiments/pathtracer_nvdb_optix.cu $(NVDB_OPTIX_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) --cuda-gpu-arch=$(CUDA_ARCH) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) --cuda-device-only -S $< -o $@

build/pathtracer_nvdb_optix_clang: experiments/pathtracer_nvdb_optix.cpp $(NVDB_OPTIX_HEADERS) $(OPTIX_INCLUDE)/optix.h build/pathtracer_nvdb_optix_clang.ptx Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(CUDA_LDLIBS) -o $@

# Single-scattering WDAS cloud renderer; host and device sources stay separate.
CLOUD_HEADERS = experiments/pathtracer_cloud.h $(NVDB_HEADERS)
.PHONY: cloud cloud-clang
cloud: build/pathtracer_cloud
cloud-clang: build/pathtracer_cloud_clang

build/pathtracer_cloud.ptx: experiments/pathtracer_cloud.cu $(CLOUD_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) --ptx $< -o $@

build/pathtracer_cloud: experiments/pathtracer_cloud.cpp $(CLOUD_HEADERS) external/cuda/helper_math.h external/stb/stb_image_write.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_cloud.ptx Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(CUDA_LDLIBS) -o $@

build/pathtracer_cloud_clang.ptx: experiments/pathtracer_cloud.cu $(CLOUD_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) --cuda-gpu-arch=$(CUDA_ARCH) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) --cuda-device-only -S $< -o $@

build/pathtracer_cloud_clang: experiments/pathtracer_cloud.cpp $(CLOUD_HEADERS) external/cuda/helper_math.h external/stb/stb_image_write.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_cloud_clang.ptx Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) -I$(OPTIX_INCLUDE) -isystem $(NANOVDB_INCLUDE) $< $(CUDA_LDLIBS) -o $@

# Four execution designs, with a shared scene and estimator.
CUDA_DESIGNS = megakernel scheduled wavefront optix
CUDA_DESIGN_TARGETS = $(addprefix build/pathtracer_cuda_,$(CUDA_DESIGNS))
.PHONY: cuda-designs optix-sdk test-cuda-designs
cuda-designs: $(CUDA_DESIGN_TARGETS)
optix-sdk: $(OPTIX_INCLUDE)/optix.h

$(OPTIX_INCLUDE)/optix.h: | build
	git clone --depth 1 --branch $(OPTIX_TAG) https://github.com/NVIDIA/optix-dev.git $(patsubst %/include,%,$(OPTIX_INCLUDE))

build/pathtracer_cuda_megakernel build/pathtracer_cuda_scheduled build/pathtracer_cuda_wavefront: build/pathtracer_cuda_%: experiments/pathtracer_cuda_%.cu experiments/cuda_common.cuh external/stb/stb_image_write.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) $< -o $@

build/pathtracer_cuda_optix.ptx: experiments/pathtracer_cuda_optix.cu experiments/cuda_common.cuh $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) --ptx -DTINY_OPTIX_DEVICE $< -o $@

build/pathtracer_cuda_optix: experiments/pathtracer_cuda_optix.cu experiments/cuda_common.cuh external/stb/stb_image_write.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_cuda_optix.ptx Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) $< -Xlinker=-ldl -o $@

test-cuda-designs: cuda-designs
	python3 tests/validate_cuda_designs.py
