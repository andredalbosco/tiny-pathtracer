# Astra plan: throughput and image quality, round four

The first throughput experiment should be the ordinary C++ ray-parallel numerator test. The first quality experiment should be cosine sampling, followed by one direct-light sample per diffuse vertex. Keep their rankings separate: faster paths and lower image error per second answer different questions. The current source snapshot is the post-840ccd4 implementation; the previous PGO profile attributed about 52.7% of P-core and 74.5% of E-core samples inclusively to intersection, including its packing and nearest-hit bookkeeping. Sampling was about 23.9%/13.8%, nested within scatter. Those percentages are a reason to test, not a speedup prediction.

## Track 1: same sampler, more useful intersection work

### First: defer normalization until the winning triangle

Keep the existing ray8 packing, triangle loop and ordinary C++ inner ray loop. Compute the same unnormalized Möller–Trumbore quantities:

```
h = cross(direction, e2)
D = dot(e1, h)
s = origin - p
U = dot(s, h)
q = cross(s, e1)
V = dot(direction, q)
T = dot(e2, q)
```

For front-facing triangles test `D >= 1e-10`, `U >= 0`, `V >= 0`, `U+V <= D`, and `T > 1e-4*D`. Keeping the redundant `U <= D` check initially makes comparison with the old tests easier. Store each ray's best `T,D,U,V,id`; initialize `bestT=INFINITY, bestD=1, id=-1`. A valid candidate is closer when `T*bestD < bestT*D`. Use strict comparison and original triangle order. At the end, for a valid winner, compute `inverse=1/bestD` and form `t,u,v` using multiplication by that inverse. Empty/zero-direction lanes must return the ordinary miss result, without dividing by an invalid determinant.

This removes a division for every ray/triangle candidate, leaving one per winning ray. It does not necessarily reduce the source-level multiply count: the old three candidate normalization multiplies are replaced by the distance-threshold multiply and two nearest-ratio products. It also carries one additional determinant vector across the triangle loop, possibly increasing register pressure. Geometry traffic and layout remain unchanged. At 36 triangles, broad layout changes are unnecessary to establish whether this arithmetic trade works.

Mathematical equivalence is not float equivalence. Division-free edge, distance and tie decisions round differently; cross-products of numerator and determinant can overflow/underflow for extreme coordinates even when the old quotient is representable. With FMA, a contracted difference used by the compiler can introduce another rounding difference. Validate against both the old float implementation and a double implementation, including exact ties, coplanar/near-parallel rays, edges, vertices, the distance cutoff, inactive lanes, empty scenes and 1/7/8/9/17/36 triangle counts. Log hit IDs and absolute/relative t/u/v error separately. A rare robust fallback can be justified by observed failures, but must have explicit trigger counts and measured cost. Do not call this variant bit-preserving, even with contraction disabled.

Use a fixed, recorded corpus of camera and bounce rays for arithmetic comparison, plus adversarial cases. A random mostly-miss corpus alone can rank a different workload. Then score unchanged-sampler full renders against the frozen PGO binary. Record live rays because different floating-point hits may change later paths. Rebuild/train PGO separately for each candidate with the same training workload and flags; do not compare an untrained new kernel to a well-trained old kernel.

### Second: compact precomputed affine intersection

Use one ordinary struct per triangle containing `p`, unnormalized plane normal `N=e1×e2`, and dual vectors `A=(e2×N)/dot(N,N)`, `B=(N×e1)/dot(N,N)`. Precompute in double and store float, rejecting degenerate triangles explicitly. The ray loop becomes:

```
s = origin - p
D = -dot(N, direction)
t = dot(N, s) / D
w = s + direction*t
u = dot(A, w)
v = dot(B, w)
```

Apply the front-face determinant threshold and ordinary t/u/v validity tests. This removes both per-candidate cross products and uses only dots and a ray advance, at the cost of precomputed coefficients: 48 geometry bytes per triangle versus 36 for p/e1/e2. Across 36 triangles, the extra 432 bytes are small; actual load counts and register pressure matter more than calling the method “more bandwidth heavy.” Keep a triangle-outer/ray-inner C++ loop so geometry can serve eight rays, with no mandatory intrinsics or packets.

