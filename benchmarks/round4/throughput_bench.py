import subprocess,time,json,random,hashlib,pathlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass4';rows=[]
configs=[('baseline-pgo','pass4-baseline-pgo','rays'),('cursor-pgo','pass4-cursor-pgo','rays'),('numerator-pgo','pass4-numerator-pgo','numerator')]
for scope,threads,cpus,runs in [('P1',1,'8',6),('std32',32,','.join(map(str,range(32))),6),('E1',1,'16',4)]:
 for repeat in range(runs):
  order=configs.copy();random.Random(33+repeat).shuffle(order)
  for label,binary,kernel in order:
   file=out/(label+'-'+scope+'.f32');cmd=[str(root/'build'/binary),'128','320',str(file),'--raw','--kernel',kernel,'--batch','8','--refill','--threads',str(threads),'--cpus',cpus]
   start=time.perf_counter();p=subprocess.run(cmd,capture_output=True,check=True,text=True);wall=time.perf_counter()-start
   row=json.loads(p.stderr);row.update(label=label,scope=scope,repeat=repeat,wall_seconds=wall,sha256=hashlib.sha256(file.read_bytes()).hexdigest());rows.append(row)
   (out/'throughput.json').write_text(json.dumps(rows,indent=2));print(label,scope,repeat,round(wall,4),flush=True)
(out/'throughput-metadata.json').write_text(json.dumps({b:hashlib.sha256((root/'build'/b).read_bytes()).hexdigest() for _,b,_ in configs},indent=2))
