# Astra: plain baseline plan and source review

The broader user correction is handled in `astra-general-pass5.md`: this plain rewrite is one baseline within the renderer analysis, not the entire optimization agenda. No renderer source or benchmark was modified by this review.

## Review result

I read the complete 509-line local `pathtracer_plain.cpp`, SHA256 `eb6ab6ffdc40b1bf3ffb4d3b3690cf8f8a6443a3c366e674dc66277ee0b20ef8`, against the current `renderer_common.h`, `simple_kernels.h`, `render_driver.h`, `quality_sampling.h` and quality entrypoint. No blocking rendering-math or scheduling defect was found by inspection. This is not a substitute for the parent's compiled raw-image tests.

The reviewed source has no STL containers or std::thread/chrono/iostream use. It still uses ordinary C++ structs, operators, lambdas and compile-time templates; those are language facilities, consistent with the requested no-STL scope. `<new>` supplies placement construction, while allocation comes from malloc/calloc/posix_memalign. The copied numerical helpers do not acquire different mathematics merely because their names lose `std::`.

Checked invariants:

- Float operations and expression grouping match the originals, including `2*M_PI*uniform` in the samplers and the camera's **double tan argument/result followed by float assignment**. `sqrtf`, `sinf` and `cosf` match the original float overloads. The `maximum(a,b)` helper has the original comparison/select ordering rather than substituting fmaxf's different NaN behavior.
- Both PCG state words, seed preparation, stream identity and high-24-bit uniform conversion match. Optional xoroshiro implementations are also copied; their earlier statistical caveats and opt-in status remain applicable.
- `Path p{}` value-initializes inactive ray members; `Ray rays[8]{}` initializes unused batch lanes. Every lane is refilled before its first read. Batch ts/ids are initialized for all eight lanes, and hits are overwritten before scatter reads them.
- The worker result allocation is sufficient for min(16,width)×min(8,width)×spp records. Placement default construction is trivial for the changed Float3 type, so it does not intentionally zero the results. Every job is issued once, every completed job assigns its result, and the tile ends only after all active lanes finish. Ordered sums therefore read assigned records, including small/partial tiles.
- Previous MIS PDFs are initialized in Scratch, written at diffuse continuation, and ignored for primary emitters. A refilled lane may contain an old PDF, but the depth-zero branch does not read it. The previous ray origin/direction remains available when evaluating the emitter-hit PDF.
- NEE excludes non-primary continuation emitter contributions, keeps the full finite shadow segment and current exact coplanar-emitter exclusions, and generates direct-light samples only before the final surface visit. Mode-specific accumulation matches the corresponding reference mode. The unchanged 24-visit cap remains in the refill loop.
- Pixel sample sums preserve original ascending sample order. Integer cursor progression retains pixel/sample IDs. Per-tile writes are disjoint. Workers claim tiles through a relaxed atomic counter; all synchronization happens before totals are read.
- Statistics requires 64-byte alignment; posix_memalign requests 64, the requested array length is a multiple of sizeof(Statistics), and placement construction runs the default member initializers. The old Statistics was already cache-line aligned, so this is not a newly introduced false-sharing optimization.
- Allocation, affinity and undersized OpenMP-team failures set an atomic failure flag and prevent an apparently successful output. The sample-count limit keeps full-tile job arithmetic below INT_MAX for accepted options.

The main remaining tests are strict image equality for all four modes, several seeds, widths 1/7/8/15/16/17 and a normal image, sample counts including a partial lane group, and worker counts 1/2/32 with CPU lists. Compare ray, slot, batch and shadow counts as well as pixels. Test actual finite shadow endpoints and coplanar seams through a small harness. A debug sanitizer run is useful for the deliberate initialization change; benchmark only the ordinary optimized builds.

Two limited differences deserve explicit documentation rather than blocking this experiment. The plain CLI accepts only the selected eight-ray/AoS/refill/OpenMP path, and it lacks nearest-visibility and worker-report switches; compare supported matching configurations. It uses atoi/strtoull without complete conversion/range-error checks, so malformed or oversized textual numeric inputs do not have the same rejection semantics as the exception-based CLI. Use strtol/strtoull with end-pointer and errno checks if maintaining the plain executable as a general interface. This is outside the validated numeric benchmark inputs. Also, plain uniform mode calls the basis/light preparation routine even though RayScene uniform preparation is empty; this affects only whole-process setup, and should not be mistaken for rendering work.

## What the comparison can establish

Freshly inspected GCC15 headers implement array indexing as underlying C-array indexing and vector indexing as a start-pointer dereference; size is a pointer difference. Whether bound loads, checks or copies survive must be answered by the actual build/assembly. Existing `if constexpr` mode selection and HasBatch traits generate specialized code; removing their source syntax does not automatically remove runtime work.

The rewrite can change real costs: result-buffer default initialization, setup allocation, fixed-capacity/count optimization, hot-function code shape, loop bounds and alias analysis, CLI parsing and logging. It can also worsen inlining or constant propagation. The NEE sample report's 1.86% inlined vector::size attribution is neither zero evidence nor proof of a 1.86% removable library call. It points to the loop's generated code for inspection.

Benchmark plain versus current Quality **OpenMP**, not versus std::thread. Use the same CPU lists, scene/camera, mode, PCG seed, eight-ray refill, FP flags and output format. At least P1 and all32 should have five warmed interleaved full-render observations; add E1 for architectural interpretation. Keep source/binary hashes and raw timings. Present process wall time as the primary score, with render-only time as a secondary diagnostic. Worker allocations are inside both render intervals, while scene preparation and image allocation are outside both. malloc's avoided zero fill is a legitimate implementation difference, but not proof that all vector operations are slow.

Do not compare a plain non-PGO executable to a separately well-trained PGO reference and label the result an STL effect. If testing PGO, train both with the same policy and identify the mode mixture. Strict FP builds should match bitwise; fast/FMA builds may generate different contractions despite matched source expressions, so record image error, changed values and ray counts separately.

The first follow-up computations remain the general report's S1 camera stratification, S2 dedicated bounded shadow query and S3 cheaper cosine continuation. Their numerical and quality targets differ from the no-STL baseline and must stay separate until measured.
