#!/usr/bin/env python3
"""Time identical seeded renders on one CPU and check their HDR bytes."""
import argparse
import hashlib
import json
import statistics
import subprocess
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spp", type=int, default=128)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--cpu", type=int, default=8)
    parser.add_argument("--generic", action="store_true", help="also time plain -O3 builds")
    args = parser.parse_args()
    if min(args.spp, args.width, args.runs) < 1:
        parser.error("spp, width, and runs must be positive")
    variants = ["pathtracer", "pathtracer_optim", "pathtracer_simd"]
    if args.generic:
        variants += ["pathtracer_o3", "pathtracer_optim_o3"]
    timings = {name: [] for name in variants}
    hashes = {}

    def run(name, spp, width):
        output = Path("build") / f"{name}-benchmark.hdr"
        start = time.perf_counter()
        subprocess.run(["taskset", "-c", str(args.cpu), f"./build/{name}",
                        str(spp), str(width), str(output)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return time.perf_counter() - start, hashlib.sha256(output.read_bytes()).hexdigest()

    for name in variants:
        run(name, args.spp, args.width)  # Full workload warmup, excluded from results.

    for trial in range(args.runs):
        order = variants[trial % len(variants):] + variants[:trial % len(variants)]
        for name in order:
            elapsed, digest = run(name, args.spp, args.width)
            timings[name].append(elapsed)
            hashes[name] = digest
            if digest != hashes[variants[0]]:
                raise RuntimeError(f"HDR output differs for {name}")
            print(f"Run {trial + 1}/{args.runs}: {name} {elapsed:.3f} s", flush=True)

    results = {"settings": vars(args), "timings_seconds": timings,
               "hdr_sha256": hashes,
               "median_seconds": {name: statistics.median(times)
                                  for name, times in timings.items()}}
    destination = Path("build") / f"benchmark-native-{args.width}-{args.spp}.json"
    destination.write_text(json.dumps(results, indent=2) + "\n")
    scalar = results["median_seconds"]["pathtracer"]
    simd = results["median_seconds"]["pathtracer_simd"]
    print(f"SIMD speedup over scalar -O3: {scalar / simd:.2f}x")
    print(f"Identical HDR output; results saved to {destination}")


if __name__ == "__main__":
    main()
