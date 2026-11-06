"""Render the viewer presets independently in an existing PBRT-v4 installation."""
import argparse
from array import array
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time

repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pbrt-dir', type=Path, default=repo.parent/'volume-assets/.cache/pbrt-v4')
parser.add_argument('--asset', type=Path, default=repo.parent/'volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb')
parser.add_argument('--output', type=Path, default=repo/'build/cloud-pbrt')
parser.add_argument('--width', type=int, default=256)
parser.add_argument('--spp', type=int, default=1024)
parser.add_argument('--optix-spp', type=int, default=1024)
parser.add_argument('--scene', choices=['all', 'day', 'warm', 'cool', 'day-fuller'], default='all')
parser.add_argument('--cpu', action='store_true', help='Use PBRT CPU rendering instead of its GPU renderer')
parser.add_argument('--export-only', action='store_true')
parser.add_argument('--reuse-renders', action='store_true', help='Rebuild previews from existing linear renders')
parser.add_argument('--exposure-offset', type=float, default=-2,
                    help='Same EV adjustment for all renderers; default -2 preserves bright cloud detail')
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
pbrt_dir = args.pbrt_dir.resolve()
pbrt, imgtool = pbrt_dir/'build/pbrt', pbrt_dir/'build/imgtool'
if not pbrt.is_file() or not imgtool.is_file():
    parser.error('Expected existing build/pbrt and build/imgtool under --pbrt-dir')
if args.optix_spp < 1:
    parser.error('--optix-spp must be positive')
if not math.isfinite(args.exposure_offset) or not -4 <= args.exposure_offset <= 4:
    parser.error('--exposure-offset must be finite and between -4 and 4')
environment = {**os.environ, 'OMP_NUM_THREADS': '4'}
records = []

def run(label, command):
    print(label, flush=True)
    start = time.monotonic()
    with (out/(label+'.log')).open('w') as log:
        result = subprocess.run([str(a) for a in command], cwd=repo, env=environment,
                                stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{label} failed; see {out/(label+".log")}')
    records.append({'name': label, 'command': [str(a) for a in command],
                    'seconds': round(time.monotonic()-start, 3)})

def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

if not args.reuse_renders:
    run('export', [repo/'build/export_cloud_pbrt', args.asset.resolve(), out, args.width, args.spp])
scenes = json.loads((out/'scenes.json').read_text())
if args.scene != 'all':
    scenes['scenes'] = [scene for scene in scenes['scenes'] if scene['name'] == args.scene]
for scene in scenes['scenes']:
    name = scene['name']
    if not args.reuse_renders:
        run(name+'-sky', [imgtool, 'makeequiarea', '--resolution', 512, '--outfile',
                         out/(name+'-sky.exr'), out/(name+'-sky-latlong.pfm')])
if args.export_only:
    print(f'PBRT scenes exported to {out}', flush=True)
    raise SystemExit(0)

for scene in scenes['scenes']:
    name = scene['name']
    scene['display_exposure'] = scene['exposure']+args.exposure_offset
    if not args.reuse_renders:
        run(name+'-pbrt', [pbrt, *([] if args.cpu else ['--gpu']), '--nthreads', 4,
                          '--seed', 1, '--quiet', out/(name+'.pbrt')])
    run(name+'-pbrt-png', [imgtool, 'convert', '--scale', 2**scene['display_exposure'],
                          '--outfile', out/(name+'-pbrt.png'), out/(name+'-pbrt.exr')])
    run(name+'-pbrt-linear', [imgtool, 'convert', '--outfile', out/(name+'-pbrt.pfm'),
                             out/(name+'-pbrt.exr')])
    for mode in ['march', 'reference']:
        # Xvfb supplies the raylib window only; OptiX still uses the real GPU.
        if not args.reuse_renders:
            run(name+'-optix-'+mode, ['xvfb-run', '-a', '-s', '-screen 0 1600x900x24',
                'env', 'LIBGL_ALWAYS_SOFTWARE=1', repo/'build/cloud_viewer', args.asset.resolve(),
                '--mode', mode, '--preset', scene['preset'], '--neutral', '--albedo', scene['albedo'],
                '--density', scene['density'], '--exposure', scene['exposure'], '--width', args.width,
                '--samples', 1 if mode=='march' else args.optix_spp, '--seed', 1,
                '--output', out/(name+'-optix-'+mode+'.png')])
        # Rebuild all previews from linear outputs with the same exposure.
        linear = out/(name+'-optix-'+mode+'.pfm')
        rgb = (out/(name+'-optix-'+mode+'.png.f32')).read_bytes()
        w, h = scenes['width'], scenes['height']
        if len(rgb) != w*h*12:
            raise RuntimeError(f'Unexpected dimensions in {name}/{mode}')
        with linear.open('wb') as f:
            f.write(f'PF\n{w} {h}\n-1.0\n'.encode())
            for y in range(h-1, -1, -1):
                f.write(rgb[y*w*12:(y+1)*w*12])
        run(name+'-optix-'+mode+'-png', [imgtool, 'convert', '--scale', 2**scene['display_exposure'],
            '--outfile', out/(name+'-optix-'+mode+'.png'), linear])

# Inspect linear PBRT data, including errors hidden by display clipping.
statistics = {}
for scene in scenes['scenes']:
    path = out/(scene['name']+'-pbrt.pfm')
    with path.open('rb') as f:
        assert f.readline().strip() == b'PF'
        w, h = map(int, f.readline().split())
        scale = float(f.readline())
        values = array('f', f.read())
    if scale > 0:  # This exporter/runner targets little-endian Linux.
        values.byteswap()
    assert (w, h)==(scenes['width'], scenes['height']) and len(values)==w*h*3
    assert all(math.isfinite(v) for v in values), f'Non-finite PBRT output: {path}'
    statistics[scene['name']] = {'linear_mean': [sum(values[c::3])/(w*h) for c in range(3)],
                                  'linear_min': min(values), 'linear_max': max(values)}

from PIL import Image, ImageDraw, ImageFont
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 14)
width, height = scenes['width'], scenes['height']
board = Image.new('RGB', (3*width+16, len(scenes['scenes'])*(height+40)+48), '#171b22')
draw = ImageDraw.Draw(board)
for col, label in enumerate(['OptiX: ray march', 'OptiX: multiple scattering', 'PBRT v4: volpath']):
    draw.text((8+col*width, 10), label, font=font, fill='white')
