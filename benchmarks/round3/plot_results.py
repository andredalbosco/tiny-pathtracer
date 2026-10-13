#!/usr/bin/env python3
"""Plot the whole-process throughput medians and observed ranges."""
import json
import pathlib
import matplotlib.pyplot as plt
import numpy as np

folder=pathlib.Path(__file__).resolve().parent
rows=json.loads((folder/'final-results.json').read_text())['configurations']
fig,axes=plt.subplots(1,2,figsize=(10.5,4.2),gridspec_kw={'width_ratios':[1,2.7]})
labels=['Previous','Simple changes','Simple + PGO']
colors=['#969ba2','#3274b5','#27885e']
for ax,groups,names in [(axes[0],[0,1],['1 P','1 E']),(axes[1],[2,3,4,5,6],['8 P','16 E','24 physical','32 std','32 OpenMP'])]:
    x=np.arange(len(groups));width=.24
    for variant in range(3):
        selected=[rows[3*g+variant] for g in groups]
        y=np.array([r['median_paths_per_second']/1e6 for r in selected])
        low=np.array([r['runs'][0]['completed_paths']/max(s['wall_seconds'] for s in r['runs'])/1e6 for r in selected])
        high=np.array([r['runs'][0]['completed_paths']/min(s['wall_seconds'] for s in r['runs'])/1e6 for r in selected])
        ax.bar(x+(variant-1)*width,y,width,label=labels[variant],color=colors[variant],yerr=[y-low,high-y],capsize=2)
    ax.set_xticks(x,names);ax.set_ylim(bottom=0);ax.grid(axis='y',alpha=.18);ax.set_axisbelow(True)
    ax.spines[['top','right']].set_visible(False)
axes[0].set_ylabel('Million completed paths / second')
axes[0].set_title('Single core');axes[1].set_title('Parallel throughput')
axes[1].legend(loc='upper left',frameon=False,fontsize=9)
fig.suptitle('tiny-pathtracer: small source changes and GCC profile feedback',fontsize=13)
fig.text(.5,.015,'320×320, 128 spp · five interleaved runs · whole executable time · whiskers show observed ranges',ha='center',fontsize=9,color='#444444')
fig.tight_layout(rect=[0,.05,1,.93])
fig.savefig(folder/'throughput.svg');fig.savefig(folder/'throughput.png',dpi=180)

p=folder/'throughput.svg'
p.write_text('\n'.join(line.rstrip() for line in p.read_text().splitlines())+'\n')
