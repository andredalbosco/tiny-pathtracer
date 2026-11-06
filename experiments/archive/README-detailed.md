# Tiny path tracer

`pathtracer_bvh.cpp` is a minimal CPU BVH example built on `pathtracer.cpp`.
It renders Ajax against a black background using Nori's `scenes/pa3/ajax-simple.xml`
camera (30 degrees), diffuse albedo (0.5), and point light. One shadow ray tests
visibility to the light through the BVH. The mesh keeps its original coordinates;
the camera shows its front without rotating the geometry.

- `Bounds::intersect`: clip a ray against the three pairs of box faces.
- `Scene::build_node`: enclose the triangles, sort their centers along the box's
  longest axis, and split at the median. Stop at four triangles per leaf.
- `Scene::intersect_node`: skip a subtree when its box is missed; otherwise test
  the leaf's triangles or visit both children. The closest hit shortens the ray
  interval used by subsequent box tests.

Triangle IDs stay in input order, including nearest-hit ties. Rebuild with
`scene.build_bvh()` after geometry changes. This teaching version uses a full
sort at each split and always visits the left child first.

```sh
git submodule update --init external/eigen
make bvh test-bvh
./build/pathtracer_bvh
./build/pathtracer_bvh 32 768 build/ajax.hdr /path/to/ajax.obj
```

The BVH renderer uses Eigen's `Vector2f`, `Vector3f`, `.dot()`, `.cross()`,
`.normalized()`, and `.cwiseProduct()` directly. Eigen 3.4.0 is pinned as the
`external/eigen` submodule; `EIGEN_INCLUDE` can override its include directory.
The original scalar, CUDA, and OptiX renderers keep their existing math types.
`make test-bvh` checks exact BVH/linear intersections and exact rendered pixels
against a single-leaf reference using the same Eigen math. Floating-point
reduction order can differ from the original scalar renderer.

`utils_obj.hpp` keeps OBJ parsing separate from the BVH code. The default
mesh is `../nori/scenes/pa2/ajax.obj`; an optional OBJ overrides it. Defaults are
32 samples per pixel, 768×768, and `build/ajax.hdr`. It reads
positions and face indices (including slash and negative indices), fan-triangulates
polygons, and uses flat diffuse triangles; materials, UVs and vertex normals are
ignored. Nori uses interpolated vertex normals, so this flat-normal example
is not a pixel-exact reproduction of Nori. The Cornell scene is retained as a
regression fixture for comparison with the original scalar path tracer.

The previous version with direction-ordered traversal is saved in
`experiments/archive/pathtracer_bvh_obj.cpp`, with its tests in `experiments/archive/test_bvh_obj.cpp`.

`experiments/pathtracer_cuda.cu` is a minimal CUDA version of the original renderer.
One thread handles one pixel, looping over samples, up to 24 surface visits,
and all 36 triangles. The host constructs the Cornell box, copies it to the GPU,
launches one kernel, and copies the RGB floats back for HDR output.
It uses uniform hemisphere sampling and a PCG stream per pixel/sample.

```sh
make cuda
./build/pathtracer_cuda 128 320 build/cuda.hdr
make render-cuda RENDER_ARGS="128 320 build/cuda.hdr"
```

Arguments are samples, width and output filename; defaults are 128, 320 and
`build/cuda.hdr`. Requires an NVIDIA GPU and CUDA toolkit. `NVCC`, `CUDA_FLAGS`
and `CUDA_HOST_CXX` are overridable; the host compiler defaults to `g++-13`
for blacknoir's CUDA 12.4 installation. This is a basic starting point with
ordinary triangle records, a linear intersection loop and sequential paths.
The original MT random sequence differs; GPU floating-point results can also
vary from the CPU versions.

## NanoVDB cloud silhouette

`experiments/pathtracer_nvdb.cpp` is a single-threaded CPU example. It needs NanoVDB headers
and a C++17 compiler. One ray per pixel returns white for any
voxel with positive density and black for a miss. HDDA traversal skips empty
regions. There is no lighting or scattering.

```sh
make nvdb-clang compdb
./build/pathtracer_nvdb_clang
```

This writes `build/nvdb-cloud.ppm` at 640×640. `make nvdb` builds with the default C++ compiler.
Optional arguments select the NanoVDB file and output PPM:

```sh
./build/pathtracer_nvdb_clang ../volume-assets/wdas-cloud/wdas_cloud_eighth.nvdb build/nvdb-eighth.ppm
```

