CXX ?= c++
BUILD ?= release
ifeq ($(BUILD),debug)
BUILD_DIR = build/debug
OPT_FLAGS = -O0 -g
# OptiX needs constant propagation for trace payload arguments.
CUDA_OPT_FLAGS = -O1 -g
CHECK_FLAGS =
else
BUILD_DIR = build
OPT_FLAGS = -O3
CUDA_OPT_FLAGS = -O2
CHECK_FLAGS = -DNDEBUG
endif
CPPFLAGS ?= -isystem . -isystem external/stb
CXXFLAGS ?= $(OPT_FLAGS) -std=c++17 -Wall -Wextra -Wpedantic
ARCH_FLAGS ?= -march=native
UTILS_HEADERS = utils.h external/pcg/pcg32.h external/stb/stb_image_write.h
RENDER_ARGS ?=

.PHONY: all simd render render-simd clean optix-sdk
all: $(BUILD_DIR)/pathtracer
simd: $(BUILD_DIR)/pathtracer_simd

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/pathtracer: pathtracer.cpp $(UTILS_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(CHECK_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/pathtracer_simd: pathtracer_simd.cpp $(UTILS_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ARCH_FLAGS) $(CHECK_FLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

render: $(BUILD_DIR)/pathtracer
	./$(BUILD_DIR)/pathtracer $(RENDER_ARGS)

render-simd: $(BUILD_DIR)/pathtracer_simd
	./$(BUILD_DIR)/pathtracer_simd $(RENDER_ARGS)

clean:
	rm -f $(BUILD_DIR)/pathtracer $(BUILD_DIR)/pathtracer_simd
	rm -f $(BUILD_DIR)/pathtracer_optix $(BUILD_DIR)/pathtracer_optix.ptx $(BUILD_DIR)/pathtracer_optix_clang $(BUILD_DIR)/pathtracer_optix_clang.ptx

NVCC ?= nvcc
CUDA_HOST_CXX ?= g++-13
CUDA_FLAGS ?= $(CUDA_OPT_FLAGS) -std=c++17 -I. -Iexternal/stb

OPTIX_TAG ?= v9.1.0
OPTIX_INCLUDE ?= external/optix-dev/include
CLANG_CUDA_CXX ?= clang++
CLANG_CUDA_FLAGS ?= $(CUDA_OPT_FLAGS) -std=c++17 -I. -Iexternal/stb
CUDA_ARCH ?= sm_89
CUDA_LDLIBS ?= -lcudart -ldl
.PHONY: optix optix-clang
optix: $(BUILD_DIR)/pathtracer_optix
optix-clang: $(BUILD_DIR)/pathtracer_optix_clang

$(BUILD_DIR)/pathtracer_optix_clang.ptx: pathtracer_optix.cu pathtracer_optix.h $(UTILS_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | $(BUILD_DIR)
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) $(CHECK_FLAGS) --cuda-gpu-arch=$(CUDA_ARCH) -I$(OPTIX_INCLUDE) --cuda-device-only -S $< -o $@

$(BUILD_DIR)/pathtracer_optix_clang: pathtracer_optix.cpp $(UTILS_HEADERS) pathtracer_optix.h $(OPTIX_INCLUDE)/optix.h $(BUILD_DIR)/pathtracer_optix_clang.ptx Makefile | $(BUILD_DIR)
	$(CLANG_CUDA_CXX) $(CLANG_CUDA_FLAGS) $(CHECK_FLAGS) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

$(BUILD_DIR)/pathtracer_optix.ptx: pathtracer_optix.cu pathtracer_optix.h $(UTILS_HEADERS) $(OPTIX_INCLUDE)/optix.h Makefile | $(BUILD_DIR)
	$(NVCC) $(CUDA_FLAGS) $(CHECK_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) --ptx $< -o $@

$(BUILD_DIR)/pathtracer_optix: pathtracer_optix.cpp $(UTILS_HEADERS) pathtracer_optix.h $(OPTIX_INCLUDE)/optix.h $(BUILD_DIR)/pathtracer_optix.ptx Makefile | $(BUILD_DIR)
	$(NVCC) $(CUDA_FLAGS) $(CHECK_FLAGS) -ccbin $(CUDA_HOST_CXX) -I$(OPTIX_INCLUDE) $< $(CUDA_LDLIBS) -o $@

optix-sdk: $(OPTIX_INCLUDE)/optix.h

$(OPTIX_INCLUDE)/optix.h: | $(BUILD_DIR)
	git clone --depth 1 --branch $(OPTIX_TAG) https://github.com/NVIDIA/optix-dev.git $(patsubst %/include,%,$(OPTIX_INCLUDE))
