import pathlib,json,subprocess,time
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass4/rng';rows=json.loads((out/'runs.json').read_text())
for replicate in range(8):
 for label,binary in [('pp','pathtracer_quality_xoroshiro_pp'),('ss','pathtracer_quality_xoroshiro_ss')]:
  f=out/('agreement-'+label+'-'+str(replicate)+'.f32');start=time.perf_counter();p=subprocess.run([str(root/'build'/binary),'4096','32',str(f),'--sampling','nee','--seed',str(20001+replicate),'--threads','32','--batch','8','--refill','--raw'],capture_output=True,text=True,check=True)
  r=json.loads(p.stderr);r.update(label=f.stem,width=32,spp=4096,file=f.name,wall_seconds=time.perf_counter()-start);rows.append(r)
(out/'runs.json').write_text(json.dumps(rows,indent=2));print('16 independent high-sample RNG agreement images complete')