`experiments/nvdb_viewer.cpp` displays the same CPU renderer in a raylib window. Drag with
the left mouse button to orbit, use the wheel to zoom, and press Escape to close.
It loads the cloud once and rerenders only when the camera or debug mode changes. The camera,
intersection, and rendering functions are shared through `experiments/nvdb_renderer.h`.

| Key | View | Meaning |
| --- | --- | --- |
| 1 | Silhouette | White if any voxel along the ray has positive density. |
| 2 | Max density | Maximum density along the full ray, divided by the grid's maximum density; linear grayscale. |
| 3 | First-hit depth | First positive-density voxel's ray distance; near is white, far is dark. The range follows a sphere enclosing the grid bounds. |
| 4 | Traversal cells | HDDA cells tested before the first hit or exit; blue means fewer, red means more. Logarithmic scale, saturated at 1023 cells. Rays missing the grid bounds stay black. |

```sh
make nvdb_viewer compdb
./build/nvdb_viewer
```

The viewer accepts an optional NanoVDB filename. `make nvdb_viewer` uses the
default C++ compiler. The first build downloads the official raylib 6.2 Linux
x86-64 package into `build/`; it needs the system OpenGL and X11 development
libraries. `RAYLIB_DIR` and `RAYLIB_LDLIBS` can select another raylib installation.

The local defaults use NanoVDB 10.0.1 headers from
`../volume-assets/.cache/vdb/root/usr/include` and the quarter-resolution
`../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb` dataset. Override the header
location with `make nvdb-clang NANOVDB_INCLUDE=/path/to/include`. Input must be
an uncompressed NanoVDB float fog grid named `density`, with zero background.
The dataset and NanoVDB headers are external to this repository.

The earlier GPU example is preserved as `experiments/pathtracer_nvdb_optix.cpp`, `.cu`, and
`.h`, with separate `nvdb-optix` and `nvdb-optix-clang` build targets.

