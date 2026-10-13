#!/usr/bin/env python3
"""Compare frozen and candidate float outputs; timings here are not scores."""
import hashlib
import array
import math
import json
import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[3]
out = root / 'build' / 'pass3'
checks = []
def render(binary, samples, width, strict=False, runtime='std', threads=3, kernel='rays', batch=8, refill=True):
    path = out / (binary + '-parity.f32')
    args = [str(root/'build'/binary),str(samples),str(width),str(path),'--raw','--kernel',kernel,'--batch',str(batch),'--threads',str(threads),'--runtime',runtime]
    if refill:args.append('--refill')
    run = subprocess.run(args,text=True,capture_output=True,check=True)
    return path.read_bytes(),json.loads(run.stderr)

for samples,width in [(1,1),(7,7),(9,17),(19,40),(33,65),(128,320)]:
    for strict in [False,True]:
        baseline='pass3-baseline'+('-strict' if strict else '')
        ref,stats=render(baseline,samples,width)
        for candidate,kernel in [('pass3-sincos'+('-strict' if strict else ''),'rays'),('pass3-columns'+('-strict' if strict else ''),'columns')]:
            for runtime in ['std','omp']:
                result,other=render(candidate,samples,width,runtime=runtime,kernel=kernel)
                entry=dict(binary=candidate,kernel=kernel,runtime=runtime,samples=samples,width=width,exact=result==ref,
                           ray_match=other['rays']==stats['rays'],sha256=hashlib.sha256(result).hexdigest())
                a,b=array.array('f',ref),array.array('f',result)
                entry['changed_floats']=sum(x!=y for x,y in zip(a,b))
                entry['rmse']=math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a))
                entry['max_abs']=max(abs(x-y) for x,y in zip(a,b))
                checks.append(entry)
                if strict:assert entry['exact'] and entry['ray_match'],entry
        if not strict:
            result,other=render('pass3-pgo',samples,width)
            checks.append(dict(binary='pass3-pgo',samples=samples,width=width,exact=result==ref,ray_match=other['rays']==stats['rays'],rays=other['rays'],reference_rays=stats['rays'],changed_float_bytes=sum(a!=b for a,b in zip(result,ref))))
(out/'parity.json').write_text(json.dumps(checks,indent=2)+'\n')
print('Exact interfaces/sincos checks:',sum(x['exact'] for x in checks),'of',len(checks),'(PGO reported separately)')
