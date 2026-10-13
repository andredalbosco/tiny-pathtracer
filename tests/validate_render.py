#!/usr/bin/env python3
"""Check deterministic scheduling, sample tails and numerical image differences."""
import array
import json
import math
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
out = root / 'build' / 'validation'
out.mkdir(parents=True, exist_ok=True)

def render(binary, kernel, batch=1, threads=1, layout='aos', samples=19, width=40, refill=False, runtime="std"):
    path = out / f'{binary}-{kernel}-{batch}-{threads}-{layout}.f32'
    command = [str(root / 'build' / binary), str(samples), str(width), str(path), '--raw',
               '--kernel', kernel, '--batch', str(batch), '--threads', str(threads), '--layout', layout, '--runtime', runtime]
    if refill: command.append('--refill')
    process = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    return path.read_bytes(), json.loads(process.stderr)

reference, reference_stats = render('pathtracer_simple_strict', 'scalar')
checks = 0
for kernel in ('scalar', 'flat', 'rays'):
    for batch in (1, 2, 4, 8):
        for layout in ('aos', 'soa'):
            result, stats = render('pathtracer_simple_strict', kernel, batch, 3, layout)
            assert result == reference, (kernel, batch, layout)
            assert stats['rays'] == reference_stats['rays']
            checks += 1
            refilled, _ = render('pathtracer_simple_strict', kernel, batch, 3, layout, refill=True)
            assert refilled == reference, ('refill', kernel, batch, layout)
            checks += 1
            for refill_mode in (False, True):
                parallel, _ = render('pathtracer_simple_strict', kernel, batch, 3, layout, refill=refill_mode, runtime='omp')
                assert parallel == reference, ('OpenMP', kernel, batch, layout, refill_mode)
                checks += 1

reference, stats = render('pathtracer_simple', 'scalar', samples=64, width=128)
values = array.array('f', reference)
strict_reference, strict_stats = render('pathtracer_simple_strict', 'scalar', samples=64, width=128)
strict_values = array.array('f', strict_reference)
fma_rmse = math.sqrt(sum((a-b)**2 for a,b in zip(values,strict_values)) / len(values))
fma_change = dict(rmse=fma_rmse, max_abs=max(abs(a-b) for a,b in zip(values,strict_values)), strict_rays=strict_stats['rays'], fma_rays=stats['rays'])
reports = []
for binary, kernels in [('pathtracer_simple', ('flat', 'rays')), ('pathtracer_advanced', ('affine', 'culled', 'ray8', 'ray8-affine'))]:
    for kernel in kernels:
        batch = 8 if kernel in ('rays', 'ray8', 'ray8-affine') else 1
        result, statistics = render(binary, kernel, batch, samples=64, width=128)
        candidate = array.array('f', result)
        assert len(candidate) == len(values)
        assert all(math.isfinite(v) and v >= 0 for v in candidate), kernel
        assert any(v > 0 for v in candidate), kernel
        threaded, _ = render(binary, kernel, batch, 3, samples=64, width=128)
        assert result == threaded, ('scheduling', kernel)
        rmse = math.sqrt(sum((a-b)**2 for a,b in zip(values,candidate)) / len(values))
        refilled, refill_stats = render(binary, kernel, batch, samples=64, width=128, refill=True)
        assert result == refilled, ('refill numerical equivalence', kernel)
        reports.append(dict(rmse=rmse, max_abs=max(abs(a-b) for a,b in zip(values,candidate)), **statistics))
print(json.dumps(dict(exact_scheduling_checks=checks, scalar_contraction_change=fma_change, kernel_image_differences=reports), indent=2))
