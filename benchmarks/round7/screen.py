import subprocess,pathlib,time,json,random,hashlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';rows=[]
configs=[('gcc-stl',root/'build/pathtracer_quality'),('gcc-fixed',root/'build/pathtracer_plain')]+[(n,out/n) for n in ['gcc-plain-default','gcc-plain-pointer','gcc-plain-end','clang-plain','clang-stl']]
for mode in ['uniform','nee']:
 for repeat in range(4):
  order=configs.copy();random.Random(745+repeat).shuffle(order)
  for name,binary in order:
   image=out/(name+'-'+mode+'-screen.f32');cmd=[str(binary),'128','160',str(image),'--sampling',mode,'--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8','--raw']
   start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start
   r=json.loads(p.stderr);r.update(name=name,mode=mode,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(image.read_bytes()).hexdigest(),command=cmd);rows.append(r);(out/'screen.json').write_text(json.dumps(rows,indent=2));print(name,mode,repeat,round(wall,4),flush=True)
for mode in ['uniform','nee']:print(mode,{name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['mode']==mode and r['repeat']>0) for name,binary in configs},flush=True)
