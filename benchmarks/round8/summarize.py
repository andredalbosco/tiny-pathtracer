import array,json,math,pathlib,statistics
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8'
rows=json.loads((out/'full-results.json').read_text());summary=[]
for mode in ['uniform','nee']:
    for scope in ['P1','all32']:
        for name in sorted({r['name'] for r in rows}):
            runs=[r for r in rows if r['mode']==mode and r['scope']==scope and r['name']==name]
            scored=[r['wall_seconds'] for r in runs if r['repeat']>0]
            assert len(runs)==6 and len(scored)==5,(name,mode,scope)
            assert len({r['sha256'] for r in runs})==1,(name,mode,scope,'determinism')
            summary.append(dict(name=name,mode=mode,scope=scope,median=statistics.median(scored),
                                minimum=min(scored),maximum=max(scored),sha256=runs[0]['sha256']))
for s in summary:
    baseline=next(t['median'] for t in summary if t['mode']==s['mode'] and t['scope']==s['scope'] and t['name']==s['name'].split('-')[0]+'-original')
    s['time_change_percent']=100*(s['median']/baseline-1)
    p1=next(t['median'] for t in summary if t['mode']==s['mode'] and t['scope']=='P1' and t['name']==s['name'])
    s['speedup_from_one_P_core']=p1/s['median']
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
images=[]
def read_image(file):
    a=array.array('f');a.frombytes(file.read_bytes());return a
for mode in ['uniform','nee']:
    for compiler in ['gcc','clang']:
        ref=read_image(out/(compiler+'-original-'+mode+'-P1.f32'))
        for layout in ['fields','soa']:
            data=read_image(out/(compiler+'-'+layout+'-'+mode+'-P1.f32'))
            assert len(ref)==len(data)
            squared=0.;maximum=0.;different=0
            for a,b in zip(ref,data):
                assert math.isfinite(a) and math.isfinite(b)
                d=abs(a-b);squared+=d*d;maximum=max(maximum,d);different+=a!=b
            images.append(dict(name=compiler+'-'+layout,reference=compiler+'-original',mode=mode,
                               rmse=math.sqrt(squared/len(ref)),max_absolute=maximum,different_channels=different))
(out/'fast-validation.json').write_text(json.dumps(images,indent=2)+'\n')
print(json.dumps(summary,indent=2));print(json.dumps(images,indent=2))
