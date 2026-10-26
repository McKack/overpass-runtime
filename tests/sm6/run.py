import os, subprocess, sys, shlex
# usage: run.py <work dir with dxc/ and smoke/prefix> <corpus dir>
S, C = sys.argv[1], sys.argv[2]
wine = os.path.expanduser("~/Library/Application Support/OverPass/Wine/current/bin/wine")
env = dict(os.environ, WINEPREFIX=S + "/smoke/prefix", WINEDEBUG="-all", OVERPASS_DLL_PATH_PREPEND=os.path.expanduser("~/Dev/OverPass-runtime/install"))
win = lambda p: "Z:" + p.replace("/", "\\")
TABLES = "DescriptorTable(SRV(t0, numDescriptors=4), UAV(u0, numDescriptors=4), CBV(b0)), DescriptorTable(Sampler(s0, numDescriptors=2))"
RS = {
  "cs": f'RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED), {TABLES}',
  "gfx": f'RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED), {TABLES}',
}
def compile(name, prof, extra):
    out = f"{C}/dxil/{name}.cso"
    if os.path.exists(out): os.remove(out)
    rs = RS["cs" if prof[:2] in ("cs", "ms", "as") else "gfx"]
    r = subprocess.run([wine, S + "/dxc/bin/x64/dxc.exe", "-nologo", "-T", prof, *extra, "-E", "main",
                        "-rootsig-define", "RSIG", f"-DRSIG={rs}", "-Fo", win(out), win(f"{C}/src/{name}.hlsl")],
                       env=env, capture_output=True, text=True)
    open(f"{C}/log/{name}.dxc", "w").write(r.stdout + r.stderr)
    return out if os.path.exists(out) and os.path.getsize(out) else None
uvs, ups = compile("uni_vs", "vs_6_0", []), compile("uni_ps", "ps_6_0", [])
ums = compile("uni_ms", "ms_6_5", [])
results = []
for line in open(C + "/list.txt"):
    parts = shlex.split(line.replace('"', ''))
    name, prof = parts[0], parts[1]
    extra = [p for p in parts[2:] if p.startswith("-")]
    label = " ".join(p for p in parts[2:] if not p.startswith("-"))
    cso = compile(name, prof, extra)
    if not cso:
        status, detail = "NOCOMPILE", open(f"{C}/log/{name}.dxc").read().strip().splitlines()[:1]
    else:
        stage = prof[:2]
        args = [wine, S + "/smoke/sm6pso.exe"]
        if stage == "vs": args += ["vs", win(cso), win(ups)]
        elif stage == "ps": args += ["ps", win(cso), win(uvs)]
        elif stage == "ms": args += ["ms", win(cso)]
        elif stage == "as": args += ["as", win(cso), win(ums)]
        else: args += ["cs", win(cso)]
        try:
            r = subprocess.run(args, env=env, capture_output=True, text=True, timeout=120)
            out = r.stdout + r.stderr
        except subprocess.TimeoutExpired:
            out = "TIMEOUT"
        open(f"{C}/log/{name}.pso", "w").write(out)
        status = "PASS" if "PASS pipeline" in out else "FAIL"
        errs = [l for l in out.splitlines() if l.startswith(("err", "FAIL", "dxil", "warn")) or "abort" in l.lower()]
        detail = errs[:2]
    print(f"{status:9} {name:16} {label}" + ("" if status == "PASS" else "   | " + " / ".join(detail)[:170]), flush=True)
