#!/usr/bin/env python3
"""Plot homogeneous speedup and calibrated heterogeneous throughput."""
import json,pathlib
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--output-dir', default=str(pathlib.Path(__file__).resolve().parent.parent / 'build'))
args=parser.parse_args()
folder=pathlib.Path(args.output_dir)
folder.mkdir(parents=True,exist_ok=True)
source=pathlib.Path(__file__).resolve().parent
data=json.loads((source/'computation-openmp.json').read_text())
extra=json.loads((source/'computation-openmp-extra.json').read_text())
rows={r['config']['name']:r for d in (data,extra) for r in d['configurations']}
fig,axes=plt.subplots(1,3,figsize=(15,4.5),layout='constrained')
colors={'std':'#2563eb','omp':'#d97706'}
for ax,kind,counts in [(axes[0],'P',[1,2,4,8]),(axes[1],'E',[1,2,4,8,16])]:
 ax.plot(counts,counts,color='#737373',linestyle='--',label='Linear ideal')
 for runtime in ['std','omp']:
  base=rows[f'1{kind}-{runtime}']['median_wall_seconds']
  values=[base/rows[f'{n}{kind}-{runtime}']['median_wall_seconds'] for n in counts]
  ax.plot(counts,values,'o-',color=colors[runtime],label='std::thread' if runtime=='std' else 'OpenMP',markersize=5)
 ax.set(xlabel=f'Physical {kind}-cores',ylabel=f'Speedup over one {kind}-core',title=f'{kind}-core scaling')
 ax.set_xticks(counts);ax.grid(alpha=.2);ax.set_ylim(bottom=0);ax.legend(frameon=False,fontsize=9)

ax=axes[2]
names=['8P','16E','24physical','32logical']
labels=['8 P','16 E','8 P + 16 E','All + SMT']
x=np.arange(len(names));width=.25
for k,runtime in enumerate(['std','omp']):
 rates=[rows[name+'-'+runtime]['median_paths_per_second']/1e6 for name in names]
 ax.bar(x+(k-.5)*width,rates,width,color=colors[runtime],label='std::thread' if runtime=='std' else 'OpenMP')
rp=rows['1P-std']['median_paths_per_second']/1e6
re=rows['1E-std']['median_paths_per_second']/1e6
pair=rows['1P-SMT-std']['median_paths_per_second']/1e6
ideal=[8*rp,16*re,8*rp+16*re,8*pair+16*re]
ax.scatter(x,ideal,marker='_',s=350,color='#404040',label='Isolated-core capacity model',zorder=5)
ax.set(xticks=x,xticklabels=labels,ylabel='Million completed paths/s',title='Aggregate throughput')
ax.tick_params(axis='x',labelsize=9);ax.grid(axis='y',alpha=.2);ax.set_ylim(bottom=0);ax.legend(frameon=False,fontsize=8,loc='upper left')
fig.suptitle('i9-14900K · 320×320 · 128 spp · five-run wall medians',fontsize=13)
fig.savefig(folder/'core-scaling.png',dpi=160)
fig.savefig(folder/'core-scaling.svg')
