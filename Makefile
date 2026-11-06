CXX ?= c++
CPPFLAGS ?= -isystem . -isystem external/stb
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
ARCH_FLAGS ?= -march=native -ffp-contract=off
SIMD_FLAGS ?= -mavx2
RENDER_ARGS ?=

.PHONY: all simd render render-simd clean optix-sdk
all: build/pathtracer
simd: build/pathtracer_simd

build:
	mkdir -p $@

build/pathtracer: pathtracer.cpp utils_linalg.hpp utils_image.hpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

build/pathtracer_simd: pathtracer_simd.cpp external/stb/stb_image_write.h Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(SIMD_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render: build/pathtracer
	./build/pathtracer $(RENDER_ARGS)

render-simd: build/pathtracer_simd
	./build/pathtracer_simd $(RENDER_ARGS)

clean:
	rm -f build/pathtracer build/pathtracer_simd
	rm -f build/pathtracer_optix build/pathtracer_optix.ptx build/pathtracer_optix_clang build/pathtracer_optix_clang.ptx

NVCC ?= nvcc
CUDA_HOST_CXX ?= g++-13
CUDA_FLAGS ?= -O2 -std=c++17 -I. -Iexternal/stb

OPTIX_TAG ?= v9.1.0
OPTIX_INCLUDE ?= external/optix-dev-$(OPTIX_TAG)/include
CLANG_CUDA_CXX ?= clang++
CLANG_CUDA_FLAGS ?= -O2 -std=c++17 -I. -Iexternal/stb
CUDA_ARCH ?= sm_89
CUDA_LDLIBS ?= -lcudart -ldl
.PHONY: optix optix-clang
optix: build/pathtracer_optix
optix-clang: build/pathtracer_optix_clang

build/pathtracer_optix_clang.ptx: pathtracer_optix.cu pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) --cuda-gpu-arch=$(CUDA_ARCH) -I$(OPTIX_INCLUDE) --cuda-device-only -S $< -o $@

build/pathtracer_optix_clang: pathtracer_optix.cpp pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_optix_clang.ptx Makefile | build
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

build/pathtracer_optix.ptx: pathtracer_optix.cu pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) --ptx $< -o $@

build/pathtracer_optix: pathtracer_optix.cpp pathtracer_optix.h external/cuda/helper_math.h $(OPTIX_INCLUDE)/optix.h build/pathtracer_optix.ptx Makefile | build
	$(NVCC) $(CUDA_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

optix-sdk: $(OPTIX_INCLUDE)/optix.h

$(OPTIX_INCLUDE)/optix.h: | build
	git clone --depth 1 --branch $(OPTIX_TAG) https://github.com/NVIDIA/optix-dev.git $(patsubst %/include,%,$(OPTIX_INCLUDE))
