#!/usr/bin/env python3
"""Generate independent linear-RGB measurements; analyze after timing."""
import pathlib,subprocess,time,json,random,hashlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass4/quality';out.mkdir(exist_ok=True)
binary=root/'build/pathtracer_quality_strict';rows=[]
def run(label,mode,spp,width,seed,threads=32,cpus=None,runtime='std',batch=8,refill=True,visibility='any'):
 f=out/(label+'.f32')
 cmd=[str(binary),str(spp),str(width),str(f),'--sampling',mode,'--seed',str(seed),'--batch',str(batch),'--threads',str(threads),'--runtime',runtime,'--visibility',visibility,'--raw']
 if refill:cmd+=['--refill']
 if cpus:cmd+=['--cpus',cpus]
 start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start
 row=json.loads(p.stderr);row.update(label=label,mode=mode,spp=spp,width=width,seed=seed,wall_seconds=wall,file=f.name,sha256=hashlib.sha256(f.read_bytes()).hexdigest());rows.append(row)
 (out/'runs.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(label,round(wall,4),flush=True);return row
# Deterministic scheduling parity, including lane reuse and partial final batches.
checks=[]
for mode in ['uniform','cosine','nee','mis']:
 ref=run('check-'+mode,mode,13,17,872,1)['sha256']
 for batch,refill,threads,runtime in [(1,False,1,'std'),(2,False,2,'std'),(4,False,4,'omp'),(8,False,3,'std'),(8,True,4,'omp')]:
  row=run('check-'+mode+'-'+str(batch)+'-'+str(refill)+'-'+runtime,mode,13,17,872,threads,runtime=runtime,batch=batch,refill=refill)
  assert row['sha256']==ref,(mode,batch,refill,runtime)
  checks.append(row['label'])
for mode in ['nee','mis']:
 a=run('check-any-'+mode,mode,32,32,999)['sha256'];b=run('check-nearest-'+mode,mode,32,32,999,visibility='nearest')['sha256'];assert a==b
binary=root/'build/pathtracer_quality'
# Warm all estimators, then randomize methods within each independent seed.
for mode in ['uniform','cosine','nee','mis']:run('warm-'+mode,mode,128,128,42)
for spp in [32,128,512]:
 for replicate in range(8):
  modes=['uniform','cosine','nee','mis'];random.Random(673+spp+replicate).shuffle(modes)
  for mode in modes:run('candidate-'+mode+'-'+str(spp)+'-'+str(replicate),mode,spp,128,101+replicate)
# Moderate-spp independent references, averaged in float64 by the analyzer.
for replicate in range(24):run('reference-'+str(replicate),'mis',2048,128,10001+replicate)
# Independent estimator means for a separate, smaller image, preventing self-reference.
for replicate in range(8):
 for mode in ['uniform','cosine','nee','mis']:run('agreement-'+mode+'-'+str(replicate),mode,4096,32,20001+replicate)
# Same-mode visibility and CPU scaling controls, interleaved whole-process timing.
physical=','.join(map(str,list(range(0,16,2))+list(range(16,32))))
configs=[('P1',1,'8','std','any'),('E1',1,'16','std','any'),('P8',8,','.join(map(str,range(0,16,2))),'std','any'),('E16',16,','.join(map(str,range(16,32))),'std','any'),('physical24',24,physical,'std','any'),('std32',32,','.join(map(str,range(32))),'std','any'),('omp32',32,','.join(map(str,range(32))),'omp','any'),('nearest32',32,','.join(map(str,range(32))),'std','nearest')]
for repeat in range(3):
 order=configs.copy();random.Random(644+repeat).shuffle(order)
 for label,threads,cpus,runtime,visibility in order:run('scaling-'+label+'-'+str(repeat),'mis',512,128,42,threads,cpus,runtime,visibility=visibility)
(out/'metadata.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),sources={f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in ['renderer_common.h','simple_kernels.h','render_driver.h','quality_sampling.h','pathtracer_quality.cpp']},checks=checks,reference_protocol='24 independent 2048-spp images; two halves of 12; float64 analysis; candidate seeds disjoint'),indent=2))
