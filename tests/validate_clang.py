"""Check the consolidated renderer against independent frozen implementations."""
import array,hashlib,json,math,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build/clang-delivery';out.mkdir(exist_ok=True);rows=[]
keys=['completed_paths','rays','ray_slots','batch_calls','shadow_rays']
def run(binary,mode,width,samples,seed,threads):
    file=out/'check.f32'
    cmd=[str(root/'build'/binary),str(samples),str(width),str(file),'--sampling',mode,
         '--seed',str(seed),'--threads',str(threads),'--raw']
    p=subprocess.run(cmd,capture_output=True,text=True,check=True)
    return file.read_bytes(),json.loads(p.stderr)
for mode in ['uniform','cosine','nee','mis']:
    for width,samples,seed in [(1,1,42),(9,3,42),(17,13,42),(33,9,123456789),(64,32,0)]:
        ref,work=run('pathtracer_plain_clang_strict',mode,width,samples,seed,1)
        for threads in [1,4,32]:
            data,stats=run('pathtracer_clang_strict',mode,width,samples,seed,threads)
            assert data==ref,(mode,width,threads,'pixels')
            assert all(stats.get(k,0)==work.get(k,0) for k in keys),(mode,width,threads,'work')
            rows.append(dict(kind='strict',mode=mode,width=width,samples=samples,seed=seed,threads=threads,
                             identical=True,sha256=hashlib.sha256(data).hexdigest()))
for mode in ['uniform','nee']:
    ref,work=run('pathtracer_plain_clang_strict',mode,320,128,42,1)
    data,stats=run('pathtracer_clang_strict',mode,320,128,42,1)
    assert data==ref,(mode,'full pixels')
    assert all(stats.get(k,0)==work.get(k,0) for k in keys),(mode,'full work')
    rows.append(dict(kind='strict-full',mode=mode,width=320,samples=128,seed=42,threads=1,
                     identical=True,sha256=hashlib.sha256(data).hexdigest()))
    print('full strict',mode,'PASS',flush=True)
for mode,layout in [('uniform','soa'),('nee','fields')]:
    ref,work=run('pathtracer_triangles_clang_'+layout,mode,64,32,42,1)
    for threads in [1,32]:
        data,stats=run('pathtracer_clang',mode,64,32,42,threads)
        a=array.array('f');a.frombytes(ref);b=array.array('f');b.frombytes(data)
        assert len(a)==len(b) and all(math.isfinite(v) for v in b)
        rmse=math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a))
        rows.append(dict(kind='fast',mode=mode,reference=layout,threads=threads,rmse=rmse,
                         max_absolute=max(abs(x-y) for x,y in zip(a,b)),sha256=hashlib.sha256(data).hexdigest()))
(out/'validation.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS',sum(r['kind'].startswith('strict') for r in rows),'strict pixel/work checks;',
      sum(r['kind']=='fast' for r in rows),'fast comparisons',flush=True)
