import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
# Preserve the unsuppressed LLVM-runtime leak diagnostic and test the original
# with the same instrumentation before using libgomp for the storage checks.
baseline=out/'sanitize-original-clang'
build=['clang++-20','-isystem','.', '-isystem',str(root/'external/stb'),'-I',str(root/'experiments'),'-O1','-g','-std=c++17','-march=native',
       '-ffp-contract=off','-fopenmp','-fsanitize=address,undefined',
       '-fno-omit-frame-pointer','experiments/pathtracer_plain.cpp','-o',str(baseline)]
subprocess.run(build,cwd=root,check=True)
for name,binary in [('original',baseline),('fields',out/'sanitize-fields')]:
    trial_build=build
    if name=='fields':
        trial_build=build[:-3]+['-DTRIANGLE_LAYOUT=2','experiments/pathtracer_triangles.cpp','-o',str(binary)]
        subprocess.run(trial_build,cwd=root,check=True)
    cmd=[str(binary),'13','17',str(out/'sanitize.f32'),'--sampling','uniform',
         '--threads','1','--runtime','omp','--batch','8','--refill','--raw']
    p=subprocess.run(cmd,capture_output=True,text=True)
    (out/('clang-sanitizer-'+name+'.log')).write_text(p.stderr)
    assert p.returncode==1 and '224 byte(s) leaked in 1 allocation(s)' in p.stderr and '___kmp_allocate' in p.stderr,(name,p.stderr)
    rows.append(dict(name=name,command=cmd,build_command=trial_build,
                     result='same 224-byte libomp allocation leak as unchanged baseline'))
for layout,name in [(2,'fields'),(3,'soa')]:
    binary=out/('sanitize-gcc-'+name)
    cmd=['g++','-isystem','.', '-isystem',str(root/'external/stb'),'-I',str(root/'experiments'),'-O1','-g','-std=c++17','-march=native','-ffp-contract=off',
         '-fopenmp','-fsanitize=address,undefined','-fno-omit-frame-pointer',
         '-DTRIANGLE_LAYOUT='+str(layout),'experiments/pathtracer_triangles.cpp','-o',str(binary)]
    subprocess.run(cmd,cwd=root,check=True)
    for mode in ['uniform','cosine','nee','mis']:
        for threads in [1,32]:
            run=[str(binary),'13','17',str(out/'sanitize.f32'),'--sampling',mode,'--threads',str(threads),
                 '--runtime','omp','--batch','8','--refill','--raw']
            p=subprocess.run(run,capture_output=True,text=True,check=True)
            stats=json.loads(p.stderr)
            rows.append(dict(layout=name,mode=mode,threads=threads,command=run,build_command=cmd,passed=True))
(out/'sanitizer.json').write_text(json.dumps(rows,indent=2)+'\n');print('PASS',sum(r.get('passed',False) for r in rows),'ASan/UBSan renders',flush=True)
