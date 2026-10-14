import hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
keys=['completed_paths','rays','ray_slots','batch_calls','shadow_rays']
configs=[(c+'-'+l,out/(c+'-'+l+'-strict')) for c in ['gcc','clang'] for l in ['fields','soa']]
def run(binary,mode,width,samples,threads):
    image=out/'strict.f32'
    cpus='8' if threads==1 else ','.join(map(str,[0,2,4,6] if threads==4 else range(32)))
    cmd=[str(binary),str(samples),str(width),str(image),'--sampling',mode,'--seed','42',
         '--threads',str(threads),'--runtime','omp','--batch','8','--refill','--cpus',cpus,'--raw']
    p=subprocess.run(cmd,capture_output=True,text=True,check=True)
    return image.read_bytes(),json.loads(p.stderr),cmd
for mode in ['uniform','nee']:
    reference,work,_=run(root/'build/pathtracer_plain_strict',mode,320,128,1)
    for name,binary in configs:
        data,stats,cmd=run(binary,mode,320,128,1)
        assert data==reference,(name,mode,'full pixels')
        assert all(stats.get(k,0)==work.get(k,0) for k in keys),(name,mode,'full work')
        rows.append(dict(name=name,mode=mode,width=320,samples=128,threads=1,
                         pixels_identical=True,work_identical=True,sha256=hashlib.sha256(data).hexdigest(),command=cmd))
    print('full strict',mode,'PASS',flush=True)
for mode in ['uniform','cosine','nee','mis']:
    reference,work,_=run(root/'build/pathtracer_plain_strict',mode,17,13,1)
    for name,binary in configs:
        for threads in [4,32]:
            data,stats,cmd=run(binary,mode,17,13,threads)
            assert data==reference,(name,mode,threads,'parallel pixels')
            assert all(stats.get(k,0)==work.get(k,0) for k in keys),(name,mode,threads,'parallel work')
            rows.append(dict(name=name,mode=mode,width=17,samples=13,threads=threads,
                             pixels_identical=True,work_identical=True,sha256=hashlib.sha256(data).hexdigest(),command=cmd))
(out/'validation-more.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS',len(rows),'additional strict comparisons',flush=True)
