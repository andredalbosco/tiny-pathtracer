from pathlib import Path
import subprocess,time,json
from array import array
import math
repo=Path(__file__).resolve().parents[1]
p=repo/'build/cloud-transport-checks';p.mkdir(parents=True,exist_ok=True)
grid=str(repo.parent/'volume-assets/wdas-cloud/wdas_cloud_quarter.nvdb')
cases=[('march-day', ['--mode','march','--samples','1']),
       ('reference-day-16',['--mode','reference','--samples','16']),
       ('reference-day-16-repeat',['--mode','reference','--samples','16']),
       ('reference-day-256',['--mode','reference','--samples','256']),
       ('reference-day-seed2',['--mode','reference','--samples','256','--seed','2']),
       ('reference-day-single',['--mode','reference','--samples','256','--bounces','1']),
       ('march-warm',['--mode','march','--samples','1','--preset','warm']),
       ('reference-warm',['--mode','reference','--samples','256','--preset','warm'])]
report={}
for name,args in cases:
 start=time.monotonic()
 r=subprocess.run([str(repo/'build/cloud_viewer'),grid,'--width','128','--neutral','--output',str(p/(name+'.png')),*args],cwd=repo,capture_output=True,text=True,timeout=90)
 (p/(name+'.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==0,(name,(r.stdout+r.stderr)[-2000:])
 a=array('f'); a.frombytes((p/(name+'.png.f32')).read_bytes())
 assert all(math.isfinite(v) and v>=0 for v in a),(name,'invalid RGB')
 report[name]={'seconds':round(time.monotonic()-start,3),'linear_mean':[sum(a[i::3])/(len(a)//3) for i in range(3)],'linear_max':max(a)}
 print(name,report[name],flush=True)
assert (p/'reference-day-16.png.f32').read_bytes()==(p/'reference-day-16-repeat.png.f32').read_bytes(), 'Identical seed did not reproduce linear RGB'
report['same_seed_repeatable']=True
def load(name):
 a=array('f');a.frombytes((p/(name+'.png.f32')).read_bytes())
 return [a[(y*128+x)*3+c] for y in range(32,63) for x in range(40,87) for c in range(3)]
low=load('reference-day-16'); high=load('reference-day-256'); independent=load('reference-day-seed2');single=load('reference-day-single')
e16=sum((a-b)**2 for a,b in zip(low,independent))/len(low)
e256=sum((a-b)**2 for a,b in zip(high,independent))/len(high)
report['convergence']={'mse_16_vs_independent_256':e16,'mse_256_vs_independent_256':e256,'noise_reduction':e16/e256}
report['multiple_scattering']={'single_mean':sum(single)/len(single),'multiple_mean':sum(high)/len(high)}
assert e256<e16*.35,report['convergence']
assert sum(high)>sum(single)*1.05,report['multiple_scattering']
(p/'render-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k in ['convergence','multiple_scattering']},indent=2),flush=True)
