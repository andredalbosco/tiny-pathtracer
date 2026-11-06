#!/usr/bin/env python3
"""Train and build GCC PGO without changing the renderer's arithmetic flags."""
import argparse
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='c++')
parser.add_argument('--flags', default='-isystem . -isystem external/stb -Iexperiments -O3 -std=c++17 -Wall -Wextra -Wpedantic -march=native -ffp-contract=fast -pthread -fopenmp')
parser.add_argument('--binary', default='pathtracer_simple_pgo')
parser.add_argument('--cpu', type=int)
parser.add_argument('--source', default='experiments/pathtracer_simple.cpp')
parser.add_argument('--kernel', default='rays')
parser.add_argument('--sampling', choices=['uniform','cosine','nee','mis'])
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
build = root/'build'
build.mkdir(exist_ok=True)
binary = build/args.binary
profiles = pathlib.Path(tempfile.mkdtemp(prefix=args.binary+'-profile-', dir=build))
flags = shlex.split(args.flags)
compile_args = shlex.split(args.cxx)+flags+[args.source,'-o',str(binary)]
generate = compile_args+['-fprofile-generate='+str(profiles),'-fprofile-update=atomic']
subprocess.run(generate,cwd=root,check=True)
available = sorted(os.sched_getaffinity(0))
cpu = args.cpu if args.cpu is not None else (8 if 8 in available else available[0])
workloads = [(128,320,1,'std'),(8,32,min(32,len(available)),'std')]
if '-fopenmp' in flags:workloads.append((8,32,min(32,len(available)),'omp'))
training = []
for samples,width,threads,runtime in workloads:
    command = [str(binary),str(samples),str(width),str(build/'pgo-training.hdr'),
               '--kernel',args.kernel,'--batch','8','--refill','--threads',str(threads),'--runtime',runtime]
    if args.sampling:command += ['--sampling',args.sampling]
    if threads == 1:command += ['--cpus',str(cpu)]
    run = subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    training.append(dict(command=command,statistics=json.loads(run.stderr)))
    print('PGO training: '+str(samples)+' spp / '+str(width)+' / '+runtime,flush=True)
use = compile_args+['-fprofile-use='+str(profiles),'-Werror=missing-profile','-Werror=coverage-mismatch']
run = subprocess.run(use,cwd=root,text=True,capture_output=True)
(build/(args.binary+'-build.log')).write_text(run.stderr)
if run.returncode:raise RuntimeError(run.stderr)
metadata = dict(compiler=subprocess.check_output(shlex.split(args.cxx)+['--version'],text=True).splitlines()[0],
                generate=generate,use=use,training=training,diagnostics=run.stderr,
                source_sha256={f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in set(['experiments/renderer_common.h','experiments/render_driver.h','experiments/simple_kernels.h',args.source]+(['experiments/experimental_kernels.h'] if args.source=='experiments/pathtracer_explore.cpp' else [])+(['experiments/quality_sampling.h'] if args.source=='experiments/pathtracer_quality.cpp' else []))},
                binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                profiles={str(p.relative_to(build)):hashlib.sha256(p.read_bytes()).hexdigest() for p in profiles.rglob('*.gcda')})
(build/(args.binary+'-metadata.json')).write_text(json.dumps(metadata,indent=2)+'\n')
print('PGO binary: '+str(binary),flush=True)