The previous explicit affine version lost. This version tests whether ordinary compiler optimization and PGO can make the arithmetic reduction useful, not whether precomputation is inherently faster. Changed coefficient rounding, plane evaluation, and barycentrics require the same numerical checks as above. Do not combine affine coefficients, deferred division and new culling in the first comparison: an isolated loser is easier to reject and explain.

### Small independent control: refill cursor

The current refill lambda still derives pixel/sample IDs through integer division and remainder. An incrementing `(sample,x,y)` cursor can preserve exactly the sequence of job indices and PCG identities while leaving camera expressions and result order untouched. First inspect final PGO assembly to verify expensive divisions remain. This is a small opportunity: the full lambda was only about 4.8% P / 2.7% E, and much of that is indispensable camera work. Require strict image equality and score it separately before combining with a winning intersection kernel.

## Track 2: lower error per second

Keep `uniform` as the historical estimator. Add explicit quality modes in this order: `cosine`, `nee`, and optionally `mis`. Preserve the camera, including its existing half-pixel jitter convention, geometry, one-sided triangle policy, materials, terminal emitters and maximum 24 surface visits. Changing sampling is expected to change RNG consumption and images; deterministic per-path seed identities should still make each mode independent of scheduling. Do not claim identical PCG draws across estimator modes if NEE consumes additional dimensions.

### Cosine continuation