for row, scene in enumerate(scenes['scenes']):
    name = scene['name']
    label = f"{name}  |  albedo {scene['albedo']:.3f}, density {scene['density']:g}, EV {scene['display_exposure']:+g}"
    y = 40+row*(height+40)
    draw.text((8, y), label, font=font, fill='#d4ddeb')
    for col, suffix in enumerate(['optix-march', 'optix-reference', 'pbrt']):
        with Image.open(out/(name+'-'+suffix+'.png')) as image:
            board.paste(image.convert('RGB'), (8+col*width, y+24))
board.save(out/'comparison.png')

if args.reuse_renders:
    previous = json.loads((out/'render-manifest.json').read_text())
    records = previous['runs']+records
report = {**(previous if args.reuse_renders else {}), **scenes,
          'optix_spp': previous['optix_spp'] if args.reuse_renders else args.optix_spp,
          'pbrt_backend': previous['pbrt_backend'] if args.reuse_renders else 'CPU' if args.cpu else 'GPU',
          'display_exposure_offset': args.exposure_offset,
          'pbrt_commit': subprocess.check_output(['git', '-C', str(pbrt_dir), 'rev-parse', 'HEAD'], text=True).strip(),
          'asset_sha256': digest(args.asset.resolve()), 'viewer_sha256': digest(repo/'experiments/cloud_viewer.cpp'),
          'pbrt_binary_sha256': digest(pbrt), 'runs': records, 'pbrt_statistics': statistics,
          'comparison_limits': ['PBRT uses a sky environment and sunlight evaluated at cloud center.',
              'The viewer shows a solar disc; PBRT distant lighting has no visible disc in these scenes.',
              'OptiX integrates position-dependent air inside and outside the cloud bounds.',
              'PBRT reconstructs spectra from RGB lights; OptiX transports RGB directly.',
              'PBRT maxdepth is 128; OptiX uses roulette without a fixed depth limit.',
              'Neutral colors, no denoiser, same exposure and sRGB display transform.']}
for key, path in [('viewer_binary_sha256', repo/'build/cloud_viewer'),
                  ('exporter_binary_sha256', repo/'build/export_cloud_pbrt')]:
    report[key] = previous.get(key, digest(path)) if args.reuse_renders else digest(path)
if args.reuse_renders:
    # Preview edits must preserve the provenance of the original linear renders.
    for key in ['pbrt_commit', 'asset_sha256', 'viewer_sha256', 'pbrt_binary_sha256']:
        report[key] = previous[key]
(out/'render-manifest.json').write_text(json.dumps(report, indent=2)+'\n')
print(f'Saved {out/"comparison.png"}', flush=True)
