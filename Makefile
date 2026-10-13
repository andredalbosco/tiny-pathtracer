CXX ?= c++
CPPFLAGS ?= -isystem .
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
ARCH_FLAGS ?= -march=native -ffp-contract=off
SIMD_FLAGS ?= -mavx2
RENDER_ARGS ?=

.PHONY: all optim simd render render-optim render-simd test benchmark clean
all: build/pathtracer
optim: build/pathtracer_optim
simd: build/pathtracer_simd

build:
	mkdir -p $@

build/pathtracer: pathtracer.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_simd: pathtracer_simd.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_o3: pathtracer.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_optim: pathtracer_optim.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_optim_o3: pathtracer_optim.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render: build/pathtracer
	./build/pathtracer $(RENDER_ARGS)

render-simd: build/pathtracer_simd
	./build/pathtracer_simd $(RENDER_ARGS)

render-optim: build/pathtracer_optim
	./build/pathtracer_optim $(RENDER_ARGS)

build/test_simd: tests/test_intersect.cpp pathtracer_simd.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_optim: tests/test_intersect.cpp pathtracer_optim.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_optim_o3: tests/test_intersect.cpp pathtracer_optim.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_scalar: tests/raw_render.cpp pathtracer.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_simd: tests/raw_render.cpp pathtracer_simd.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) -DSIMD $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_optim: tests/raw_render.cpp pathtracer_optim.cpp stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) -DOPTIM $< $(LDFLAGS) $(LDLIBS) -o $@

build/raw_optim_o3: tests/raw_render.cpp pathtracer_optim.cpp stb_image_write.h Makefile | build
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
	python3 benchmark.py $(BENCH_ARGS)

clean:
	rm -f build/pathtracer build/pathtracer_o3 build/pathtracer_simd build/pathtracer_optim build/pathtracer_optim_o3
	rm -f build/test_simd build/test_optim build/test_optim_o3 build/raw_scalar build/raw_simd build/raw_optim build/raw_optim_o3

# This pass uses one identical PCG stream per pixel/sample, with FMA isolated
# from the strict builds used to test exact layout/scheduling equivalence.
OMP_FLAGS ?= -fopenmp
COMPUTE_FLAGS ?= -march=native -ffp-contract=fast -pthread $(OMP_FLAGS)
COMPUTE_HEADERS = renderer_common.h simple_kernels.h render_driver.h stb_image_write.h

.PHONY: simple advanced pgo render-simple render-advanced test-compute test-sincos clean-compute
simple: build/pathtracer_simple
advanced: build/pathtracer_advanced
pgo: build/pathtracer_simple_pgo

build/pathtracer_simple_pgo: pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile benchmarks/build_pgo.py | build
	python3 benchmarks/build_pgo.py --cxx "$(CXX)" --flags "$(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS)"

build/test_sincos: tests/test_sincos.cpp Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

test-sincos: build/test_sincos
	./build/test_sincos

build/pathtracer_simple: pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_simple_strict: pathtracer_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_advanced: pathtracer_advanced.cpp advanced_kernels.h renderer_common.h render_driver.h stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_advanced_work: pathtracer_advanced.cpp advanced_kernels.h renderer_common.h render_driver.h stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMPUTE_FLAGS) -DCOUNT_WORK $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_simple: tests/test_simple.cpp $(COMPUTE_HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -march=native -ffp-contract=off -pthread $(OMP_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/test_advanced: tests/test_advanced.cpp advanced_kernels.h renderer_common.h stb_image_write.h Makefile | build
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
	rm -f build/pathtracer_simple build/pathtracer_simple_strict build/pathtracer_advanced build/pathtracer_advanced_work build/test_simple build/test_advanced
	rm -f build/pathtracer_simple_pgo build/test_sincos
