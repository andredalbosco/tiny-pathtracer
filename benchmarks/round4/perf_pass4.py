#!/usr/bin/env python3
"""Root perf observes taskset-pinned ordinary-user renderers."""
import subprocess,pathlib,json,random
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass4/perf';out.mkdir(exist_ok=True);rows=[]
def run(label,binary,mode,pmu,cpu,repeat,details=False,spp=128,width=320,kernel='rays'):
 file=out/(label+'-'+pmu+'-'+str(repeat)+('-details' if details else '')+'.csv')
 events=[pmu+'/'+name+'/u' for name in (['cycles','instructions','arith.fpdiv_active','mem_load_retired.l1_miss','mem_inst_retired.all_loads'] if details else ['cycles','instructions','ref-cycles','branch-instructions','branch-misses'])]
 cmd=['sudo','-n','perf','stat','--no-big-num','-x,','-o',str(file),'-e',','.join(events),'--','taskset','-c',str(cpu),'/usr/sbin/runuser','-u','blacknoir','--',str(root/'build'/binary),str(spp),str(width),str(out/'render.hdr'),'--kernel',kernel,'--batch','8','--refill','--cpus',str(cpu)]
 if mode:cmd+=['--sampling',mode]
 p=subprocess.run(cmd,text=True,capture_output=True,check=True)
 stats=json.loads(p.stderr);counts={}
 for line in file.read_text().splitlines():
  fields=line.split(',')
  if len(fields)>4 and fields[0].strip().replace('.','',1).isdigit():counts[fields[2]]=dict(count=float(fields[0]),enabled=fields[4])
 rows.append(dict(label=label,pmu=pmu,cpu=cpu,repeat=repeat,details=details,command=cmd,render=stats,counts=counts));(out/'counts.json').write_text(json.dumps(rows,indent=2));print(label,pmu,repeat,'details' if details else '',flush=True)
for pmu,cpu in [('cpu_core',8),('cpu_atom',16)]:
 for repeat in range(3 if pmu=='cpu_core' else 1):
  order=[('baseline-pgo','pass4-baseline-pgo','rays'),('cursor-pgo','pass4-cursor-pgo','rays'),('numerator-pgo','pass4-numerator-pgo','numerator')];random.Random(73+repeat).shuffle(order)
  for label,binary,kernel in order:run(label,binary,None,pmu,cpu,repeat,kernel=kernel)
for label,binary,kernel in [('baseline-pgo','pass4-baseline-pgo','rays'),('numerator-pgo','pass4-numerator-pgo','numerator')]:run(label,binary,None,'cpu_core',8,0,True,kernel=kernel)
for repeat in range(3):
 modes=['uniform','cosine','nee','mis'];random.Random(41+repeat).shuffle(modes)
 for mode in modes:run('sampling-'+mode,'pathtracer_quality',mode,'cpu_core',8,repeat,spp=512,width=128)
