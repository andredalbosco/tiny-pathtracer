import array,hashlib,json,math,pathlib,statistics,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/clang-simple';dest=root/'benchmarks/clang-simple';dest.mkdir(exist_ok=True)
rows=json.loads((out/'timings.json').read_text());summary=[];images=[]
for scope in ['P1','all32']:
    medians={name:statistics.median(r['wall_seconds'] for r in rows if r['name']==name and r['scope']==scope and r['repeat']>0)
             for name in ['previous','simple']}
    summary.append(dict(scope=scope,medians=medians,time_change_percent=100*(medians['simple']/medians['previous']-1),
                        ranges={name:[min(r['wall_seconds'] for r in rows if r['name']==name and r['scope']==scope and r['repeat']>0),
                                      max(r['wall_seconds'] for r in rows if r['name']==name and r['scope']==scope and r['repeat']>0)] for name in medians}))
    a=array.array('f');a.frombytes((out/('previous-'+scope+'.f32')).read_bytes())
    b=array.array('f');b.frombytes((out/('simple-'+scope+'.f32')).read_bytes())
    assert len(a)==len(b) and all(math.isfinite(x) for x in b)
    images.append(dict(scope=scope,rmse=math.sqrt(sum((x-y)**2 for x,y in zip(a,b))/len(a)),
                       max_absolute=max(abs(x-y) for x,y in zip(a,b)),pixels_identical=a==b))
for name in ['previous','simple']:
    assert len({r['image_sha256'] for r in rows if r['name']==name})==1,(name,'determinism')
info=dict(parent_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
          previous_source_commit='e90b8d6',clang=subprocess.check_output(['clang++-20','--version'],text=True).splitlines()[0],
          source_sha256=hashlib.sha256((root/'pathtracer_clang.cpp').read_bytes()).hexdigest(),
          binary_sha256={n:hashlib.sha256((root/'build'/n).read_bytes()).hexdigest() for n in ['pathtracer_clang','pathtracer_clang_strict','raw_clang','raw_clang_strict']},
          previous_binary_sha256=hashlib.sha256((out/'previous-fast').read_bytes()).hexdigest())
for name,value in [('summary.json',summary),('fast-images.json',images),('provenance.json',info)]:
    (out/name).write_text(json.dumps(value,indent=2)+'\n')
for name in ['bench.py','record.py','timings.json','validation.json','summary.json','fast-images.json','provenance.json']:
    (dest/name).write_text((out/name).read_text())
print(json.dumps(images,indent=2));print('archived',len(rows),'timing records')
