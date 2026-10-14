import pathlib,subprocess,hashlib,json
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';out.mkdir(exist_ok=True)
s=(root/'pathtracer_plain.cpp').read_text();start=s.index('static inline void intersect_batch');end=s.index('struct Scratch',start);block=s[start:end]
variants={'plain-default':s}
view=block.replace('const SceneData& scene,','const Triangle* triangles,int triangle_count,').replace('scene.count','triangle_count').replace('scene.triangles','triangles')
variants['plain-pointer']=s[:start]+view+s[end:];variants['plain-pointer']=variants['plain-pointer'].replace('intersect_batch(scene,rays,hits);','intersect_batch(scene.triangles,scene.count,rays,hits);')
view=block.replace('const SceneData& scene,','const Triangle* triangles,const Triangle* end,').replace('    float ox[8]', '    int triangle_count=int(end-triangles);\n    float ox[8]').replace('scene.count','triangle_count').replace('scene.triangles','triangles')
variants['plain-end']=s[:start]+view+s[end:];variants['plain-end']=variants['plain-end'].replace('intersect_batch(scene,rays,hits);','intersect_batch(scene.triangles,scene.triangles+scene.count,rays,hits);')
records=[]
for name,source in variants.items():
    file=out/(name+'.cpp');file.write_text(source)
    cmd=['g++','-iquote',str(root),'-isystem',str(root),'-O3','-std=c++17','-march=native','-ffp-contract=fast','-fopenmp','-fdump-tree-unrolljam-details',str(file),'-o',str(out/('gcc-'+name))]
    subprocess.run(cmd,check=True);records.append(dict(name='gcc-'+name,command=cmd,source_sha256=hashlib.sha256(source.encode()).hexdigest(),binary_sha256=hashlib.sha256((out/('gcc-'+name)).read_bytes()).hexdigest()));print('built',name,flush=True)
for name,source in [('plain','pathtracer_plain.cpp'),('stl','pathtracer_quality.cpp')]:
    for fp in ['fast','off']:
        binary=out/('clang-'+name+('-strict' if fp=='off' else ''))
        cmd=['clang++-20','-isystem','.', '-O3','-std=c++17','-Wall','-Wextra','-Wpedantic','-march=native','-ffp-contract='+fp,'-fopenmp',source,'-o',str(binary)]
        if name=='stl':cmd.insert(cmd.index(source),'-pthread')
        subprocess.run(cmd,cwd=root,check=True);records.append(dict(name=binary.name,command=cmd,source_sha256=hashlib.sha256((root/source).read_bytes()).hexdigest(),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()));print('built',binary.name,flush=True)
(out/'builds.json').write_text(json.dumps(records,indent=2))
