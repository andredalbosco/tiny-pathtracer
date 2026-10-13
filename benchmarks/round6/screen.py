import subprocess,pathlib,time,json,random,hashlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass6';rows=[]
configs=['baseline-stl','baseline-plain','control','inline_batch','inline_hot','old_ctors','unroll1','inline_unroll1','count_param']
def run(name,mode,repeat):
 f=out/(name+'-'+mode+'.f32');cmd=[str(out/name),'128','160',str(f),'--sampling',mode,'--threads','1','--runtime','omp','--batch','8','--refill','--cpus','8','--raw']
 start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,text=True,check=True);wall=time.perf_counter()-start
 r=json.loads(p.stderr);r.update(name=name,mode=mode,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(f.read_bytes()).hexdigest());rows.append(r);(out/'screen.json').write_text(json.dumps(rows,indent=2));print(name,mode,repeat,round(wall,4),flush=True)
for repeat in range(4):
 order=configs.copy();random.Random(839+repeat).shuffle(order)
 for name in order:run(name,'uniform',repeat)
rank=sorted(configs,key=lambda name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['repeat']>0))
selected=list(dict.fromkeys(['baseline-stl','baseline-plain','control']+rank[:3]));print('NEE selection',selected,flush=True)
for repeat in range(3):
 order=selected.copy();random.Random(367+repeat).shuffle(order)
 for name in order:run(name,'nee',repeat)
print('medians',{name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['mode']=='uniform' and r['repeat']>0) for name in configs},flush=True)
