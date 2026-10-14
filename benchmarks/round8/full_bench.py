import hashlib,json,pathlib,random,statistics,subprocess,time

root=pathlib.Path(__file__).resolve().parents[2]
out=root/'build/pass8'
configs=[('gcc-original',root/'build/pathtracer_plain'),
         ('clang-original',root/'build/pathtracer_plain_clang')]
configs += [(c+'-'+l,out/(c+'-'+l)) for c in ['gcc','clang'] for l in ['fields','soa']]
rows=[]
for mode in ['uniform','nee']:
    for scope,threads,cpus in [('P1',1,'8'),('all32',32,','.join(map(str,range(32))))]:
        for repeat in range(6):
            order=configs.copy();random.Random(831+repeat).shuffle(order)
            for name,binary in order:
                image=out/(name+'-'+mode+'-'+scope+'.f32')
                cmd=[str(binary),'128','320',str(image),'--sampling',mode,'--seed','42',
                     '--threads',str(threads),'--runtime','omp','--batch','8','--refill',
                     '--cpus',cpus,'--raw']
                start=time.perf_counter()
                p=subprocess.run(cmd,capture_output=True,text=True,check=True)
                wall=time.perf_counter()-start
                row=json.loads(p.stderr)
                row.update(name=name,mode=mode,scope=scope,repeat=repeat,wall_seconds=wall,
                           sha256=hashlib.sha256(image.read_bytes()).hexdigest(),command=cmd,
                           binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
                rows.append(row);(out/'full-results.json').write_text(json.dumps(rows,indent=2)+'\n')
                print(name,mode,scope,repeat,round(wall,6),flush=True)
        print('medians',mode,scope,{name:statistics.median(r['wall_seconds'] for r in rows
              if r['mode']==mode and r['scope']==scope and r['name']==name and r['repeat']>0)
              for name,binary in configs},flush=True)
