import json,pathlib,re,subprocess
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';rows=[]
for name in ['original','fields','soa']:
    binary=root/'build/pathtracer_plain' if name=='original' else out/('gcc-'+name)
    asm=subprocess.check_output(['objdump','-d','-C','--no-show-raw-insn',str(binary)],text=True)
    function='';instructions=[];candidates=[]
    for line in asm.splitlines():
        header=re.match(r'^[0-9a-f]+ <(.*)>:',line)
        if header:function=header[1];instructions=[]
        m=re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)',line)
        if not m:continue
        address=int(m[1],16);op=m[2];args=m[3];instructions.append((address,op,args,line))
        target=re.match(r'([0-9a-f]+) <',args)
        if 'render<' in function and op.startswith('j') and target and int(target[1],16)<address:
            first=int(target[1],16);body=[x for x in instructions if first<=x[0]<=address]
            if len(body)<200 and any(x[1]=='vdivps' for x in body):
                candidates.append(dict(name=name,function=function,start=hex(first),end=hex(address),
                                       instructions=len(body),ymm_stack_operands=sum('%ymm' in x[2] and ('%rsp' in x[2] or '%rbp' in x[2]) for x in body),
                                       vector_divides=sum(x[1]=='vdivps' for x in body),
                                       permutes=sum(x[1].startswith('vperm') for x in body)))
                mode=re.search(r'render<(\d)>',function)[1]
                (out/('gcc-'+name+'-mode'+mode+'-loop.asm')).write_text('\n'.join(x[3].rstrip() for x in body)+'\n')
    assert len(candidates)==4,(name,len(candidates))
    rows.extend(candidates)
(out/'gcc-loop-audit.json').write_text(json.dumps(rows,indent=2)+'\n');print(json.dumps(rows,indent=2))
