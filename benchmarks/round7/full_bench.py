import subprocess,pathlib,time,json,random,hashlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';rows=[]
base=[('gcc-stl',root/'build/pathtracer_quality'),('gcc-plain',root/'build/pathtracer_plain'),('clang-stl',root/'build/pathtracer_quality_clang'),('clang-plain',root/'build/pathtracer_plain_clang')]
for mode in ['uniform','nee']:
 for scope,threads,cpus in [('P1',1,'8'),('all32',32,','.join(map(str,range(32))))]:
  configs=base+([('gcc-plain-libomp',out/'gcc-plain-libomp')] if scope=='all32' else [])
  for repeat in range(6):
   order=configs.copy();random.Random(762+repeat).shuffle(order)
   for name,binary in order:
    image=out/(name+'-'+mode+'-'+scope+'.f32');cmd=[str(binary),'128','320',str(image),'--sampling',mode,'--seed','42','--threads',str(threads),'--runtime','omp','--batch','8','--refill','--cpus',cpus,'--raw'];start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start;r=json.loads(p.stderr);r.update(name=name,mode=mode,scope=scope,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(image.read_bytes()).hexdigest(),command=cmd);rows.append(r);(out/'full-results.json').write_text(json.dumps(dict(rows=rows,binaries={name:hashlib.sha256(binary.read_bytes()).hexdigest() for name,binary in base+ [('gcc-plain-libomp',out/'gcc-plain-libomp')]}),indent=2));print(name,mode,scope,repeat,round(wall,5),flush=True)
for mode in ['uniform','nee']:
 for scope in ['P1','all32']:print(mode,scope,{name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['mode']==mode and r['scope']==scope and r['repeat']>0) for name in sorted({r['name'] for r in rows if r['scope']==scope})},flush=True)
