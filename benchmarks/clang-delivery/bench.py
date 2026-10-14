import hashlib,json,pathlib,random,statistics,subprocess,time
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/clang-delivery';out.mkdir(exist_ok=True);rows=[]
for mode,layout in [('uniform','soa'),('nee','fields')]:
    configs=[('pathtracer_clang',root/'build/pathtracer_clang'),
             ('round8-'+layout,root/('build/pathtracer_triangles_clang_'+layout))]
    for scope,threads,cpus in [('P1',1,'8'),('all32',32,','.join(map(str,range(32))))]:
        for repeat in range(4):
            order=configs.copy();random.Random(914+repeat).shuffle(order)
            for name,binary in order:
                file=out/(name+'-'+mode+'-'+scope+'.f32')
                cmd=[str(binary),'128','320',str(file),'--sampling',mode,'--threads',str(threads),
                     '--cpus',cpus,'--seed','42','--raw']
                start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True)
                row=json.loads(p.stderr);row.update(name=name,mode=mode,scope=scope,repeat=repeat,
                     wall_seconds=time.perf_counter()-start,command=cmd,
                     binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                     image_sha256=hashlib.sha256(file.read_bytes()).hexdigest())
                rows.append(row);(out/'timings.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(mode,scope,{name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name
              and r['mode']==mode and r['scope']==scope and r['repeat']>0) for name,b in configs},flush=True)
