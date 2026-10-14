import pathlib,subprocess,json,hashlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';s=(root/'pathtracer_plain.cpp').read_text();records=[]
cases={'max-reference':s.replace('static float maximum(float a,float b)', 'static const float& maximum(const float& a,const float& b)'), 'count-positive':s.replace('    for(int i=0;i<scene.count;++i) {\n        const Triangle& triangle=', '    if(scene.count<0)__builtin_unreachable();\n    for(int i=0;i<scene.count;++i) {\n        const Triangle& triangle=',1)}
for name,source in cases.items():
    file=out/(name+'.cpp');file.write_text(source);binary=out/('gcc-'+name)
    cmd=['g++','-iquote',str(root),'-isystem',str(root),'-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-fdump-tree-unrolljam-details','-fdump-tree-ch-details',str(file),'-o',str(binary)]
    subprocess.run(cmd,check=True);records.append(dict(name=binary.name,command=cmd,source_sha256=hashlib.sha256(source.encode()).hexdigest()));print(binary.name,flush=True)
folder=out/'stl-cached-count';folder.mkdir(exist_ok=True)
for file in ['pathtracer_quality.cpp','quality_sampling.h','simple_kernels.h']:(folder/file).write_bytes((root/file).read_bytes())
file=folder/'simple_kernels.h';t=file.read_text();pos=t.index('// Ordinary C++ over several rays.');before=t[:pos];after=t[pos:].replace('        for (int i = 0; i < int(triangles.size()); ++i) {','        const int triangle_count = int(triangles.size());\n        for (int i = 0; i < triangle_count; ++i) {',1);file.write_text(before+after)
cmd=['g++','-iquote',str(root),'-isystem',str(root),'-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-pthread','-fdump-tree-unrolljam-details','-fdump-tree-ch-details',str(folder/'pathtracer_quality.cpp'),'-o',str(out/'gcc-stl-cached-count')]
subprocess.run(cmd,check=True);records.append(dict(name='gcc-stl-cached-count',command=cmd,source_sha256=hashlib.sha256(file.read_bytes()).hexdigest()));print('gcc-stl-cached-count',flush=True)
(out/'trigger-builds.json').write_text(json.dumps(records,indent=2))
