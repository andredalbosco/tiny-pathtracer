import pathlib,subprocess,json,hashlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
keys=['completed_paths','rays','ray_slots','batch_calls','shadow_rays']
def run(binary,mode,width,samples,threads=1):
 f=out/'check.f32';cmd=[str(binary),str(samples),str(width),str(f),'--sampling',mode,'--seed','42','--threads',str(threads),'--runtime','omp','--batch','8','--refill','--raw'];p=subprocess.run(cmd,capture_output=True,text=True,check=True);return f.read_bytes(),json.loads(p.stderr)
for mode in ['uniform','cosine','nee','mis']:
 for width,samples in [(1,1),(9,3),(17,13),(32,8),(64,64)]:
  reference,work=run(root/'build/pathtracer_plain_strict',mode,width,samples)
  for compiler in ['gcc','clang']:
   for layout in ['aos-control','split','fields','soa']:
    name=compiler+'-'+layout+'-strict';data,stats=run(out/name,mode,width,samples);assert data==reference,(name,mode,width,'pixels');assert all(stats.get(k,0)==work.get(k,0) for k in keys),(name,mode,width,'work');rows.append(dict(name=name,mode=mode,width=width,samples=samples,pixels_identical=True,work_identical=True,sha256=hashlib.sha256(data).hexdigest()))
(out/'validation.json').write_text(json.dumps(rows,indent=2));print('PASS',len(rows),'strict pixel/work comparisons',flush=True)
