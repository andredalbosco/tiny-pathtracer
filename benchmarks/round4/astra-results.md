# Astra audit: round-four quality prototype

This audit covers the **prototype** quality dataset inspected before the final rerun. Its binary SHA-256 is `a96cce6b81181b17f04b6459cefbaf33bff811b2ed79a5d73c97ac0c6f14673d`; its `experiments/quality_sampling.h` SHA-256 is `bceda3a291fb87873f87747e11d225ca4627eb0b0683bbdb59e314fccec1dcd1`. All five local source hashes matched that dataset's `quality/metadata.json` when audited. Root subsequently tightened emitter-plane exclusion and selected NEE as the default; the final quality rerun, including a new E1 scaling measurement, is pending. Do not attribute these prototype numbers to that later executable without its validation and metadata.

## Supported results

The dataset has 128×128 renders at 32/128/512 spp, eight seed replicates per mode, using 32 logical CPUs. The primary metric averages each run's **linear RGB MSE multiplied by its own whole-process wall time**. It does not multiply an average MSE by a separately selected median time.

At 128 spp, mean MSE and median wall time were respectively:

| Mode | Mean MSE | Median wall seconds | Mean MSE×seconds |
|---|---:|---:|---:|
| Uniform | 0.00813431 | 0.0524106 | 0.000423229 |
| Cosine | 0.00449687 | 0.0439695 | 0.000198740 |
| NEE | 0.000394821 | 0.104560 | 0.0000418247 |
| MIS | 0.000394764 | 0.100542 | 0.0000397206 |

NEE's improvement is **10.119× using the ratio of mean MSE×time products**. Calling it the “mean paired ratio” would be incorrect: that different statistic is 10.904×. Use one convention consistently. The corresponding NEE full-image ratios at 32 and 512 spp are 12.562× and 9.172×, so 10.119× describes this sample budget, not a scene-independent constant or a measured universal time-to-error ratio.

The reference-defined mask `max(reference RGB)<2` retains 16,278/16,384 pixels, or 99.353%. Its 128-spp NEE improvement is 76.493× by the same ratio-of-means convention; the mean of paired ratios is 78.417×. This is a useful secondary result, but the mask is a brightness selection, not an exact geometry classification of all nonemitter pixels. The excluded 106 pixels account for about 87.3% of NEE's full-image squared error. Keep the full-image result primary.

NEE and MIS MSEs differ by less than 0.021% at all three budgets. Timing does not establish a consistent winner: the order changes with sample budget, and the 512-spp differences are small relative to observed variation. Choosing NEE for its smaller state and estimator logic is reasonable; do not claim this experiment establishes that MIS is generally unnecessary or always slower.

## Reference and target checks

I independently recomputed the reference from all 24 saved 2048-spp images and verified their stored image hashes. Averaging in float64 gives 49,152 spp. The two-half estimate of the mean reference's noise is `mean((A-B)^2)/4 = 4.67390e-7` over the full image. The masked estimate is `1.27676e-7`. At 128-spp NEE these are about 0.118% and 0.254% of the corresponding candidate MSEs, respectively. They are finite-sample noise estimates, not rigorous upper bounds or estimates of systematic bias.

The separate 32×32, eight-replicate, 4096-spp estimator agreement calculations correctly pair seeds in the saved row order. Observed mean-image difference MSE divided by estimated mean-difference noise is 1.088 for uniform, 0.899 for cosine and 1.008 for NEE, relative to MIS. This supports agreement at the tested resolution and noise level; it is not proof of unbiasedness. Uniform's mean brightness difference is 2.311 estimated standard errors and should remain disclosed. NEE/MIS share substantial code and random samples, so their exceptionally close agreement does not independently validate shared visibility or PDF code.

The reported 20 strict scheduling/layout checks and any-hit versus nearest-hit visibility agreement are useful determinism checks. They do not replace independent estimator-target tests. The implementation's cosine throughput, area-proportional light-PDF cancellation, accumulated radiance, primary-emitter rule, per-lane preceding BSDF PDF and last-visit gating match the intended diffuse, terminal-emitter estimator. NEE is evaluated only at visits 0..22; an emitter can still contribute at visit23. The origin offset, front-face policy and finite depth are part of the comparison target.

## Visibility correction

The first implementation's shortened endpoint interval could miss a real blocker near the emitter. The revised prototype retains the full finite interval and excludes the sampled emitter and coplanar emissive neighbors. Its tolerance-based normal test could still incorrectly exclude a slightly tilted emitter sharing the reference vertex plane: that triangle may intersect the connection before its endpoint. Tightening to exact normal-component and projected-plane equality is a conservative improvement for the actual axis-aligned Cornell emitter pair. It avoids the identified angular-tolerance failure. General meshes may need explicit, precomputed emitter-plane grouping rather than assuming every mathematically coplanar pair has identical rounded normals.

The final source and binary hashes, close-blocker regression, strict images, quality images and timings should accompany the rerun. The prototype metadata should remain identifiable rather than being silently relabeled as the final implementation.

## Next focused proposals

1. **Stratify camera jitter over the existing sampling domain.** The visible bright region and its boundary dominate remaining global error. At fixed spp, a rectangular grid of equal-area strata with independent per-stratum jitter is a compact first experiment; retain the current half-pixel domain and pixel filter rather than changing the target. This changes the quality-mode samples deliberately. Test several budgets and independent scrambles/seeds; use the same reference. Keep historical uniform sampling available.
2. **Compare bounded shadow batching with scalar any-hit.** Any-hit can stop on the first blocker, whereas an eight-ray mask can amortize geometry loads and use SIMD but may perform more candidates after some lanes are blocked. Preserve each ray's own upper bound and endpoint exclusions. Record actual shadow rays, triangle tests and live lanes alongside time and MSE×time. Precompute static emitter-plane classification so each shadow query does not repeat geometric identity tests. Promote batching only if the measured quality/time benefit pays for the extra state.
3. **Validate the estimator target at more depths and scenes.** Test one, two and three permitted visits with an analytically simple directly lit diffuse patch, primary emitter visibility, complete occlusion and reversed emitter orientation. Add unequal light areas/intensities, triangulation changes, a nearby tilted emissive blocker and a larger emitter where MIS may matter more. Compare independent high-sample means before interpreting a low MSE against the shared MIS reference as correctness. These tests are particularly valuable before combining the quality integrator with numerator/affine intersection kernels that alter floating-point hit decisions.

No source files or benchmark workloads were changed during this audit.
