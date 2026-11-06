import pathlib,subprocess,json,hashlib,re
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';s=(root/'experiments/pathtracer_plain.cpp').read_text();records=[]
helper='static int checked_count(const SceneData& scene) { if(scene.count<0)__builtin_unreachable(); return scene.count; }\n'
pos=s.index('static inline void intersect_batch');v=s[:pos]+helper+s[pos:];start=v.index('static inline void intersect_batch');end=v.index('struct Scratch',start);v=v[:start]+v[start:end].replace('i<scene.count','i<checked_count(scene)')+v[end:]
file=out/'plain-size-guard.cpp';file.write_text(v);cmd=['g++','-iquote',str(root),'-isystem',str(root),'-isystem',str(root/'external/stb'),'-I',str(root/'experiments'),'-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-fdump-tree-unrolljam-details','-fdump-tree-ch-details',str(file),'-o',str(out/'gcc-plain-size-guard')];subprocess.run(cmd,check=True);records.append(dict(name='gcc-plain-size-guard',command=cmd,source_sha256=hashlib.sha256(v.encode()).hexdigest()));print('gcc-plain-size-guard',flush=True)
folder=out/'stl-distance';folder.mkdir(exist_ok=True)
for name in ['experiments/pathtracer_quality.cpp','experiments/quality_sampling.h','experiments/simple_kernels.h']:(folder/pathlib.Path(name).name).write_bytes((root/name).read_bytes())
file=folder/'simple_kernels.h';t=file.read_text();pos=t.index('// Ordinary C++ over several rays.');t=t[:pos]+t[pos:].replace('i < int(triangles.size())','i < int(triangles.end()-triangles.begin())',1);file.write_text(t)
cmd=['g++','-iquote',str(root),'-isystem',str(root),'-isystem',str(root/'external/stb'),'-I',str(root/'experiments'),'-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-pthread','-fdump-tree-unrolljam-details','-fdump-tree-ch-details',str(folder/'pathtracer_quality.cpp'),'-o',str(out/'gcc-stl-distance')];subprocess.run(cmd,check=True);records.append(dict(name='gcc-stl-distance',command=cmd,source_sha256=hashlib.sha256(t.encode()).hexdigest()));print('gcc-stl-distance',flush=True)
for name,source in [('plain','experiments/pathtracer_plain.cpp'),('stl','experiments/pathtracer_quality.cpp')]:
 cmd=['clang++-20','-isystem','.', '-isystem',str(root/'external/stb'),'-I',str(root/'experiments'), '-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-fno-unroll-loops',source,'-o',str(out/('clang-'+name+'-nounroll'))];subprocess.run(cmd,cwd=root,check=True);records.append(dict(name='clang-'+name+'-nounroll',command=cmd));print('clang-'+name+'-nounroll',flush=True)
(out/'more-builds.json').write_text(json.dumps(records,indent=2))
for prefix in ['gcc-stl-ch','gcc-stl-cached-count','gcc-stl-distance']:
 for kind in ['ch2','unrolljam']:
  p=next(out.glob(prefix+'-*.t.'+kind),None)
  if not p:p=next(out.glob(prefix+'-*.ch2' if kind=='ch2' else prefix+'-*.unrolljam'))
  text=p.read_text();matches=list(re.finditer(r';; Function RayScene::intersect_batch.constprop',text));start=matches[1].start();end=text.find(';; Function',start+12);part=text[start:end];(out/(prefix+'-ray8-'+kind+'.txt')).write_text(part)
  print(prefix,kind,[l for l in part.splitlines() if 'applying unroll' in l or 'Duplicating header of' in l],flush=True)
