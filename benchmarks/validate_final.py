#!/usr/bin/env python3
"""Strict parity, fast rounding differences, tails, and scheduling checks."""
import array
import hashlib
import json
import math
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
out = root/'build'/'pass3'
results = []
def render(binary,samples,width,batch,refill,runtime):
    path=out/(binary+'-check.f32')
    command=[str(root/'build'/binary),str(samples),str(width),str(path),'--kernel','rays','--batch',str(batch),'--threads','3','--runtime',runtime,'--raw']
    if refill:command.append('--refill')
    run=subprocess.run(command,capture_output=True,text=True,check=True)
    return path.read_bytes(),json.loads(run.stderr)

cases=[(1,1,1,True),(7,7,2,True),(9,17,4,True),(19,40,8,True),(33,65,8,True),(128,320,8,True),(9,17,8,False)]
for samples,width,batch,refill in cases:
    for strict in [False,True]:
        suffix='-strict' if strict else ''
        reference,stats=render('pass3-baseline'+suffix,samples,width,batch,refill,'std')
        a=array.array('f',reference)
        for name in ['pass3-simple','pass3-final-pgo']:
            outputs=[]
            for runtime in ['std','omp']:
                output,other=render(name+suffix,samples,width,batch,refill,runtime)
                b=array.array('f',output)
                assert len(b)==len(a) and all(math.isfinite(x) and x>=0 for x in b)
                record=dict(binary=name+suffix,runtime=runtime,samples=samples,width=width,batch=batch,refill=refill,
                            exact=output==reference,ray_match=other['rays']==stats['rays'],rays=other['rays'],reference_rays=stats['rays'],
                            changed_floats=sum(x!=y for x,y in zip(a,b)),rmse=math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a)),
                            max_abs=max(abs(x-y) for x,y in zip(a,b)),sha256=hashlib.sha256(output).hexdigest())
                if strict:assert record['exact'] and record['ray_match'],record
                results.append(record);outputs.append(output)
            assert outputs[0]==outputs[1],('runtime parity',name,samples,width,strict)
(out/'final-validation.json').write_text(json.dumps(results,indent=2)+'\n')
print('Validated',len(results),'comparisons, strict exactness and std/OpenMP identity; fast differences recorded.')
