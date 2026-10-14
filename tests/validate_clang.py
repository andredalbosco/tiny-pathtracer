"""Compare the simple renderer with the independent uniform PCG reference."""
import hashlib,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build/clang-simple';out.mkdir(exist_ok=True);rows=[]
for width,samples in [(1,1),(9,3),(17,13),(33,9),(64,32),(320,128)]:
    image=out/'reference.f32'
    cmd=[str(root/'build/pathtracer_plain_clang_strict'),str(samples),str(width),str(image),
         '--sampling','uniform','--seed','42','--threads','1','--raw']
    subprocess.run(cmd,capture_output=True,check=True);reference=image.read_bytes()
    for threads in ([1,32] if width==320 else [1,4,32]):
        cmd=[str(root/'build/raw_clang_strict'),str(samples),str(width),str(threads)]
        p=subprocess.run(cmd,capture_output=True,check=True)
        assert p.stdout==reference,(width,samples,threads,'pixels')
        rows.append(dict(width=width,samples=samples,threads=threads,pixels_identical=True,
                         sha256=hashlib.sha256(p.stdout).hexdigest()))
(out/'validation.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS',len(rows),'strict pixel comparisons',flush=True)
