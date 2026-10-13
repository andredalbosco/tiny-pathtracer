#!/usr/bin/env python3
"""Root perf observer, ordinary-user renderer, separate P/E PMUs."""
import csv
import hashlib
import json
import pathlib
import subprocess
import time

root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build'/'pass3'/'perf'
out.mkdir(exist_ok=True)
configs=[dict(binary=b,cpu=cpu,pmu=pmu) for cpu,pmu in [(8,'cpu_core'),(16,'cpu_atom')]
         for b in ['pass3-baseline','pass3-simple','pass3-final-pgo']]

def launcher(c,binary=None):
    return ['/usr/bin/taskset','-c',str(c['cpu']),'/usr/sbin/runuser','-u','blacknoir','--',
            str(root/'build'/(binary or c['binary'])),'128','320',str(out/'render.hdr'),
            '--kernel','rays','--batch','8','--refill','--cpus',str(c['cpu'])]

def stat(name,c,options):
    path=out/(name+'.csv');path.touch()
    command=['sudo','-n','perf','stat','--no-big-num','-x,','-o',str(path)]+options+['--']+launcher(c)
    start=time.perf_counter();run=subprocess.run(command,capture_output=True,text=True,check=True)
    (out/(name+'-stderr.txt')).write_text(run.stderr)
    rows=[]
    for row in csv.reader(path.read_text().splitlines()):
        if len(row)<3 or row[0].startswith('#'):continue
        try:value=float(row[0])
        except ValueError:value=None
        rows.append(dict(value=value,event=row[2],unit=row[1],running_ns=row[3],running_percent=row[4],extra=row[5:]))
    return dict(command=command,wall_seconds=time.perf_counter()-start,render=json.loads(run.stderr.strip().splitlines()[-1]),events=rows)

runs=[[] for c in configs]
for repeat in range(3):
    order=range(len(configs)) if repeat%2==0 else reversed(range(len(configs)))
    for i in order:
        c=configs[i]
        events=','.join(c['pmu']+'/'+e+'/u' for e in ['cycles','instructions','ref-cycles','branch-instructions','branch-misses'])
        runs[i].append(stat(c['binary']+'-'+c['pmu']+'-'+str(repeat),c,['-e',events]))
        print('Perf '+str(repeat+1)+'/3 '+c['binary']+' '+c['pmu'],flush=True)
(out/'stat.json').write_text(json.dumps(dict(scope='user hardware counts, observer root, renderer blacknoir; includes launcher, startup, tracing and output',configurations=[dict(config=c,runs=r) for c,r in zip(configs,runs)]),indent=2)+'\n')

details=[]
for c in [configs[0],configs[2],configs[5]]:
    pmu=c['pmu'];events=['cycles','instructions','arith.fpdiv_active']
    events+=['mem_load_retired.l1_miss','mem_inst_retired.all_loads'] if pmu=='cpu_core' else ['mem_load_uops_retired.l1_miss','mem_uops_retired.all_loads']
    details.append(dict(config=c,result=stat(c['binary']+'-'+pmu+'-loads',c,['-e',','.join(pmu+'/'+e+'/u' for e in events)])))
for c in [configs[0],configs[2]]:
    details.append(dict(config=c,result=stat(c['binary']+'-topdown',c,['--cputype','core','--all-user','-M','TopdownL1'])))
(out/'details.json').write_text(json.dumps(details,indent=2)+'\n')

samples=[]
for c,binary in [(configs[0],'pathtracer_simple_profile'),(configs[2],'pass3-final-pgo-profile'),(configs[5],'pass3-final-pgo-profile')]:
    name=c['binary']+'-'+c['pmu'];data=out/(name+'.data');data.touch()
    command=['sudo','-n','perf','record','-o',str(data),'-e',c['pmu']+'/cycles/u','-F','499','--call-graph','dwarf,8192','--']+launcher(c,binary)
    run=subprocess.run(command,capture_output=True,text=True,check=True)
    for mode,flag in [('self','--no-children'),('inclusive','--children')]:
        report=subprocess.check_output(['perf','report','-i',str(data),'--stdio','--percent-limit','0.5','--sort','dso,symbol','-g','none',flag],text=True)
        (out/(name+'-'+mode+'.txt')).write_text('\n'.join(line.rstrip() for line in report.splitlines())+'\n')
    samples.append(dict(config=c,command=command,record_summary=run.stderr))
    print('Sample '+name,flush=True)
(out/'samples.json').write_text(json.dumps(samples,indent=2)+'\n')
print('Perf complete',flush=True)