For diffuse reflectance rho, sample a direction with solid-angle density `pB=cos(theta)/pi`; the continuation weight simplifies to `beta *= rho`. Precompute an orthonormal tangent/bitangent per triangle once. A simple polar disk sample uses two uniforms, `r=sqrt(u1)`, `phi=2*pi*u2`, local direction `(r*cos(phi),r*sin(phi),sqrt(1-u1))`, then transforms through that basis. This avoids building a basis at every bounce and retains combined sin/cos. The two extra basis vectors cost only 24 bytes per triangle. Test the directional moments and unit lengths; verify sample means against the existing estimator. Cosine sampling follows the standard diffuse importance-sampling construction, but is not guaranteed to lower variance for every possible incident-light distribution. [PBRT cosine sampling](https://pbr-book.org/4ed/Sampling_Algorithms/Sampling_Multidimensional_Functions)

### Next-event estimation with area lights

Prepare a list of all nondegenerate emissive triangles, their areas, and a selection PMF `q_i`. Area-proportional selection is the simplest generic starting point; area times emitted luminance is another optional PMF if emitters have very different intensity. Every nonzero emitter must have nonzero selection probability. Draw a triangle, then a uniform area point with two independent variates, for example square-root barycentrics. Its joint area density is `pA=q_i/area_i`. [PBRT triangle sampling](https://www.pbr-book.org/4ed/Shapes/Triangle_Meshes)

Let x be the consistently defined, offset ray origin at a diffuse vertex, y the light point, wi=(y-x)/distance, cX=max(0,nX·wi), cL=max(0,nL·(-wi)). If either cosine is zero, contribute zero. With front-face visibility V and current incoming path weight beta, accumulate

`result += beta * rho * Le * [cX*cL / (pi*distance²*pA)] * V`.

Use `+=`, replacing the old “assign only when a light is hit” behavior. For NEE-only, count primary camera hits on emitters normally and suppress every continuation emitter hit. Otherwise those light paths are counted twice. A missing-light scene has zero direct term. Keep emitters terminal to match the current model, even if they also have a stored albedo. Area/solid-angle conversion and one-sided emission follow the usual shape/light formulation. [PBRT shape sampling](https://pbr-book.org/4ed/Shapes/Basic_Shape_Interface), [PBRT area lights](https://pbr-book.org/4ed/Light_Sources/Area_Lights)

Use the same offset origin to define the connection direction, distance, PDFs and later MIS reference. An unoffset PDF paired with a differently aimed offset shadow ray is an avoidable inconsistency. Begin with shared intersection/visibility code for correctness. A dedicated bounded any-hit query is a subsequent compute optimization: it need not find a nearest triangle or return barycentrics and can stop on the first blocker. Specify the finite segment and exclude its sampled emitter endpoint; do not allow floating-point self-intersection at y to report a blocker. Preserve the current front-face-only blocker policy for this comparison. Test blocked/unblocked light points, reverse-facing lights, self-shadowing, triangle seams and close endpoints. The geometric offset remains a numerical approximation that should be common across modes.

### Optional MIS

For one light sample and one cosine continuation, convert the light PDF to solid angle: `pL=pA*distance²/cL`. Use power weights `wL=pL²/(pL²+pB²)` and `wB=pB²/(pL²+pB²)`, guarding zero/nonfinite inputs. Multiply the NEE term by wL. When a continuation hits an emitter, add `beta*Le*wB`, using the actual hit light's selection PMF and area, the preceding diffuse reference point, and the preceding BSDF PDF. Primary light hits have weight one. Store only the minimal previous-vertex data needed; the current ray origin/direction can often supply it. The selection PMF is part of pL and cannot be omitted. MIS partitions overlapping sampling contributions; it does not mean summing full NEE and full emitter-hit estimates. [PBRT path-tracer MIS](https://pbr-book.org/4ed/Light_Transport_I_Surface_Reflection/A_Better_Path_Tracer), [Veach's MIS derivation](https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf)

### Preserve the finite-depth target

Visits are zero-based 0..23. At an emitter visit, evaluate its allowed emission contribution before termination. At a diffuse visit, add NEE only when `depth < 23`: its sampled emitter is the next surface visit. Adding NEE at depth23 would include a 25th surface and change the target relative to the existing tracer. In NEE-only, a continuation emitter at visit23 is suppressed because the direct term at visit22 already represents it. In MIS, both complementary weighted techniques remain available at that last connection. No Russian roulette in the first comparisons; it would introduce another efficiency trade and another variable.

## Quality measurement and acceptance

Use a separate quality dataset. Render the same linear RGB target with each estimator at several sample counts or matched time budgets, with at least 8 independent seed replicates for final comparisons. Report whole-process time, render time, MSE/RMSE against a common independent high-sample reference, and MSE times time. For approximately unbiased Monte Carlo in the variance-dominated regime, smaller `MSE*time` means better efficiency; a single seed or a single visually pleasing crop is insufficient. Compare error versus measured time directly when overhead or bias makes the inverse-time approximation poor. This efficiency framing follows the standard variance-reduction objective. [PBRT Monte Carlo efficiency](https://pbr-book.org/4ed/Monte_Carlo_Integration/Improving_Efficiency)

Build the reference from many independent moderate-spp renders averaged in float64, avoiding a huge float32 sequential pixel sum. Produce two independent reference halves A and B; their mean has a reference-noise floor estimated by mean((A-B)²)/4 when both halves have equal variance. Make that floor small relative to candidate error and disclose it. Use a common, tested intersection implementation initially; later quantify whether a faster numerical kernel changes converged means. Independently compare high-sample cosine, NEE and MIS means to catch estimator bugs that self-reference would hide. Measure the emitting panel separately if it dominates the global score, and include dark indirect regions and shadow boundaries, while retaining the full-image metric.

The camera and depth target must match the reference exactly. Do not compare with earlier Mitsuba/PBRT images having different camera/material/estimator assumptions and call the difference sampling noise. Do not clamp fireflies, tone map, denoise or compare gamma-encoded images in the primary MSE score. Keep image-quality rankings separate from paths/s, ray-segments/s, shadow-ray count and triangle-test counts. NEE can trace more rays and still win decisively in useful image information per second.

Retain only experiments with reproduced benefit and clear numerical behavior. The clarified paper is *Manuka: A batch-shading architecture for spectral path tracing in movie production*. Its authors explicitly compare variance reduction with sampling cost: halving per-sample standard deviation permits four times the cost per sample at equal asymptotic error/time. That motivates the quality track and `MSE*time`, without making any unmeasured claim about this Cornell box. Its whole-path CPU and shade-before-hit design addressed large production scenes and expensive shading; it is not evidence that this renderer should discard its measured eight-ray refill winner. I read the preserved page 5 directly. [Authors’ manuscript, §2.4](https://jo.dreggn.org/home/2018_manuka.pdf)


Implementation note after coordination: keep the historical uniform `Path` representation unchanged. A compile-time quality-scatter hook can keep optional quality logic out of its hot loop. For MIS, a worker-local `previous_bsdf_pdf[8]` is sufficient in this restricted diffuse/terminal-emitter model if the current ray origin is retained until emission evaluation: that origin supplies the preceding offset reference point. Index the PDF by persistent lane, update it on every continuation, and skip reading it for depth-zero primary hits. This avoids adding a field to every historical path. Maintain equivalent logic in fixed batches if those modes remain supported.
