import json,pathlib,re,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
for name in ['original','aos-control','split','fields','soa']:
    asm=out/('clang-'+name+'-intersect.asm')
    binary=root/'build/pathtracer_plain_clang' if name=='original' else out/('clang-'+name)
    decoded=subprocess.check_output(['objdump','-d','-C','--no-show-raw-insn',str(binary)],text=True).splitlines()
    first=next(i for i,line in enumerate(decoded) if re.match(r'^[0-9a-f]+ <intersect_batch\(SceneData const&, Ray const\*, Hit\*\)>:',line))
    last=next(i for i in range(first+1,len(decoded)) if not decoded[i].strip())
    asm.write_text('\n'.join(line.rstrip() for line in decoded[first:last])+'\n')
    instructions=[]
    for line in asm.read_text().splitlines():
        m=re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)',line)
        if m:instructions.append((int(m[1],16),m[2],m[3],line))
    loops=[]
    for address,op,args,_ in instructions:
        target=re.match(r'([0-9a-f]+) <intersect_batch',args)
        if op.startswith('j') and target and int(target[1],16)<address:
            first=int(target[1],16)
            body=[x for x in instructions if first<=x[0]<=address]
            if any(x[1]=='vdivps' for x in body):loops.append(body)
    assert len(loops)==1,(name,len(loops))
    body=loops[0]
    rows.append(dict(name=name,start=hex(body[0][0]),end=hex(body[-1][0]),
                     instructions=len(body),ymm_stack_operands=sum('%ymm' in x[2] and '%rsp' in x[2] for x in body),
                     vector_divides=sum(x[1]=='vdivps' for x in body),
                     permutes=sum(x[1].startswith('vperm') for x in body)))
(out/'loop-audit.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(rows,indent=2))
