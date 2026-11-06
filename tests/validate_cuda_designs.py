"""Execution parity, deterministic streams, partial blocks, and output checks."""
from array import array
from pathlib import Path
import math
import subprocess

root = Path(__file__).resolve().parents[1]
variants = ('megakernel', 'scheduled', 'wavefront', 'optix')

def run(name, samples, width, filename):
    subprocess.run([str(root / 'build' / ('pathtracer_cuda_' + name)),
                    str(samples), str(width), str(filename)], cwd=root, check=True)
    values = array('f')
    values.frombytes(filename.read_bytes())
    assert len(values) == width * width * 3
    assert all(math.isfinite(x) and x >= 0 for x in values)
    return values

for width, samples in ((1, 1), (17, 8), (65, 32), (128, 64)):
    images = {}
    for name in variants:
        filename = root / 'build' / f'cuda-check-{name}.f32'
        images[name] = run(name, samples, width, filename)
        repeated = root / 'build' / f'cuda-check-{name}-repeat.f32'
        run(name, samples, width, repeated)
        assert filename.read_bytes() == repeated.read_bytes(), name + ' is nondeterministic'
    reference = images['megakernel']
    assert reference == images['scheduled'], 'warp scheduling changed the image'
    assert reference == images['wavefront'], 'queue scheduling changed the image'
    rms = math.sqrt(sum(x*x for x in reference) / len(reference))
    error = math.sqrt(sum((x-y)**2 for x,y in zip(reference, images['optix'])) / len(reference))
    # Native triangle intersections differ from the software intersection routine.
    # Use a numerical image check, not an exact-pixel claim, for this backend.
    relative_error = error / max(rms, 1e-10)
    assert relative_error < 1e-4, f'OptiX relative RMS error {relative_error}'
    print(f'{width}x{width}, {samples} spp: CUDA exact; OptiX relative RMS={relative_error:.6g}')

for name in variants:
    binary = root / 'build' / ('pathtracer_cuda_' + name)
    for args in (['0'], ['1', '0'], ['bad'], ['1', '16385']):
        result = subprocess.run([str(binary), *args], cwd=root, capture_output=True)
        assert result.returncode != 0, (name, args)
    hdr = root / 'build' / f'cuda-check-{name}.hdr'
    subprocess.run([str(binary), '8', '17', str(hdr)], cwd=root, check=True)
    data = hdr.read_bytes()
    assert data.startswith(b'#?RADIANCE') and b'-Y 17 +X 17' in data
print('All four designs passed.')
