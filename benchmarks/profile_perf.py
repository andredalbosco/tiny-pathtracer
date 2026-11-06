#!/usr/bin/env python3
"""Privileged perf observer; renderer itself runs as the ordinary user."""
import csv,hashlib,json,pathlib,statistics,subprocess,time
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build'/'perf';out.mkdir(parents=True,exist_ok=True)
p=[0,2,4,6,8,10,12,14];e=list(range(16,32))
configs=[
 dict(name='scalar-P1',kernel='scalar',cpus=[8]),
 dict(name='flat-P1',kernel='flat',cpus=[8]),
 dict(name='rays-fixed-P1',kernel='rays',batch=8,cpus=[8]),
 dict(name='rays-refill-P1',kernel='rays',batch=8,refill=True,cpus=[8]),
 dict(name='affine-P1',binary='pathtracer_advanced',kernel='affine',cpus=[8]),
 dict(name='culled-P1',binary='pathtracer_advanced',kernel='culled',cpus=[8]),
 dict(name='ray8-refill-P1',binary='pathtracer_advanced',kernel='ray8',batch=8,refill=True,cpus=[8]),
 dict(name='rays-refill-E1',kernel='rays',batch=8,refill=True,cpus=[16]),
 dict(name='rays-refill-P8',kernel='rays',batch=8,refill=True,cpus=p),
 dict(name='rays-refill-E16',kernel='rays',batch=8,refill=True,cpus=e),
 dict(name='rays-refill-physical24',kernel='rays',batch=8,refill=True,cpus=p+e),
 dict(name='rays-refill-logical32',kernel='rays',batch=8,refill=True,cpus=list(range(32))),
 dict(name='rays-refill-omp32',kernel='rays',batch=8,refill=True,runtime='omp',cpus=list(range(32))),
]
def command(c,profile=False):
 binary=c.get('binary','pathtracer_simple')
 if profile:binary='pathtracer_simple_profile'
 args=[str(root/'build'/binary),'128','320',str(out/'render.hdr'),'--kernel',c['kernel'],'--threads',str(len(c['cpus'])),'--cpus',','.join(map(str,c['cpus'])),'--batch',str(c.get('batch',1)),'--runtime',c.get('runtime','std')]
 if c.get('refill'):args.append('--refill')
 return args

def run_perf(name,args,options):
 # Files are created as the user before perf opens them, preserving ownership.
 file=out/(name+'.csv');file.touch()
 cmd=['sudo','-n','/usr/bin/perf','stat','--no-big-num','-x,','-o',str(file)]+options+['--','/usr/bin/taskset','-c',args[args.index('--cpus')+1],'/usr/sbin/runuser','-u','blacknoir','--']+args
 start=time.perf_counter();run=subprocess.run(cmd,capture_output=True,text=True)
 (out/(name+'-stderr.txt')).write_text(run.stderr)
 if run.returncode:raise RuntimeError(run.stderr)
 try:stats=json.loads(run.stderr.strip().splitlines()[-1])
 except (ValueError,IndexError):stats=None
 events=[]
 for row in csv.reader(file.read_text().splitlines()):
  if len(row)<3 or row[0].startswith('#'):continue
  value=None
  try:value=float(row[0].strip())
  except ValueError:pass
  events.append(dict(value=value,value_text=row[0].strip(),unit=row[1],event=row[2],running_ns=row[3] if len(row)>3 else None,running_percent=row[4] if len(row)>4 else None,extra=row[5:]))
 return dict(command=cmd,wall_seconds=time.perf_counter()-start,render=stats,events=events)

def basic_events(c):
 pmus=['cpu_atom'] if min(c['cpus'])>=16 else ['cpu_core'] if max(c['cpus'])<16 else ['cpu_core','cpu_atom']
 hw=[f'{p}/{e}/u' for p in pmus for e in ['cycles','instructions','branch-instructions','branch-misses']]
 return ','.join(hw+['task-clock','context-switches','cpu-migrations','page-faults'])
results=[[] for c in configs]
for c in configs:
 subprocess.run(command(c),capture_output=True,check=True)
 print('Warmup '+c['name'],flush=True)
for repeat in range(3):
 order=range(len(configs)) if repeat%2==0 else reversed(range(len(configs)))
 for i in order:
  r=run_perf(configs[i]['name']+'-'+str(repeat),command(configs[i]),['-e',basic_events(configs[i])])
  results[i].append(r)
  print(f"Hardware {repeat+1}/3 {configs[i]['name']} {r['wall_seconds']:.4f}s",flush=True)
report=dict(workload=dict(width=320,spp=128,max_visits=24),repetitions=3,scope='perf observer runs as root; renderer runs as blacknoir. Counters include startup, preparation, rendering and HDR writing. No machine policy/settings changed.',source_sha256={f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in ['experiments/render_driver.h','experiments/renderer_common.h','experiments/simple_kernels.h','experiments/advanced_kernels.h','experiments/pathtracer_simple.cpp','experiments/pathtracer_advanced.cpp']},configurations=[dict(config=c,runs=r) for c,r in zip(configs,results)])
(out/'hardware-stat.json').write_text(json.dumps(report,indent=2)+'\n')

# Separate, small PMU-specific groups keep diagnostics interpretable.
extra=[]
for c in [configs[0],configs[1],configs[3],configs[5],configs[6],configs[7]]:
 atom=c['cpus'][0]>=16;pmu='cpu_atom' if atom else 'cpu_core'
 events=[f'{pmu}/cycles/u',f'{pmu}/instructions/u',f'{pmu}/arith.fpdiv_active/u']
 if not atom:events += [f'{pmu}/mem_load_retired.l1_miss/u',f'{pmu}/mem_inst_retired.all_loads/u',f'{pmu}/mem_load_retired.l3_miss/u']
 else:events += [f'{pmu}/mem_load_uops_retired.l1_miss/u',f'{pmu}/mem_uops_retired.all_loads/u']
 r=run_perf(c['name']+'-specific',command(c),['-e',','.join(events)])
 extra.append(dict(config=c,result=r));print('Specific PMU '+c['name'],flush=True)
(out/'pmu-specific.json').write_text(json.dumps(extra,indent=2)+'\n')

samples=[]
for name,c,pmu in [('refill-P1',configs[3],'cpu_core'),('refill-E1',configs[7],'cpu_atom'),('scalar-P1',configs[0],'cpu_core')]:
 data=out/(name+'.data');data.touch()
 args=command(c,True)
 cmd=['sudo','-n','/usr/bin/perf','record','-o',str(data),'-e',f'{pmu}/cycles/u','-F','499','--call-graph','dwarf,8192','--','/usr/sbin/runuser','-u','blacknoir','--']+args
 run=subprocess.run(cmd,text=True,capture_output=True,check=True)
 (out/(name+'-record.txt')).write_text(run.stderr)
 for mode,options in [('self',['--no-children']),('inclusive',['--children'])]:
  display=subprocess.run(['perf','report','-i',str(data),'--stdio','--percent-limit','0.5','--sort','dso,symbol','-g','none']+options,capture_output=True,text=True,check=True)
  (out/(name+'-'+mode+'.txt')).write_text(display.stdout)
 samples.append(dict(name=name,command=cmd,record_summary=run.stderr,data_bytes=data.stat().st_size))
 print('Sample profile '+name,flush=True)
(out/'samples-summary.json').write_text(json.dumps(samples,indent=2)+'\n')
print('Hardware perf collection complete',flush=True)
