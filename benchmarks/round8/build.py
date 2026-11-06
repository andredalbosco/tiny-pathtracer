import pathlib,subprocess,hashlib,json
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';out.mkdir(exist_ok=True);records=[]
for compiler in ['gcc','clang']:
 for layout,name in [(0,'aos-control'),(1,'split'),(2,'fields'),(3,'soa')]:
  for fp in ['fast','off']:
   binary=out/(compiler+'-'+name+('-strict' if fp=='off' else ''));cmd=[('g++' if compiler=='gcc' else 'clang++-20'),'-isystem','.', '-isystem',str(root/'external/stb'),'-I',str(root/'experiments'), '-O3','-std=c++17','-Wall','-Wextra','-Wpedantic','-march=native','-ffp-contract='+fp,'-fopenmp','-DTRIANGLE_LAYOUT='+str(layout)]
   if compiler=='gcc':cmd+=['-fno-loop-unroll-and-jam']
   cmd+=['experiments/pathtracer_triangles.cpp','-o',str(binary)];subprocess.run(cmd,cwd=root,check=True);records.append(dict(name=binary.name,command=cmd,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()));(out/'builds.json').write_text(json.dumps(records,indent=2));print('built',binary.name,flush=True)
