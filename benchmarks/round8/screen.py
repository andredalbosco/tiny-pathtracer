import pathlib,subprocess,time,json,hashlib,random,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
configs=[('gcc-original',root/'build/pathtracer_plain'),('clang-original',root/'build/pathtracer_plain_clang')]+[(c+'-'+l,out/(c+'-'+l)) for c in ['gcc','clang'] for l in ['aos-control','split','fields','soa']]
for mode in ['uniform','nee']:
 for repeat in range(4):
  order=configs.copy();random.Random(825+repeat).shuffle(order)
  for name,binary in order:
   image=out/(name+'-'+mode+'-screen.f32');cmd=[str(binary),'128','160',str(image),'--sampling',mode,'--seed','42','--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8','--raw'];start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start;r=json.loads(p.stderr);r.update(name=name,mode=mode,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(image.read_bytes()).hexdigest(),command=cmd);rows.append(r);(out/'screen.json').write_text(json.dumps(rows,indent=2));print(name,mode,repeat,round(wall,5),flush=True)
for mode in ['uniform','nee']:print(mode,{name:statistics.median(r['wall_seconds'] for r in rows if r['mode']==mode and r['name']==name and r['repeat']>0) for name,binary in configs},flush=True)
