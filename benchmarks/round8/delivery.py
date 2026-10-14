import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parents[2];out=root/'build/pass8';records=[]
for delivered,scored in [('pathtracer_triangles_soa','gcc-soa'),
                         ('pathtracer_triangles_fields','gcc-fields'),
                         ('pathtracer_triangles_clang_soa','clang-soa'),
                         ('pathtracer_triangles_clang_fields','clang-fields'),
                         ('pathtracer_triangles_strict_soa','gcc-soa-strict'),
                         ('pathtracer_triangles_clang_strict_fields','clang-fields-strict')]:
    a=hashlib.sha256((root/'build'/delivered).read_bytes()).hexdigest()
    b=hashlib.sha256((out/scored).read_bytes()).hexdigest()
    assert a==b,(delivered,scored)
    records.append(dict(delivered=delivered,scored=scored,sha256=a,identical=True))
(out/'delivery.json').write_text(json.dumps(records,indent=2)+'\n');print('PASS',len(records),'delivered/scored binary identities')
