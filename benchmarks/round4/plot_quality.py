#!/usr/bin/env python3
import argparse,json,pathlib,numpy as np,matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
parser=argparse.ArgumentParser();parser.add_argument('--data',required=True,type=pathlib.Path);parser.add_argument('--output',required=True,type=pathlib.Path);args=parser.parse_args();p=args.data;out=args.output;out.mkdir(exist_ok=True,parents=True)
report=json.loads((p/'analysis.json').read_text());runs=json.loads((p/'scored-runs.json').read_text());ref=np.load(p/'reference.npy');colors={'uniform':'#3573b9','cosine':'#3b9a68','nee':'#df8b2a','mis':'#965fba'}
plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(1,2,figsize=(10,4.2))
for mode in colors:
 rs=[r for r in report['quality'] if r['mode']==mode];actual=[r for r in runs if r['label'].startswith('candidate-') and r['mode']==mode]
 for ax,key,summarykey in [(axes[0],'mse','mean_mse'),(axes[1],'shaded_mse','mean_shaded_mse')]:
  ax.scatter([r['wall_seconds'] for r in actual],[r[key] for r in actual],s=8,color=colors[mode],alpha=.28)
  ax.plot([r['median_wall_seconds'] for r in rs],[r[summarykey] for r in rs],marker='o',label=mode,color=colors[mode],linewidth=1.8)
  ax.set(xscale='log',yscale='log',xlabel='Whole process time (seconds)',ylabel='Linear RGB mean squared error');ax.grid(alpha=.2)
axes[0].set_title('Full image');axes[1].set_title('Shaded pixels (99.35% of image)');axes[0].legend()
axes[0].axhline(report['reference_noise_mse'],color='gray',ls=':',label='Reference noise')
fig.suptitle('Cornell box · 128² · 32 / 128 / 512 spp · 8 independent seeds',fontsize=11);fig.tight_layout()
fig.savefig(out/'quality-time.png',dpi=170);fig.savefig(out/'quality-time.svg');plt.close(fig)
fig,axes=plt.subplots(2,4,figsize=(11,5.8));modes=['uniform','cosine','nee','reference']
def display(im):return np.clip(im/(1+im),0,1)**(1/2.2)
for col,mode in enumerate(modes):
 if mode=='reference':im=ref;title='Reference · 49,152 spp'
 else:
  row=next(r for r in runs if r['label']=='candidate-'+mode+'-128-0');im=np.fromfile(p/row['file'],dtype='<f4').reshape(128,128,3);title=mode+' · '+str(round(row['wall_seconds']*1000))+' ms'
 axes[0,col].imshow(display(im),interpolation='nearest');axes[0,col].set_title(title)
 error=np.sqrt(np.mean((im-ref)**2,axis=2));axes[1,col].imshow(error,cmap='magma',vmin=0,vmax=.15,interpolation='nearest')
 axes[1,col].set_title('RGB error, scale 0–0.15' if mode!='reference' else '')
 if mode=='reference':
  axes[1,col].clear();axes[1,col].text(.5,.5,'Estimated reference noise\nMSE = %.2g\nTwo independent halves'%report['reference_noise_mse'],ha='center',va='center',transform=axes[1,col].transAxes)
 for ax in axes[:,col]:ax.axis('off')
fig.suptitle('128 spp · same seed / display curve · different measured time\nError is measured before display mapping; images are not denoised',fontsize=11);fig.tight_layout();fig.subplots_adjust(hspace=.25);fig.savefig(out/'comparison.png',dpi=160);plt.close(fig)
