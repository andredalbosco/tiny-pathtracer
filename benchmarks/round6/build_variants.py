import pathlib,subprocess,hashlib,json
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass6';out.mkdir(exist_ok=True)
s=(root/'benchmarks/round6/plain-before.cpp').read_text();start=s.index('static void intersect_batch');end=s.index('struct Scratch',start)
variants={'control':s,'inline_batch':s.replace('static void intersect_batch','static inline void intersect_batch')}
v=s
for prefix in ['static void intersect_batch','static bool visible','static float power_weight','static Float3 cosine_direction','template<int Mode> static void scatter']:
 v=v.replace(prefix,prefix.replace('static ','static inline ',1))
variants['inline_hot']=v
old=s.replace('    Float2() = default;\n    Float2(float x, float y)', '    Float2(float x=0, float y=0)').replace('    Float3() = default;\n    Float3(float x, float y, float z)', '    Float3(float x=0, float y=0, float z=0)').replace('Path p{};p.active=', 'Path p;p.active=')
variants['old_ctors']=old
block=s[start:end];block=block.replace('    for(int i=0;i<scene.count;++i) {','    #pragma GCC unroll 1\n    for(int i=0;i<scene.count;++i) {',1)
variants['unroll1']=s[:start]+block+s[end:]
count_block=s[start:end].replace('const Ray* rays,Hit* hits)', 'const Ray* rays,Hit* hits,int count)').replace('r<8','r<count')
variants['count_param']=(s[:start]+count_block+s[end:]).replace('intersect_batch(scene,rays,hits);','intersect_batch(scene,rays,hits,8);')
variants['inline_unroll1']=variants['unroll1'].replace('static void intersect_batch','static inline void intersect_batch')
# The original run froze production baselines before source edits.
# Rebuilding probe sources does not replace those frozen baselines.
records=[]
for name,source in variants.items():
 file=out/(name+'.cpp');binary=out/name;file.write_text(source)
 cmd=['g++','-iquote',str(root),'-isystem',str(root),'-isystem',str(root/'external/stb'),'-I',str(root/'experiments'),'-O3','-std=c++17','-Wall','-Wextra','-Wpedantic','-march=native','-ffp-contract=fast','-fopenmp',str(file),'-o',str(binary)]
 subprocess.run(cmd,check=True)
 records.append(dict(name=name,source_sha256=hashlib.sha256(source.encode()).hexdigest(),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),command=cmd))
 print(name,flush=True)
(out/'variants.json').write_text(json.dumps(records,indent=2))
