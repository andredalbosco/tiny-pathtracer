import hashlib,json,os,pathlib,random,statistics,subprocess,time
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/clang-simple';out.mkdir(exist_ok=True);rows=[]
if not (out/'previous-fast').exists():
    source=out/'previous.cpp'
    source.write_bytes(subprocess.check_output(['git','show','e90b8d6:pathtracer_clang.cpp'],cwd=root))
    subprocess.run(['clang++-20','-isystem','.','-O3','-std=c++17','-march=native',
                    '-ffp-contract=fast','-fopenmp',str(source),'-o',str(out/'previous-fast')],cwd=root,check=True)
for scope,threads,cpus in [('P1',1,'8'),('all32',32,','.join(map(str,range(32))))]:
    env=os.environ.copy();env.update(OMP_PROC_BIND='true',OMP_PLACES=','.join('{'+c+'}' for c in cpus.split(',')))
    for repeat in range(4):
        order=['previous','simple'];random.Random(321+repeat).shuffle(order)
        for name in order:
            image=out/(name+'-'+scope+'.f32')
            if name=='previous':
                binary=out/'previous-fast'
                args=['128','320',str(image),'--sampling','uniform','--threads',str(threads),'--cpus',cpus,'--raw']
            else:
                binary=root/'build/raw_clang';args=['128','320',str(threads)]
            cmd=['taskset','-c',cpus,str(binary)]+args
            with image.open('wb') as file:
                start=time.perf_counter();p=subprocess.run(cmd,stdout=file if name=='simple' else subprocess.DEVNULL,
                         stderr=subprocess.PIPE,env=env,check=True);wall=time.perf_counter()-start
            rows.append(dict(name=name,scope=scope,repeat=repeat,wall_seconds=wall,command=cmd,
                             omp_places=env['OMP_PLACES'],binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                             image_sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
            (out/'timings.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(scope,{n:statistics.median(r['wall_seconds'] for r in rows if r['name']==n and r['scope']==scope and r['repeat']>0)
                for n in ['previous','simple']},flush=True)
