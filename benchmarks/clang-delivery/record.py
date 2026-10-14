import hashlib,json,pathlib,statistics,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/clang-delivery';dest=root/'benchmarks/clang-delivery';dest.mkdir(exist_ok=True)
rows=json.loads((out/'timings.json').read_text());summary=[]
for mode in ['uniform','nee']:
    for scope in ['P1','all32']:
        runs=[r for r in rows if r['mode']==mode and r['scope']==scope]
        values={name:statistics.median(r['wall_seconds'] for r in runs if r['name']==name and r['repeat']>0)
                for name in {r['name'] for r in runs}}
        reference=next(v for k,v in values.items() if k!='pathtracer_clang')
        summary.append(dict(mode=mode,scope=scope,medians=values,time_change_percent=100*(values['pathtracer_clang']/reference-1),
                            ranges={name:[min(r['wall_seconds'] for r in runs if r['name']==name and r['repeat']>0),
                                          max(r['wall_seconds'] for r in runs if r['name']==name and r['repeat']>0)] for name in values}))
    hashes={r['image_sha256'] for r in rows if r['mode']==mode}
    assert len(hashes)==1,(mode,'fast full pixels or scheduling')
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
info=dict(parent_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
          clang=subprocess.check_output(['clang++-20','--version'],text=True).splitlines()[0],
          sources={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ['pathtracer_clang.cpp','Makefile','tests/validate_clang.py']},
          binaries={n:hashlib.sha256((root/'build'/n).read_bytes()).hexdigest() for n in ['pathtracer_clang','pathtracer_clang_strict','pathtracer_triangles_clang_fields','pathtracer_triangles_clang_soa']})
(out/'provenance.json').write_text(json.dumps(info,indent=2)+'\n')
for name in ['bench.py','record.py','timings.json','validation.json','summary.json','provenance.json']:
    (dest/name).write_text((out/name).read_text())
print('PASS full fast images identical across binaries and threads; archived',len(rows),'timing records')
