"""Measure and visualize an existing OptiX/PBRT scene pair in linear RGB."""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont, ImageOps

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path)
parser.add_argument('--scene', default='day')
parser.add_argument('--crop', type=int, nargs=4, default=[70, 65, 170, 130],
                    metavar=('X0', 'Y0', 'X1', 'Y1'))
args = parser.parse_args()
out = args.directory.resolve()
manifest = json.loads((out/'render-manifest.json').read_text())
scene = next(s for s in manifest['scenes'] if s['name'] == args.scene)
name = scene['name']
w, h = manifest['width'], manifest['height']
x0, y0, x1, y1 = args.crop
if not 0 <= x0 < x1 <= w or not 0 <= y0 < y1 <= h:
    parser.error('--crop must lie inside the rendered image')

with (out/(name+'-pbrt.pfm')).open('rb') as f:
    assert f.readline().strip() == b'PF'
    assert tuple(map(int, f.readline().split())) == (w, h)
    little = float(f.readline()) < 0
    pbrt = array('f', f.read())
if little != (sys.byteorder == 'little'):
    pbrt.byteswap()
# PFM scanlines run from bottom to top; the viewer exports top to bottom.
pbrt = array('f', (pbrt[((h-1-y)*w+x)*3+c]
                  for y in range(h) for x in range(w) for c in range(3)))
renders = {'pbrt': pbrt}
for mode in ['reference', 'march']:
    values = array('f', (out/(name+'-optix-'+mode+'.png.f32')).read_bytes())
    if sys.byteorder != 'little':
        values.byteswap()
    renders[mode] = values
for label, values in renders.items():
    if len(values) != w*h*3 or not all(math.isfinite(v) for v in values):
        raise ValueError('Invalid linear image: '+label)

def metrics(test, box):
    a, b = [], []
    for y in range(box[1], box[3]):
        start, end = (y*w+box[0])*3, (y*w+box[2])*3
        a.extend(test[start:end]); b.extend(pbrt[start:end])
    rmse = math.sqrt(sum((v-r)**2 for v, r in zip(a, b))/len(a))
    energy = math.sqrt(sum(v*v for v in b)/len(b))
    def luminance(values):
        return sum(.2126*r+.7152*g+.0722*b
                   for r, g, b in zip(values[0::3], values[1::3], values[2::3]))/(len(values)//3)
    mean_a, mean_b = luminance(a), luminance(b)
    return {'rmse': rmse, 'relative_rms_percent': 100*rmse/energy,
            'mean_luminance': mean_a, 'pbrt_mean_luminance': mean_b,
            'mean_luminance_difference_percent': 100*(mean_a/mean_b-1)}

results = {mode: {'full_image_including_unmatched_solar_disc': metrics(renders[mode], (0, 0, w, h)),
                  'cloud_crop': metrics(renders[mode], args.crop)}
           for mode in ['reference', 'march']}
ev = scene['display_exposure']
gain = 8
def byte(v):
    v = max(0, v)
    v = 12.92*v if v <= .0031308 else 1.055*v**(1/2.4)-.055
    return round(255*min(1, v))
error = Image.new('RGB', (w, h))
error.putdata([tuple(byte(abs(renders['reference'][i+c]-pbrt[i+c])*gain*2**ev)
                     for c in range(3)) for i in range(0, w*h*3, 3)])
error.save(out/(name+'-absolute-difference-x8.png'))

font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 18)
small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15)
tile_w, tile_h = 384, 288
board = Image.new('RGB', (3*tile_w+32, 2*tile_h+148), '#171b22')
draw = ImageDraw.Draw(board)
draw.text((12, 10), f'{name}: {w} x {h}, {manifest["spp"]} spp, EV {ev:+g}, no denoising', font=font, fill='white')
images = [Image.open(out/(name+'-optix-reference.png')).convert('RGB'),
          Image.open(out/(name+'-pbrt.png')).convert('RGB'), error]
for col, (label, image) in enumerate(zip(['Our OptiX reference', 'PBRT v4 GPU', 'Absolute RGB difference x8'], images)):
    x = 8+col*(tile_w+8)
    draw.text((x, 48), label, font=font, fill='#d4ddeb')
    board.paste(image.resize((tile_w, tile_h), Image.Resampling.NEAREST), (x, 76))
    crop = ImageOps.pad(image.crop(tuple(args.crop)), (tile_w, tile_h),
                       method=Image.Resampling.NEAREST, color='#171b22')
    board.paste(crop, (x, tile_h+106))
    image.close()
draw.text((12, tile_h+80), f'Central cloud crop: {args.crop}; enlarged for inspection', font=small, fill='#d4ddeb')
draw.text((12, 2*tile_h+118), 'Different atmosphere models and RGB/spectral transport; differences include sampling noise.', font=small, fill='#d4ddeb')
board.save(out/(name+'-detailed-comparison.png'))

report = {'scene': name, 'resolution': [w, h], 'pbrt_spp': manifest['spp'],
          'optix_spp': manifest['optix_spp'], 'cloud_crop': args.crop, 'display_exposure': ev,
          'pbrt_reference_reused': manifest.get('pbrt_reference_reused'),
          'normalization': 'Relative RMS = RGB RMSE / RMS reference RGB; means use linear Rec.709 luminance.',
          'metrics': results, 'limitations': manifest['comparison_limits']+[
              'PBRT distant light has no visible solar disc; the viewer includes a very bright disc.',
              'Full-frame errors include this unmatched disc and cannot assess cloud transport accuracy.',
              'The central crop is a rectangle, not a cloud-opacity mask; it includes some background.']}
(out/(name+'-comparison-metrics.json')).write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
