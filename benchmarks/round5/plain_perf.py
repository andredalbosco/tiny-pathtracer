import subprocess,pathlib,json
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass5/perf';out.mkdir(exist_ok=True);rows=[]
for repeat in range(2):
 for mode in ['uniform','nee']:
  for label,binary in ([('stl','pathtracer_quality'),('plain','pathtracer_plain')] if repeat==0 else [('plain','pathtracer_plain'),('stl','pathtracer_quality')]):
   file=out/(label+'-'+mode+'-'+str(repeat)+'.csv')
   cmd=['sudo','-n','perf','stat','--no-big-num','-x,','-o',str(file),'-e','cpu_core/cycles/u,cpu_core/instructions/u,cpu_core/ref-cycles/u,cpu_core/branch-instructions/u,cpu_core/branch-misses/u','--','taskset','-c','8','/usr/sbin/runuser','-u','blacknoir','--',str(root/'build'/binary),'128','320',str(out/'render.hdr'),'--sampling',mode,'--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8']
   p=subprocess.run(cmd,capture_output=True,text=True,check=True);r=json.loads(p.stderr);counts={}
   for line in file.read_text().splitlines():
    f=line.split(',')
    if len(f)>4 and f[0].strip().replace('.','',1).isdigit():counts[f[2]]={'count':float(f[0]),'running_percent':float(f[4])}
   rows.append(dict(label=label,mode=mode,repeat=repeat,counts=counts,render=r,command=cmd));(out/'counts.json').write_text(json.dumps(rows,indent=2));print(label,mode,repeat,flush=True)
