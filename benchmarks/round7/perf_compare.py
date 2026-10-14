import pathlib,subprocess,json
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7/perf';out.mkdir(exist_ok=True);rows=[]
configs=[('gcc-stl','pathtracer_quality'),('gcc-plain','pathtracer_plain'),('clang-stl','pathtracer_quality_clang'),('clang-plain','pathtracer_plain_clang')]
for repeat in range(2):
 for mode in ['uniform','nee']:
  for name,binary in (configs if repeat==0 else configs[::-1]):
   file=out/(name+'-'+mode+'-'+str(repeat)+'.csv');cmd=['sudo','-n','perf','stat','--no-big-num','-x,','-o',str(file),'-e','cpu_core/cycles/u,cpu_core/instructions/u,cpu_core/ref-cycles/u,cpu_core/branch-instructions/u,cpu_core/branch-misses/u,cpu_core/mem_inst_retired.all_loads/u,cpu_core/mem_inst_retired.all_stores/u','--','taskset','-c','8','/usr/sbin/runuser','-u','blacknoir','--',str(root/'build'/binary),'128','320',str(out/'render.f32'),'--sampling',mode,'--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8','--raw'];p=subprocess.run(cmd,capture_output=True,text=True,check=True);counts={}
   for line in file.read_text().splitlines():
    f=line.split(',')
    if len(f)>4 and f[0].strip().replace('.','',1).isdigit():counts[f[2]]={'count':float(f[0]),'running_percent':float(f[4])}
   rows.append(dict(name=name,mode=mode,repeat=repeat,command=cmd,counts=counts,render=json.loads(p.stderr)));(out/'counts.json').write_text(json.dumps(rows,indent=2));print(name,mode,repeat,flush=True)
