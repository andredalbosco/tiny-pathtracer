"""Compare fixed plain renderer pixels and work with the strict quality reference."""
import array, hashlib, json, math, pathlib, subprocess
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'build/pass6'
checks=[]
keys=('completed_paths','rays','ray_slots','batch_calls','shadow_rays')
def run(binary,mode,width,samples,threads):
    path=out/(binary+'-'+mode+'-'+str(width)+'-'+str(threads)+'.f32')
    cmd=[str(root/'build'/binary),str(samples),str(width),str(path),'--sampling',mode,'--seed','42','--threads',str(threads),'--runtime','omp','--batch','8','--refill','--raw']
    p=subprocess.run(cmd,capture_output=True,text=True,check=True)
    return path.read_bytes(),json.loads(p.stderr)
for mode in ('uniform','cosine','nee','mis'):
    for width,samples in ((1,1),(9,3),(17,13),(32,8),(64,64)):
        reference,stats=run('pathtracer_quality_strict',mode,width,samples,1)
        for threads in (1,4,32):
            pixels,plain=run('pathtracer_plain_strict',mode,width,samples,threads)
            assert pixels==reference,(mode,width,threads,'pixels')
            assert all(plain.get(k,0)==stats.get(k,0) for k in keys),(mode,width,threads,'work')
            checks.append(dict(mode=mode,width=width,samples=samples,threads=threads,pixels_identical=True,work_identical=True,sha256=hashlib.sha256(pixels).hexdigest()))
for mode in ('uniform','nee'):
    reference,stats=run('pathtracer_quality_strict',mode,320,128,1)
    pixels,plain=run('pathtracer_plain_strict',mode,320,128,1)
    assert pixels==reference,(mode,'full pixels')
    assert all(plain.get(k,0)==stats.get(k,0) for k in keys),(mode,'full work')
    checks.append(dict(mode=mode,width=320,samples=128,threads=1,pixels_identical=True,work_identical=True,sha256=hashlib.sha256(pixels).hexdigest()))
fast=[]
for mode in ('uniform','nee'):
    stl=array.array('f');stl.frombytes((out/('stl-'+mode+'-P1.f32')).read_bytes())
    old=array.array('f');old.frombytes((out/('plain-old-'+mode+'-P1.f32')).read_bytes())
    fixed=array.array('f');fixed.frombytes((out/('plain-fixed-'+mode+'-P1.f32')).read_bytes())
    assert all(math.isfinite(v) and v>=0 for v in fixed)
    fast.append(dict(mode=mode,old_plain_identical=old==fixed,rmse_stl=math.sqrt(sum((a-b)**2 for a,b in zip(stl,fixed))/len(stl)),max_absolute_stl=max(abs(a-b) for a,b in zip(stl,fixed))))
report=dict(strict_checks=checks,fast=fast,binaries={name:hashlib.sha256((root/'build'/name).read_bytes()).hexdigest() for name in ('pathtracer_plain','pathtracer_plain_strict','pathtracer_quality_strict')})
(out/'validation.json').write_text(json.dumps(report,indent=2))
print('PASS:',len(checks),'strict pixel/work checks;',fast,flush=True)
