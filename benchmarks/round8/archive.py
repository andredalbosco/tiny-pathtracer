import json,pathlib,shutil
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';dest=root/'benchmarks/round8';dest.mkdir(exist_ok=True)
names=['build.py','validate.py','screen.py','full_bench.py','validate_more.py','summarize.py',
       'audit.py','audit_gcc.py','perf_compare.py','sanitize.py','provenance.py','delivery.py','archive.py',
       'builds.json','validation.json','screen.json','full-results.json','summary.json','validation-more.json',
       'fast-validation.json','loop-audit.json','gcc-loop-audit.json','sanitizer.json','provenance.json','delivery.json',
       'clang-sanitizer-original.log','clang-sanitizer-fields.log']
for name in names:shutil.copyfile(out/name,dest/name)
shutil.copyfile(out/'perf/counts.json',dest/'perf-counts.json')
for file in out.glob('clang-*-intersect.asm'):shutil.copyfile(file,dest/file.name)
for file in out.glob('gcc-*-loop.asm'):shutil.copyfile(file,dest/file.name)
for file in (out/'perf').glob('*.csv'):shutil.copyfile(file,dest/('perf-'+file.name))
for file in dest.iterdir():
    if file.is_file():file.write_text('\n'.join(x.rstrip() for x in file.read_text().rstrip().splitlines())+'\n')
rows=json.loads((out/'full-results.json').read_text())
for name in {r['name'] for r in rows}:
    for mode in ['uniform','nee']:
        assert len({r['sha256'] for r in rows if r['name']==name and r['mode']==mode})==1,(name,mode)
print('archived',len(list(dest.iterdir())),'files; fast scheduling hashes identical')
