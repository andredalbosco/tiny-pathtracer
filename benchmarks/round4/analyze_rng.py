import pathlib,json,numpy as np,statistics,argparse
parser=argparse.ArgumentParser();parser.add_argument('--data',required=True,type=pathlib.Path);parser.add_argument('--reference',required=True,type=pathlib.Path);args=parser.parse_args();p=args.data;rows=json.loads((p/'runs.json').read_text());reference=np.load(args.reference)
result={'quality':[],'throughput':[]}
for mode in ['uniform','cosine','nee']:
 for rng in ['pcg32','xoroshiro128++','xoroshiro128**']:
  rs=[r for r in rows if r['label'].startswith('quality-') and r['mode']==mode and r['rng']==rng];ms=[]
  for r in rs:
   image=np.fromfile(p/r['file'],dtype='<f4').astype('f8').reshape(128,128,3);r['mse']=float(np.mean((image-reference)**2));r['mse_seconds']=r['mse']*r['wall_seconds']
  result['quality'].append(dict(rng=rng,mode=mode,mean_mse=float(np.mean([r['mse'] for r in rs])),median_wall_seconds=statistics.median(r['wall_seconds'] for r in rs),mean_mse_seconds=float(np.mean([r['mse_seconds'] for r in rs])),mse_range=[min(r['mse'] for r in rs),max(r['mse'] for r in rs)]))
for scope in ['P1','E1','all32']:
 for rng in ['pcg32','xoroshiro128++','xoroshiro128**']:
  rs=[r for r in rows if r['label'].startswith('throughput-'+scope) and r['rng']==rng and not r['label'].endswith('-0')]
  result['throughput'].append(dict(scope=scope,rng=rng,median_wall_seconds=statistics.median(r['wall_seconds'] for r in rs),wall_range=[min(r['wall_seconds'] for r in rs),max(r['wall_seconds'] for r in rs)],paths=rs[0]['completed_paths'],rays_range=[min(r['rays'] for r in rs),max(r['rays'] for r in rs)]))
(p/'analysis.json').write_text(json.dumps(result,indent=2));(p/'scored-runs.json').write_text(json.dumps(rows,indent=2));print(json.dumps(result,indent=2))
