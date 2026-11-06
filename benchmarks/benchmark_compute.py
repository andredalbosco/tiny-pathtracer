#!/usr/bin/env python3
"""Sequential, interleaved renderer measurements; never run competing jobs."""
import argparse
import json
import hashlib
import platform
import pathlib
import statistics
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('--matrix', required=True, help='JSON array of configurations')
parser.add_argument('--output', required=True)
parser.add_argument('--samples', type=int, default=128)
parser.add_argument('--width', type=int, default=320)
parser.add_argument('--runs', type=int, default=5)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
configurations = json.loads(pathlib.Path(args.matrix).read_text())
sources = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in ('experiments/renderer_common.h','experiments/render_driver.h','experiments/simple_kernels.h','experiments/advanced_kernels.h','experiments/pathtracer_simple.cpp','experiments/pathtracer_advanced.cpp','experiments/pathtracer_explore.cpp','experiments/experimental_kernels.h','experiments/pathtracer_quality.cpp','experiments/quality_sampling.h') if (root / name).exists()}
results = [[] for _ in configurations]

def measure(config):
    command = [str(root / 'build' / config.get('binary', 'pathtracer_simple')),
               str(args.samples), str(args.width), str(root / 'build' / 'benchmark-compute.hdr')]
    for key in ('kernel', 'batch', 'threads', 'layout', 'cpus', 'runtime', 'sampling', 'seed', 'visibility'):
        if key in config: command += ['--' + key, str(config[key])]
    if config.get('refill'): command.append('--refill')
    start = time.perf_counter()
    run = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    stats = json.loads(run.stderr)
    stats['wall_seconds'] = time.perf_counter() - start
    stats['output_sha256'] = hashlib.sha256((root / 'build' / 'benchmark-compute.hdr').read_bytes()).hexdigest()
    return stats

for config in configurations:
    measure(config)
for repetition in range(args.runs):
    order = range(len(configurations)) if repetition % 2 == 0 else reversed(range(len(configurations)))
    for index in order:
        result = measure(configurations[index])
        results[index].append(result)
        print(f"{repetition + 1}/{args.runs} {configurations[index]} {result['render_seconds']:.6f}s", flush=True)

report = dict(width=args.width, samples=args.samples, runs=args.runs, timing_scope='render includes worker creation and tile/sample buffers; excludes scene preparation, image allocation, HDR writing', machine=platform.platform(), source_sha256=sources, configurations=[])
for config, runs in zip(configurations, results):
    report['configurations'].append(dict(config=config, runs=runs,
        binary_sha256=hashlib.sha256((root / 'build' / config.get('binary','pathtracer_simple')).read_bytes()).hexdigest(),
        median_render_seconds=statistics.median(r['render_seconds'] for r in runs),
        median_wall_seconds=statistics.median(r['wall_seconds'] for r in runs),
        median_paths_per_second=args.width**2 * args.samples / statistics.median(r['wall_seconds'] for r in runs),
        median_render_paths_per_second=args.width**2 * args.samples / statistics.median(r['render_seconds'] for r in runs)))
pathlib.Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