The [WDAS Cloud Data Set](https://disneyanimation.com/resources/clouds/)
is Copyright 2017 Disney Enterprises, Inc., licensed under
[CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/).

## Single-file interactive OptiX cloud viewer

`experiments/cloud_viewer.cpp` contains the device programs, OptiX setup, raylib window, and
Dear ImGui controls through the existing rlImGui backend.
OptiX traces the cloud bounds and integrates NanoVDB density with selectable
single-scattering ray marching or progressive multiple-scattering path tracing,
plus a spherical Rayleigh/Mie/ozone atmosphere,
a sun disc, and aerial perspective. Raylib handles input and displays the finished
RGBA pixels. The display path is a blocking `cudaMemcpy` to a CPU pixel buffer,
`UpdateTexture`, and `DrawTexturePro`; there is no custom display shader.

```sh
make cloud-viewer compdb
./build/cloud_viewer
./build/cloud_viewer ../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb
```

The default input is `../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb`,
92,497,896 bytes on disk (92.5 MB), with a 92,497,696-byte GPU grid. Both input
files and decoded grids are limited to 100,000,000 bytes. The full 4.1 GB asset
is rejected before loading; the 17.3 MB eighth-resolution asset can still be
selected explicitly.
The grid is uploaded once and sampled directly on the GPU. The longest cloud
dimension is normalized to two scene units; one scene unit is one kilometre
in the atmosphere. The cloud center starts at 3 km altitude. Atmosphere optical
depths are integrated numerically, including Earth shadow and the reddening of
low sunlight. In ray-march mode, skylight is sampled in 32 directions at the cloud center. A 32-cubed
cache stores visibility through the cloud in those directions (about 4.5 MB),
so skylight is shadowed rather than a constant fill. Density edits rebuild
visibility; sun, haze, and altitude edits reuse it.

Press `P` to switch between the two baselines without changing the camera,
sun, altitude, haze, density, or exposure. The switch clears artistic color
overrides so the cloud material stays neutral in both modes. `--mode reference`
starts the reference mode directly; the default is `--mode march`.

The fast baseline uses fixed-step single scattering and an isotropic skylight
approximation. The reference uses delta tracking with one global density bound,
Henyey-Greenstein cloud scattering (g = 0.35, adjustable albedo, default 0.95), explicit directional
sun evaluation, and ratio-tracked cloud shadows. Rayleigh/Mie interactions in
the air within the cloud bounds are sampled too; green-channel probabilities
with RGB likelihood weights account for colored extinction. Escaping paths
query the numerical atmosphere at their exit positions, rather than using the
marcher's skylight cache. Null events do not count as scattering bounces. After
four real events, Russian roulette terminates paths without a fixed depth cutoff.
The density bound comes from NanoVDB min/max statistics, which must be present.

Reference mode accumulates one sample per pixel per frame in linear RGB.
Camera, lighting, density, exposure, preset, mode, and resolution changes reset
accumulation. No denoiser is applied. The HUD shows the mode and sample count.
`--bounces 1` isolates single scattering; `--bounces 0` (default) allows all orders
with roulette. Reference mode keeps artistic color overrides disabled.

The reference is accurate for the chosen transport model within the cloud
bounds as sampling converges. Outside those bounds, the atmosphere remains
numerical single scattering with a simple Lambertian ground. Cloud shadows
cast into that exterior atmosphere, exterior air/cloud feedback, spectral
transport, and a full droplet Mie phase function remain unimplemented. Direct
sunlighting uses a directional sun; the visible disc and its horizon visibility
are approximate. Twilight can still be too dark or have inaccurate colors.
The equations and Earth coefficients follow
the framework documented by [Bruneton](https://ebruneton.github.io/precomputed_atmospheric_scattering/atmosphere/functions.glsl.html);
this viewer does not implement his precomputed scattering tables.

The Make target compiles the same source twice: NVCC builds the `CLOUD_DEVICE`
section into `build/cloud_viewer.ptx`, and the C++ compiler builds the host
section. Keep the executable and its companion PTX together. This requires
CUDA, OptiX 9 headers, NanoVDB headers, and the same raylib dependency as
`nvdb_viewer`. Make also fetches Dear ImGui 1.92.7 and rlImGui pinned at
`1550009359ad975927f7f0e4a3f47e3f27123ea9` into `build/`. Their objects are cached
in `build/libcloud_imgui.a`, so normal viewer edits do not recompile the GUI
library. Font Awesome is disabled. The earlier OpenGL prototype is preserved in
`experiments/archive/cloud_viewer_opengl.cpp`.

The ImGui sidebar provides method and preset selection, sun angles, cloud
altitude, haze, density, exposure, cloud albedo, resolution, bounce limit, camera controls,
reset, and screenshot export. Text, controls, and spacing use a 2x GUI scale.
The window starts at 1600x720, reserving 640 pixels for the scrollable panel
and leaving a 960x720 viewport. `F1` hides/shows the panel. Mouse
and keyboard input captured by ImGui is excluded from viewport controls.

Left-drag to orbit (dragging right increases camera yaw; dragging down raises camera
pitch), scroll to zoom, and right-drag to move the sun, including
below the horizon. Hold Page Up/Down to move the cloud altitude, `,`/`.` to
change aerosol haze, `[`/`]` to change density, and `-`/`=` to change exposure.
`R` resets the camera and lighting; `Space` toggles quality; `S` saves
`build/cloud-viewer.png`; Escape closes.

`1` selects neutral daylight, `2` an orange cloud against blue sky, and `3` a
blue cloud against a warm sunset. Presets change the camera, sun, density,
altitude, haze, exposure, and optional artistic RGB multipliers. `C` cycles the
cloud's scattered-light color through neutral/blue/orange; `B` independently
cycles the displayed sky through neutral/blue/orange. The latter colors the
background and aerial perspective, not the incident skylight cache. The HUD
shows both overrides. `0` clears both overrides while preserving all positions
and physical settings; use it to inspect the atmosphere's calculated colors.
These color overrides are for art direction, not a spectral material model.
Presets 1/2 use EV -1 and preset 3 uses EV 0; match exposure, density, and haze
before comparing physical lighting between positions.
The window can be resized. With quality enabled, the image uses 240 horizontal
pixels during interaction and refines to 480 when released. Ray marching renders
only after view/settings changes; reference mode keeps sampling while idle.

`./build/cloud_viewer --preset warm` and `--preset cool` start the color presets.
`./build/cloud_viewer --smoke` renders three frames, saves the screenshot, and
exits for checking the OptiX launch and raylib window together. It also accepts
`--preset day|warm|cool`. `make test-atmosphere` checks analytic optical depth,
Earth shadow, sunset reddening, integration convergence, segment composition,
and the sun disc without opening a window.

For reproducible comparisons, render matching neutral scenes to PNG and raw
linear RGB (interleaved native float32, stored beside the PNG as `.png.f32`):

```sh
./build/cloud_viewer --mode march --neutral --width 160 --samples 1 --exposure -3 --output build/march.png
./build/cloud_viewer --mode reference --width 160 --samples 1024 --exposure -3 --output build/reference.png
```

`--samples N` exits after N samples. `--width N` fixes the render resolution;
height follows the viewport aspect ratio. `--seed N` makes the reference sequence
repeatable. `--exposure EV` sets the initial/reset exposure and affects only the
PNG/display transform; the `.f32` output contains the accumulated linear mean.
The command can be combined with `--preset warm|cool`. These exports omit the HUD;
`S` and `--smoke` retain the window screenshot behavior.

`make test-cloud-transport` runs the shared sampler on the CPU against analytic
free-path, phase-function, absorption, single-scattering, and conservative
multiple-scattering cases. `make test-cloud-transport-gpu` renders the WDAS cloud
with OptiX in an isolated raylib window, checks finite output, compares independent
seeds at 16/256 spp, and checks the contribution of multiple scattering. It needs
`xvfb-run` and Python 3; it uses no additional Python packages. Validation results
are saved under `build/cloud-transport-checks/`.

Run `./watch.sh` to build and launch the cloud viewer immediately, then rebuild
and relaunch whenever `experiments/cloud_viewer.cpp` or the Makefile changes. The script
uses the installed `watchexec` tool, stops the previous viewer before rebuilding,
and refreshes the compilation database. Build errors leave the watcher running
so the next edit retries. Stop with Ctrl+C. Arguments are passed to the viewer,
for example `./watch.sh ../volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb`.

### Fuller cloud interiors and independent PBRT references

Start with Reference mode and increase **Cloud albedo** from 0.95 to about 0.995.
At 0.95 each collision absorbs 5% of the incoming light; repeated scattering
therefore loses substantial energy. Higher albedo retains more light in the
interior. Increase density moderately (for example 4 to 8) for more optical
thickness, but density alone can darken the cloud. Use a lower exposure to retain
sunlit detail. The asset's silhouette and density structure still determine its
shape; these controls do not add geometric bulk or fill gaps in the dataset.

```sh
./build/cloud_viewer --mode reference --albedo 0.995 --density 8 --exposure -3
make cloud-pbrt
```

`make cloud-pbrt` reuses the installed PBRT v4 checkout at
`../volume-assets/.cache/pbrt-v4` and its `build/pbrt` and `build/imgtool` binaries.
It exports and actually renders day, warm, cool, and a fuller daylight variant
at 256x192, 1024 spp, with PBRT's GPU volumetric path tracer (`volpath`, maximum
depth 128). It also renders both OptiX methods. The exporter shares the viewer's
preset definitions, camera, cloud normalization, density multiplier, albedo,
HG g=0.35, and sun direction. Both use the same 92.5 MB quarter-resolution grid
with trilinear sampling. Artistic color overrides and denoisers are disabled.

Outputs in `build/cloud-pbrt/` include:

- `comparison.png`: four scene rows; ray march, OptiX multiple scattering, PBRT columns.
- `day|warm|cool|day-fuller.pbrt`: editable independent PBRT scenes with absolute asset paths.
- `*-pbrt.exr` and `*-pbrt.pfm`: linear HDR reference renders, plus exposed sRGB PNG previews.
- `*-optix-*.png.f32`: unclipped linear RGB from OptiX; PFM copies support `imgtool` conversion.
- Sky maps, logs, `scenes.json`, and `render-manifest.json` with settings, source/binary hashes,
  PBRT commit, actual commands, render durations, and finite-output checks.

All previews use the same display transform and a -2 EV offset from each preset
(day/warm -3 EV, cool -2 EV) to avoid losing bright cloud detail to clipping.
The linear outputs remain unexposed. For more samples or CPU PBRT rendering:

```sh
make cloud-pbrt PBRT_ARGS='--width 512 --spp 4096 --optix-spp 4096'
make cloud-pbrt PBRT_ARGS='--cpu --width 256 --spp 1024 --optix-spp 1024'
python3 tools/render_cloud_pbrt.py --reuse-renders --exposure-offset -1
```

Use `--scene day` (or `warm`, `cool`, `day-fuller`) to render one scene. For a
fresh 256x192 comparison and linear-RGB measurements of its central cloud crop:

```sh
python3 tools/render_cloud_pbrt.py --scene day --output build/cloud-pbrt-current --spp 4096 --optix-spp 4096
python3 tools/compare_cloud_pbrt.py build/cloud-pbrt-current
```

The analysis saves a side-by-side image, an amplified difference view, and
`day-comparison-metrics.json`. Its default crop is x=70..169, y=65..129 at
256x192; `--crop X0 Y0 X1 Y1` selects another region. These measurements include
sampling noise and the model differences below; they are image differences,
not a standalone test of integrator bias.

**Comparison limits:** these references validate cloud transport under closely
matched illumination, not the entire atmosphere implementation. The exporter
evaluates the viewer's sky and attenuated sunlight at the cloud center, then
uses a PBRT infinite environment and distant sun. The solar disc is excluded
from the environment to avoid adding its energy twice.
The distant sun has no visible disc in the PBRT background, whereas the viewer
includes one; full-frame linear errors can be dominated by this difference.
PBRT's required equal-area maps are converted from the exported latitude-longitude maps using
`imgtool makeequiarea`; their axes and the camera handedness are matched explicitly.
PBRT has no participating air in these scenes, whereas the viewer includes
position-dependent air and aerial perspective. PBRT reconstructs spectral
illumination from the RGB maps/lights; the viewer transports RGB. Background
and near-horizon differences are therefore expected, and pixel error against
these images is not a measurement of the viewer's complete transport accuracy.
The 128-bounce PBRT cutoff is also an approximation; increase it and the sample
count before using very dense, nearly conservative clouds as a convergence target.

The runner needs Python 3, Pillow for the contact sheet, and `xvfb-run` for
isolated raylib windows. OptiX rendering still uses the real GPU under Xvfb.
`--export-only` writes scenes and sky maps without rendering. `PBRT_DIR` overrides
the checkout location. It does not install or modify PBRT.

## OptiX cloud lighting

`experiments/pathtracer_cloud.cpp`, `.cu`, and `.h` add a single-scattering volume renderer.
OptiX returns the entry and exit of the cloud's bounding box. Fixed-step midpoint
integration samples trilinearly interpolated NanoVDB density, applies Beer-Lambert
transmittance, and integrates direct sun lighting with Henyey-Greenstein scattering
and a density-marched shadow ray. Albedo is 0.95 and phase asymmetry is 0.35.
A small constant ambient term and a blue sky gradient provide provisional fill
and background; atmosphere and multiple scattering are future steps.

```sh
make cloud-clang compdb
./build/pathtracer_cloud_clang
./build/pathtracer_cloud_clang ../volume-assets/wdas-cloud/wdas_cloud_eighth.nvdb build/cloud-preview.hdr 320 128 32 4
```

`make cloud` builds with NVCC. Arguments are NanoVDB filename, output filename,
square image width, view steps, shadow steps, and extinction per normalized scene
unit. Defaults are the quarter-resolution cloud, `build/cloud.hdr`, 640, 256, 64,
and 4. The longest cloud dimension is normalized to two scene units. Set
extinction to zero to render only the background. Increasing the step counts
reduces integration error and increases work; this renderer has no Monte Carlo
sample count yet.

HDR output contains linear RGB with no exposure or display transform. An `.f32`
output stores interleaved float32 RGB for numerical checks. The camera looks
from +X toward the cloud center at a 45-degree vertical field of view. Keep the
executable and its companion `.ptx` together. NanoVDB headers, grid requirements,
and WDAS attribution are the same as for the silhouette example above.

## OptiX Cornell box

`pathtracer_optix.cpp` contains host setup and `pathtracer_optix.cu` contains
device programs. A tiny `pathtracer_optix.h` shares the launch parameters;
the example needs no other renderer code or image library.
It builds a 36-triangle Cornell-style room with red and green walls, a white
ceiling patch, and two blocks. Perspective primary rays return the triangle index
and hit distance. Lambert shading uses a point light just below the ceiling
panel, inverse-square attenuation, and a shadow ray for visibility. The panel
also has emission. Diffuse paths use cosine-weighted hemisphere sampling,
Russian roulette, and 64 jittered samples per pixel. RGB albedos are interpreted
as linear values, and output uses clamping and an approximate gamma of 2.2.
The 640×640 result is saved as a binary PPM image. The camera and rotated block
placements match the existing Cornell scene in this repository.
Primary and shadow traces run sequentially in ray generation, so the pipeline
still has trace depth one and uses OptiX's default stack sizes.

```sh
make optix
./build/pathtracer_optix # writes build/optix-cornell.ppm
```

It needs CUDA, the NVIDIA driver, and OptiX headers (fetched as described below).
The `.cpp` builds the executable and the `.cu` builds its companion `.ptx` file;
keep the executable and PTX together. An optional argument selects the output PPM filename.

Clang can compile both the host executable and device PTX:

```sh
make optix-clang
./build/pathtracer_optix_clang
make CXX=clang++ all optim simd bvh
make compdb
```

`make compdb` generates `build/compile_commands.json` from expanded Make recipes for
those CPU targets and `optix-clang`, including the OptiX include path. Clangd
uses a separate command for each host and device source.
Regenerate after changing build flags. Override `COMPDB_TARGETS` to select other
targets, `CLANG_CUDA_CXX`/`CLANG_CUDA_FLAGS` for the CUDA compiler, or `CUDA_ARCH`
for the PTX architecture (default `sm_89` for the RTX 4090).

The workspace Clangd settings use normal database discovery. `make compdb` also
links the database into `build/` for editors configured to look there.

## CUDA execution designs

Four separate source files compare execution organization while sharing the
Cornell scene, PCG streams, half-pixel camera jitter, uniform hemisphere
sampling, and 24-surface limit in `experiments/cuda_common.cuh`:

| File | Design |
| --- | --- |
| `experiments/pathtracer_cuda_megakernel.cu` | One thread per pixel processes complete paths and sequential samples in one kernel. |
| `experiments/pathtracer_cuda_scheduled.cu` | A warp-wide state machine votes for the most populated camera/intersection/shading/finish stage, keeping path state local. Finished paths start the next sample. |
| `experiments/pathtracer_cuda_wavefront.cu` | Separate generation, intersection, shading, accumulation and averaging kernels. Atomic append queues compact active ray/hit work; each pixel has one path per sample pass. |
| `experiments/pathtracer_cuda_optix.cu` | OptiX ray-generation, closest-hit and miss programs. Native triangle BVH traversal invokes closest-hit shading; bounces are iterative. The same file also contains host pipeline/SBT setup. |

```sh
make cuda-designs
./build/pathtracer_cuda_megakernel 128 320 build/megakernel.hdr
./build/pathtracer_cuda_scheduled 128 320 build/scheduled.hdr
./build/pathtracer_cuda_wavefront 128 320 build/wavefront.hdr
./build/pathtracer_cuda_optix 128 320 build/optix.hdr
make test-cuda-designs
```

The OptiX target fetches NVIDIA's headers at tag `v9.0.0` into ignored
`external/optix-dev-v9.0.0/` when missing. Override `OPTIX_INCLUDE` to use an existing
installation. Run the OptiX executable alongside its `.ptx` file. It requires
an R570 or newer NVIDIA driver; the other designs require only CUDA.
The original `experiments/pathtracer_cuda.cu` and `make cuda` remain available.

These are intentionally basic designs, not tuned performance candidates.
The wavefront host launches full pixel-capacity grids for 24 depths, even
when queues become empty, and handles samples in separate passes. Its stage
queues separate intersection from shading; this diffuse-only scene does not
need multiple material queues. The scheduled version groups work within a
warp rather than across the GPU. The OptiX comparison also changes traversal
from a software linear scan to a hardware triangle BVH, so it does not isolate
scheduling alone. It keeps front-face culling, but native intersection arithmetic
and shared-edge tie handling can differ.

All comparison executables accept `[samples] [width] [output]`. An output
ending in `.f32` saves interleaved, native-endian linear float32 RGB for tests;
otherwise it writes Radiance HDR. Tests require exact raw parity among the
three software-intersection designs, repeatability, finite nonnegative pixels,
partial blocks, argument rejection, HDR output, and a numerical OptiX comparison.
The shared scene constructor checks its fixed triangle capacity.

## CPU renderers

`experiments/pathtracer_clang.cpp` follows the original uniform renderer's structure:
plain `Scene`, `pathtrace` and `render` functions, plus a small `main`.
It uses C arrays, per-sample PCG streams, scalar geometry columns, an eight-ray
intersection and OpenMP. There are no templates or sampling-mode options.

```sh
make build/pathtracer_clang
./build/pathtracer_clang 128 320 build/clang.hdr 32
make test-clang
```

Arguments are samples, width, output filename and optional thread count.
The defaults are 128, 320, `build/clang.hdr` and the OpenMP thread count.
`make clang` also builds it; `make render-clang` accepts `RENDER_ARGS`.
[Simplicity checks and timings](docs/CLANG.md) compare it with the previous version.
NEE/cosine/MIS remain available in `experiments/pathtracer_plain.cpp` and the quality builds.

For the STL-free implementation, use `experiments/pathtracer_plain.cpp`. It uses C arrays,
C allocation/I/O and OpenMP, with the same sampling computation.
[Triangle storage experiments](docs/ROUND8.md) compare an AoS wrapper, separate
geometry/surface records, `Float3` field arrays and scalar geometry arrays in
`struct Triangles`. They retain the same eight-ray intersection and sampling.
Clang gains modestly with different layouts for uniform and NEE; GCC gains
no throughput in these measurements. The original plain target remains the
reference. To build the scalar-array experiment or the NEE field-array candidate:

```sh
make triangles-clang build/pathtracer_triangles_clang_fields
./build/pathtracer_triangles_clang_soa 128 320 build/triangles-uniform.hdr --sampling uniform --threads 32
./build/pathtracer_triangles_clang_fields 128 320 build/triangles-nee.hdr --sampling nee --threads 32
```

`make triangles` builds the GCC scalar-array experiment. Layout suffixes are
`aos`, `split`, `fields` and `soa`; strict targets put `strict_` before the suffix.

[Clang versus GCC and the isolated array-loop trigger](docs/ROUND7.md) compare both
compilers and explain the effect of the vector size guard on GCC's loop passes.
[The compiler audit and speed recovery](docs/ROUND6.md) explain how a small inlining
and GCC loop-pass change restores the original speed. The
[general Astra analysis](docs/ROUND5.md) covers sampling quality and six proposals.

```sh
make plain
./build/pathtracer_plain 128 320 build/plain.hdr --threads 32
```

Clang 20 is also available with separate executables:

```sh
make clang
./build/pathtracer_clang 128 320 build/clang.hdr 32
```

`CLANGXX` defaults to `clang++-20`. Clang's plain build uses ordinary O3/native
flags. In the measured case it is close to tuned GCC, while the Clang STL
reference is slower; see round seven for full times, perf and strict checks.

The plain targets use GCC's `-fno-loop-unroll-and-jam` to avoid extra vector
temporaries in the eight-ray loop. `PLAIN_FLAGS` is overridable; use
`make plain PLAIN_FLAGS=` with a compiler that lacks that option. Other targets
retain their existing flags. The loop still uses compiler-generated SIMD.


The latest sampling experiment adds cosine continuation, direct light sampling
(NEE), and optional MIS, while retaining the Cornell scene and 24 surface visits.
NEE gives about 10× better full-image error per second in the measured case.
See [sampling, arithmetic and RNG results](docs/ROUND4.md) for tables, images, data
flow, perf and core scaling. PCG and the original ray-parallel intersection remain
defaults; the other intersection formulas and RNGs are explicit experiments.

```sh
make quality
./build/pathtracer_quality 128 320 build/nee.hdr --batch 8 --refill --threads 32
./build/pathtracer_quality 128 320 build/cosine.hdr --sampling cosine --batch 8 --refill --threads 32
make quality-rng explore
make test-quality test-explore test-rng
```

Quality sampling modes are `uniform`, `cosine`, `nee` (default), and `mis`.
`--seed N` controls the master seed; each pixel/sample keeps its own stream.
Std/OpenMP, AoS/SoA, fixed/refill execution and explicit CPU affinity remain
available. Optional RNG executables are `build/pathtracer_quality_xoroshiro_pp`
and `build/pathtracer_quality_xoroshiro_ss`; they change the random sequence.
`make quality-pgo` trains a GCC profile for NEE; that target is not scored here.

A small Cornell box path tracer with 36 triangles, uniform hemisphere
sampling, a maximum of 24 surface visits, and unclamped linear HDR output.

```sh
make
./build/pathtracer 128 320 build/cornell.hdr

make simd
./build/pathtracer_simd 128 320 build/cornell-simd.hdr

make optim
./build/pathtracer_optim 128 320 build/cornell-optim.hdr
```

Arguments are samples per pixel, image width, and output filename. Defaults are
128 spp, 320 pixels, and `cornell_box.hdr`. `make render`, `make render-optim`,
and `make render-simd` accept the same arguments through `RENDER_ARGS`. Executables and test/benchmark
outputs live in `build/`.

`pathtracer.cpp` is the scalar reference. Default builds use
`-O3 -march=native -ffp-contract=off` for the current CPU. The standalone
`experiments/pathtracer_simd.cpp` keeps the same camera, materials, integrator, arithmetic
order, and seeded random stream. It packs triangle positions and edges into
aligned arrays and intersects eight triangles per AVX2 packet. The final packet
is padded with degenerate triangles; equal-distance hits keep triangle order.
Repack the scene with `pack_triangles()` after changing its triangle list.

`experiments/pathtracer_optim.cpp` uses the same packed layout with ordinary C++ float
operations. It computes independent hit candidates before reducing the nearest
hit in triangle order. It evaluates validity comparisons with bitwise AND and
clamps rejected determinants before division, allowing GCC's `-O3` to vectorize
the candidate loop. There are no intrinsics, vector types, or SIMD pragmas.
The code is portable C++; the compiler chooses its instruction width.
`make build/pathtracer_o3 build/pathtracer_optim_o3` also builds plain `-O3`
controls without `-march=native`.
This scene also needs `pack_triangles()` after geometry edits.

The SIMD build requires an x86 CPU with AVX2 and adds `-mavx2`.
The original three builds disable floating-point contraction to keep arithmetic
consistent and avoid fused operations changing hit decisions. No approximate reciprocal,
fast math, parallel paths, or alternative sampler is used. The camera sampling
expression's missing `Float` was repaired as `Float2{float(x), float(y)}` in both
files, retaining the existing `random_float2() * 0.5f` jitter.

## Validation and timing

```sh
make test
make benchmark
make benchmark BENCH_ARGS="--spp 128 --width 320 --runs 5 --cpu 8"
```

Tests compare 200,006 reference intersections with each of the SIMD, optimized,
and plain `-O3` optimized builds, including empty scenes,
packet tails, back faces, edges, parallel rays, and equal-distance triangles.
They also compare every float32 pixel of a seeded 128×128, 64-spp render before
HDR encoding.

The Linux benchmark uses `taskset` to pin each renderer to one CPU, performs
one full warmup per executable, rotates execution order between trials, and
checks every output's HDR hash. It compares the scalar, optimized, and explicit
SIMD builds with `-march=native`. Add `--generic` to `BENCH_ARGS` to also compare
the plain `-O3` controls. Timings include process startup, scene setup, rendering, and HDR writing; hashes are
checked outside the timed region. Results are saved under `build/`.

See [BENCHMARK.md](docs/BENCHMARK.md) for the measured results. Existing PBRT and
Mitsuba comparison assets and their provenance are in
[references/cornell_box](../../references/cornell_box/README.md).

## Computation experiments

`make simple advanced` builds the new layout, threading and refill experiments.
All new backends use the same reproducible per-pixel/sample PCG streams; these
are deliberately separate from the historical global MT sequence above.

```sh
./build/pathtracer_simple 128 320 build/flat.hdr --kernel flat
./build/pathtracer_simple 128 320 build/refill.hdr --kernel rays --batch 8 --refill --threads 32
./build/pathtracer_advanced 128 320 build/culled.hdr --kernel culled --threads 32
make test-compute
```

The simple intersection loops use ordinary C++; the advanced kernels use AVX2
and FMA. Performance builds permit floating-point contraction; a separate strict
build verifies exact layout/scheduling equivalence. `--layout aos|soa`,
`--batch 1|2|4|8`, `--cpus`, and `--raw` support comparisons. See
[COMPUTATION.md](docs/COMPUTATION.md) for all six proposals, numerical differences,
whole-process paths/second, core scaling, and dataflow diagrams.

Select `--runtime omp` for the OpenMP team or `--runtime std` for explicit
threads. Both use the same tile queue and produce identical raw images.
See [SCALING.md](docs/SCALING.md) for the 1/2/4/8-P, 1/2/4/8/16-E, mixed-core and
SMT comparisons, worker diagnostics and available perf timing events.
`OMP_FLAGS` defaults to `-fopenmp`; set it empty for a std-only build.

Hardware counters, cycle samples, memory-controller traffic, and frequency evidence
are in [PERF.md](docs/PERF.md). [TALKS.md](docs/TALKS.md) connects twelve additional
transcript-backed talks to three simple and three more aggressive next experiments.

The next optimization round reuses one result buffer per worker and combines
sine/cosine evaluation on GNU libc, retaining the original intersection loop.
`make pgo` builds `build/pathtracer_simple_pgo` with GCC profile feedback:

```sh
make pgo
./build/pathtracer_simple_pgo 128 320 build/pgo.hdr --kernel rays --batch 8 --refill --threads 32
```

See [ROUND3.md](docs/ROUND3.md) for the new comparisons, strict numerical parity,
fresh perf interpretation, and the rejected column-interface experiment.
`make test-sincos` checks all possible sampler angles on GNU libc.
