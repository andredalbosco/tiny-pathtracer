import subprocess, pathlib, time, json, hashlib, random, statistics
root = pathlib.Path(__file__).resolve().parents[2]
out = root / 'build/pass6'
configs = [('stl', out/'baseline-stl'), ('plain-old', out/'baseline-plain'), ('plain-fixed', root/'build/pathtracer_plain')]
report = {'rows': [], 'binaries': {name: hashlib.sha256(binary.read_bytes()).hexdigest() for name,binary in configs}, 'source_sha256': hashlib.sha256((root/'pathtracer_plain.cpp').read_bytes()).hexdigest()}
for mode in ['uniform', 'nee']:
    for scope,threads,cpus in [('P1',1,'8'), ('all32',32,','.join(map(str,range(32))))]:
        for repeat in range(6):
            order = configs.copy()
            random.Random(603+repeat).shuffle(order)
            for name,binary in order:
                image = out / (name+'-'+mode+'-'+scope+'.f32')
                cmd = [str(binary),'128','320',str(image),'--sampling',mode,'--seed','42','--threads',str(threads),'--runtime','omp','--batch','8','--refill','--cpus',cpus,'--raw']
                start=time.perf_counter()
                p=subprocess.run(cmd,capture_output=True,text=True,check=True)
                elapsed=time.perf_counter()-start
                row=json.loads(p.stderr)
                row.update(name=name,mode=mode,scope=scope,repeat=repeat,wall_seconds=elapsed,sha256=hashlib.sha256(image.read_bytes()).hexdigest(),command=cmd)
                report['rows'].append(row)
                (out/'full-results.json').write_text(json.dumps(report,indent=2))
                print(name,mode,scope,repeat,round(elapsed,6),flush=True)
for mode in ['uniform','nee']:
    for scope in ['P1','all32']:
        print(mode,scope,{name:statistics.median(r['wall_seconds'] for r in report['rows'] if r['name']==name and r['mode']==mode and r['scope']==scope and r['repeat']>0) for name,binary in configs},flush=True)
