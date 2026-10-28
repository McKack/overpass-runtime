# Shader model 6.x coverage

One small HLSL shader per feature, grouped by the shader model that introduced it (`features.txt`). `run.py`
compiles each with Microsoft's DXC (with a root signature embedded) and creates a real D3D12 pipeline from it
through `sm6pso.exe` (`tests/sm6pso.cpp`), one process per shader, under overpass-wine with overpass-runtime.
A pass means DXMT turned the shader into a Metal pipeline; it doesn't check the shader's results.

    run.py <work dir> <corpus dir>

The work dir holds `dxc/bin/x64/dxc.exe` (from github.com/microsoft/DirectXShaderCompiler releases),
`smoke/sm6pso.exe` and a Wine prefix in `smoke/prefix`. `results-*.txt` are dated runs.
