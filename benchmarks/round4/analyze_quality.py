#!/usr/bin/env python3
import pathlib,json,numpy as np,statistics,argparse
parser=argparse.ArgumentParser();parser.add_argument('--data',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parent/'quality');args=parser.parse_args();p=args.data;rows=json.loads((p/'runs.json').read_text())
def image(r):return np.fromfile(p/r['file'],dtype='<f4').astype(np.float64).reshape(r['width'],r['width'],3)
refs=[r for r in rows if r['label'].startswith('reference-')]
a=np.mean([image(r) for r in refs[:12]],axis=0);b=np.mean([image(r) for r in refs[12:]],axis=0);reference=(a+b)*.5
floor=float(np.mean((a-b)**2)/4);np.save(p/'reference.npy',reference)
mask=np.max(reference,axis=2)<2
summary=[]
for spp in [32,128,512]:
 for mode in ['uniform','cosine','nee','mis']:
  selected=[r for r in rows if r['label'].startswith('candidate-') and r['spp']==spp and r['mode']==mode]
  for r in selected:
   d=image(r)-reference;r['mse']=float(np.mean(d*d));r['shaded_mse']=float(np.mean(d[mask]**2));r['mse_seconds']=r['mse']*r['wall_seconds']
  summary.append(dict(mode=mode,spp=spp,mean_mse=float(np.mean([r['mse'] for r in selected])),mean_shaded_mse=float(np.mean([r['shaded_mse'] for r in selected])),median_wall_seconds=statistics.median(r['wall_seconds'] for r in selected),mean_mse_seconds=float(np.mean([r['mse_seconds'] for r in selected])),mse_range=[min(r['mse'] for r in selected),max(r['mse'] for r in selected)],wall_range=[min(r['wall_seconds'] for r in selected),max(r['wall_seconds'] for r in selected)]))
agreements=[]
mis=[image(r) for r in rows if r['label'].startswith('agreement-mis-')]
for mode in ['uniform','cosine','nee']:
 other=[image(r) for r in rows if r['label'].startswith('agreement-'+mode+'-')]
 diff=np.asarray(other)-np.asarray(mis);mean=np.mean(diff,axis=0);noise=float(np.mean(np.var(diff,axis=0,ddof=1))/len(diff))
 means=diff.mean(axis=(1,2,3));std=float(means.std(ddof=1)/np.sqrt(len(means)))
 agreements.append(dict(mode=mode,mean_difference_mse=float(np.mean(mean**2)),estimated_noise_mse=noise,ratio=float(np.mean(mean**2)/noise),mean_brightness_difference=float(means.mean()),brightness_standard_error=std,brightness_z=float(means.mean()/std)))
scaling=[]
for label in ['P1','E1','P8','E16','physical24','std32','omp32','nearest32']:
 rs=[r for r in rows if r['label'].startswith('scaling-'+label+'-')];scaling.append(dict(label=label,median_wall_seconds=statistics.median(r['wall_seconds'] for r in rs),wall_range=[min(r['wall_seconds'] for r in rs),max(r['wall_seconds'] for r in rs)]))
report=dict(reference_spp=49152,reference_noise_mse=floor,shaded_fraction=float(mask.mean()),quality=summary,agreement=agreements,scaling=scaling)
(p/'analysis.json').write_text(json.dumps(report,indent=2));(p/'scored-runs.json').write_text(json.dumps(rows,indent=2));print(json.dumps(report,indent=2))
