import array,json,math,pathlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass7';rows=[]
def values(file):
 a=array.array('f');a.frombytes(file.read_bytes());return a
for mode in ['uniform','nee']:
 reference=values(out/('gcc-plain-'+mode+'-P1.f32'))
 for name,scope in [('gcc-stl','P1'),('clang-plain','P1'),('clang-stl','P1'),('gcc-plain-libomp','all32')]:
  candidate=values(out/(name+'-'+mode+'-'+scope+'.f32'));assert len(candidate)==len(reference);assert all(math.isfinite(x) and x>=0 for x in candidate)
  rows.append(dict(mode=mode,name=name,reference='gcc-plain',identical=candidate==reference,rmse=math.sqrt(sum((a-b)**2 for a,b in zip(reference,candidate))/len(reference)),maximum_absolute=max(abs(a-b) for a,b in zip(reference,candidate))))
(out/'fast-validation.json').write_text(json.dumps(rows,indent=2));print(json.dumps(rows,indent=2))
