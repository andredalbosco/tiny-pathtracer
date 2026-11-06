import datetime,hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8'
def command(args):return subprocess.check_output(args,cwd=root,text=True).strip()
data=dict(parent_commit=command(['git','rev-parse','HEAD']),recorded_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
          gcc=command(['g++','--version']).splitlines()[0],clang=command(['clang++-20','--version']).splitlines()[0],
          hardware=command(['lscpu']),sources={p:hashlib.sha256((root/p).read_bytes()).hexdigest()
          for p in ['experiments/pathtracer_plain.cpp','experiments/pathtracer_triangles.cpp','experiments/triangle_storage.h','Makefile']})
data['libraries']={n:command(['ldd',str(out/('clang-'+n))]) for n in ['fields','soa']}
(out/'provenance.json').write_text(json.dumps(data,indent=2)+'\n');print(data['parent_commit'])
