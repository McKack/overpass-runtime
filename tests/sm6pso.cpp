/* OverPass Runtime: does a compiled DXIL shader turn into a D3D12 pipeline? Takes the shader's stage and its
 * .cso (with an embedded root signature), and for vertex and pixel shaders the .cso of the other stage.
 *   sm6pso.exe cs shader.cso
 *   sm6pso.exe ps shader.cso partner_vs.cso
 *   sm6pso.exe vs shader.cso partner_ps.cso
 *   sm6pso.exe ms shader.cso                    (mesh pipeline without a pixel shader)
 *   sm6pso.exe as shader.cso partner_ms.cso     (the mesh shader takes the amplification shader's payload)
 * Prints PASS or FAIL with the HRESULT, and exits 0 on PASS. One shader per process: a converter crash only
 * takes down its own run. */
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static std::vector<char> load(const char *path)
{
    std::vector<char> data;
    FILE *f = fopen(path, "rb");
    if (!f) return data;
    fseek(f, 0, SEEK_END); data.resize(ftell(f)); fseek(f, 0, SEEK_SET);
    fread(data.data(), 1, data.size(), f); fclose(f);
    return data;
}

int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: sm6pso cs|vs|ps|ms|as shader.cso [partner.cso]\n"); return 2; }
    const char *stage = argv[1];
    auto shader = load(argv[2]), partner = argc > 3 ? load(argv[3]) : std::vector<char>();
    if (shader.empty()) { printf("FAIL cannot read %s\n", argv[2]); return 1; }

    ID3D12Device *dev = nullptr;
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    if (FAILED(hr)) { printf("FAIL device %#lx\n", hr); return 1; }

    ID3D12RootSignature *rs = nullptr;
    hr = dev->CreateRootSignature(0, shader.data(), shader.size(), IID_PPV_ARGS(&rs));
    if (FAILED(hr)) { printf("FAIL root signature %#lx\n", hr); return 1; }

    ID3D12PipelineState *pso = nullptr;
    if (!strcmp(stage, "cs"))
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = rs;
        d.CS = { shader.data(), shader.size() };
        hr = dev->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso));
    }
    else if (!strcmp(stage, "ms") || !strcmp(stage, "as"))
    {
        // Each subobject: its type at pointer alignment, then its data at the data's own alignment.
        std::vector<char> stream;
        auto add = [&](D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, const void *data, size_t size, size_t align = 8) {
            size_t at = (stream.size() + 7) & ~size_t(7);
            size_t data_at = (at + sizeof(type) + align - 1) & ~(align - 1);
            stream.resize((data_at + size + 7) & ~size_t(7));
            memcpy(stream.data() + at, &type, sizeof(type));
            memcpy(stream.data() + data_at, data, size);
        };
        add(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, &rs, sizeof(rs));
        const auto &ms = !strcmp(stage, "ms") ? shader : partner;
        D3D12_SHADER_BYTECODE ms_code = { ms.data(), ms.size() }, as_code = { shader.data(), shader.size() };
        add(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, &ms_code, sizeof(ms_code));
        if (!strcmp(stage, "as")) add(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, &as_code, sizeof(as_code));
        D3D12_RT_FORMAT_ARRAY rtv = {}; rtv.NumRenderTargets = 1; rtv.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        add(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, &rtv, sizeof(rtv), alignof(D3D12_RT_FORMAT_ARRAY));
        ID3D12Device2 *dev2 = nullptr;
        hr = dev->QueryInterface(IID_PPV_ARGS(&dev2));
        D3D12_PIPELINE_STATE_STREAM_DESC sd = { stream.size(), stream.data() };
        if (SUCCEEDED(hr)) hr = dev2->CreatePipelineState(&sd, IID_PPV_ARGS(&pso));
    }
    else
    {
        const auto &vs = !strcmp(stage, "vs") ? shader : partner;
        const auto &ps = !strcmp(stage, "ps") ? shader : partner;
        D3D12_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = rs;
        d.VS = { vs.data(), vs.size() };
        d.PS = { ps.data(), ps.size() };
        d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.SampleMask = ~0u;
        d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        d.RasterizerState.DepthClipEnable = TRUE;
        d.InputLayout = { il, 2 };
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        hr = dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&pso));
    }
    printf("%s pipeline %#lx\n", SUCCEEDED(hr) ? "PASS" : "FAIL", hr);
    return SUCCEEDED(hr) ? 0 : 1;
}
