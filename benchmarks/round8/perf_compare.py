import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8/perf';out.mkdir(exist_ok=True);rows=[]
configs=[('clang-original',root/'build/pathtracer_plain_clang')]
configs += [('clang-'+l,root/'build/pass8'/('clang-'+l)) for l in ['fields','soa']]
events='cpu_core/cycles/u,cpu_core/instructions/u,cpu_core/ref-cycles/u,cpu_core/branch-instructions/u,cpu_core/branch-misses/u,cpu_core/mem_inst_retired.all_loads/u,cpu_core/mem_inst_retired.all_stores/u'
for repeat in range(2):
    for mode in ['uniform','nee']:
        for name,binary in (configs if repeat==0 else configs[::-1]):
            csv=out/(name+'-'+mode+'-'+str(repeat)+'.csv')
            cmd=['sudo','-n','perf','stat','--no-big-num','-x,','-o',str(csv),'-e',events,
                 '--','taskset','-c','8','/usr/sbin/runuser','-u','blacknoir','--',str(binary),
                 '128','320',str(out/'render.f32'),'--sampling',mode,'--threads','1',
                 '--runtime','omp','--batch','8','--refill','--cpus','8','--raw']
            p=subprocess.run(cmd,capture_output=True,text=True,check=True);counts={}
            for line in csv.read_text().splitlines():
                f=line.split(',')
                if len(f)>4 and f[0].strip().replace('.','',1).isdigit():
                    counts[f[2]]={'count':float(f[0]),'running_percent':float(f[4])}
            rows.append(dict(name=name,mode=mode,repeat=repeat,command=cmd,counts=counts,render=json.loads(p.stderr)))
            (out/'counts.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,mode,repeat,flush=True)
