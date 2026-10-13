import pathlib,subprocess,json,hashlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass6';records=json.loads((out/'variants.json').read_text())
for name,source,extra in [('nojam',(out/'control.cpp').read_text(),['-fno-loop-unroll-and-jam']),('inline_nojam',(out/'inline_hot.cpp').read_text(),['-fno-loop-unroll-and-jam']),('batch_nojam',(out/'control.cpp').read_text().replace('static void intersect_batch','__attribute__((optimize("no-loop-unroll-and-jam"))) static void intersect_batch'),[])]:
 file=out/(name+'.cpp');file.write_text(source);binary=out/name
 cmd=['g++','-iquote',str(root),'-isystem',str(root),'-O3','-std=c++17','-Wall','-Wextra','-Wpedantic','-march=native','-ffp-contract=fast','-fopenmp',*extra,str(file),'-o',str(binary)]
 subprocess.run(cmd,check=True);records.append(dict(name=name,source_sha256=hashlib.sha256(source.encode()).hexdigest(),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),command=cmd));print(name,flush=True)
(out/'variants.json').write_text(json.dumps(records,indent=2))
