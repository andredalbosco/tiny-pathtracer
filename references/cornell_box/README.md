# Cornell box reference renders

All three images are 320 × 320 at 1,024 samples per pixel. Geometry, RGB material values, emitted radiance, and camera were exported from the same `cornell_box()` implementation into `scene.json`; there are 36 triangles. Mitsuba camera rays were checked against the tiny tracer. All display PNGs use the same standard sRGB conversion after clamping, with no exposure adjustment or tone mapping.

- `tiny.hdr`: unclamped Radiance RGBE output from the tiny tracer; HDR encoding has limited mantissa precision.
- `mitsuba.exr`: linear float32 RGB from Mitsuba 3.9.1, CUDA RGB variant, standard path integrator with depth 24 and roulette disabled; reproducible scene in `scene.xml` and the PLY files.
- `pbrt.exr`: linear float32 RGB from PBRT v4's spectral path integrator, maximum 23 BSDF bounces (matching the tiny tracer's 24 surface visits); reproducible scene in `scene.pbrt`.
- `comparison.png`: display comparison. `metrics.json`: single-run linear RMSE and MAE; these include sampling noise, HDR quantization, and PBRT spectral-model differences, so they are not a strict correctness verdict.

Tiny tracer uses uniform hemisphere sampling without explicit light sampling or Russian roulette. The references use their standard importance/light sampling, which explains their lower noise at the same sample count. See `metadata.json` for source/header versions and settings.
