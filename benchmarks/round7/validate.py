import array,hashlib,json,math,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';checks=[]
keys=['completed_paths','rays','ray_slots','batch_calls','shadow_rays']
def run(binary,mode,width,samples,threads=1):
 path=out/(binary+'-'+mode+'-'+str(width)+'-'+str(threads)+'.f32');cmd=[str(root/'build'/binary),str(samples),str(width),str(path),'--sampling',mode,'--threads',str(threads),'--runtime','omp','--batch','8','--refill','--raw'];p=subprocess.run(cmd,capture_output=True,text=True,check=True);return path.read_bytes(),json.loads(p.stderr)
def difference(a,b):
 aa=array.array('f');bb=array.array('f');aa.frombytes(a);bb.frombytes(b);assert len(aa)==len(bb)
 return dict(identical=a==b,rmse=math.sqrt(sum((x-y)**2 for x,y in zip(aa,bb))/len(aa)),max_absolute=max(abs(x-y) for x,y in zip(aa,bb)),finite_nonnegative=all(math.isfinite(x) and x>=0 for x in bb))
for mode in ['uniform','cosine','nee','mis']:
 for width,samples in [(1,1),(9,3),(17,13),(32,8),(64,64)]:
  reference,work=run('pathtracer_quality_strict',mode,width,samples)
  for name in ['pathtracer_plain_clang_strict','pathtracer_quality_clang_strict']:
   one,stats=run(name,mode,width,samples)
   row=dict(mode=mode,width=width,samples=samples,binary=name,**difference(reference,one),work_identical=all(work.get(k,0)==stats.get(k,0) for k in keys));assert row['finite_nonnegative'] and row['identical'] and row['work_identical'],row;checks.append(row)
   for threads in [4,32]:
    pixels,par=run(name,mode,width,samples,threads);assert pixels==one,(name,mode,width,threads,'schedule');assert all(par.get(k,0)==stats.get(k,0) for k in keys);checks.append(dict(mode=mode,width=width,samples=samples,binary=name,threads=threads,scheduling_pixels_identical=True,scheduling_work_identical=True))
for mode in ['uniform','nee']:
 reference,work=run('pathtracer_quality_strict',mode,320,128)
 for name in ['pathtracer_plain_clang_strict','pathtracer_quality_clang_strict']:
  pixels,stats=run(name,mode,320,128);row=dict(mode=mode,width=320,samples=128,binary=name,**difference(reference,pixels),work_identical=all(work.get(k,0)==stats.get(k,0) for k in keys));assert row['finite_nonnegative'] and row['identical'] and row['work_identical'],row;checks.append(row)
(out/'validation.json').write_text(json.dumps(checks,indent=2));cross=[r for r in checks if 'identical' in r];print('Strict:',sum(r['identical'] for r in cross),'/',len(cross),'cross-compiler exact; 80 scheduling checks; max RMSE',max(r['rmse'] for r in cross),flush=True)
