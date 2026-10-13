import subprocess,pathlib,time,json,random,hashlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass6';rows=[]
configs=['baseline-stl','baseline-plain','nojam','inline_nojam','batch_nojam']
for mode in ['uniform','nee']:
 for repeat in range(4):
  order=configs.copy();random.Random(273+repeat).shuffle(order)
  for name in order:
   f=out/(name+'-'+mode+'-nojam.f32');cmd=[str(out/name),'128','160',str(f),'--sampling',mode,'--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8','--raw']
   start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start
   r=json.loads(p.stderr);r.update(name=name,mode=mode,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(f.read_bytes()).hexdigest());rows.append(r);(out/'nojam-screen.json').write_text(json.dumps(rows,indent=2));print(name,mode,repeat,round(wall,4),flush=True)
for mode in ['uniform','nee']:print(mode,{name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['mode']==mode and r['repeat']>0) for name in configs})
