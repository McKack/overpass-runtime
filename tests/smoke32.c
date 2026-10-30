/* 32-bit Direct3D 11: the program must get DXMT's device through WoW64, clear a render target and read it back. */
#define COBJMACROS
#define INITGUID
#include <stdio.h>
#include <d3d11.h>

#define CHECK(what, hr) do { HRESULT r_ = (hr); printf("%s %s hr=%#lx\n", SUCCEEDED(r_) ? "ok  " : "FAIL", what, (unsigned long)r_); \
    if (FAILED(r_)) return 1; } while (0)

int main(void) {
    ID3D11Device *device; ID3D11DeviceContext *context; ID3D11Texture2D *target, *staging; ID3D11RenderTargetView *view;
    IDXGIDevice *dxgi; IDXGIAdapter *adapter; DXGI_ADAPTER_DESC desc;
    D3D11_TEXTURE2D_DESC tex = { 4, 4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
    const float green[4] = { 0, 1, 0, 1 };
    D3D11_MAPPED_SUBRESOURCE map;
    unsigned int pixel;

    CHECK("device", D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, NULL, &context));
    CHECK("dxgi device", ID3D11Device_QueryInterface(device, &IID_IDXGIDevice, (void **)&dxgi));
    CHECK("adapter", IDXGIDevice_GetAdapter(dxgi, &adapter));
    CHECK("adapter desc", IDXGIAdapter_GetDesc(adapter, &desc));
    /* Wine's own Direct3D reports an NVIDIA or AMD card; only DXMT passes the Mac's GPU through. */
    if (desc.VendorId != 0x106b) { printf("FAIL not on DXMT: vendor %04x\n", desc.VendorId); return 1; }
    CHECK("target", ID3D11Device_CreateTexture2D(device, &tex, NULL, &target));
    CHECK("view", ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)target, NULL, &view));
    tex.Usage = D3D11_USAGE_STAGING; tex.BindFlags = 0; tex.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    CHECK("staging", ID3D11Device_CreateTexture2D(device, &tex, NULL, &staging));
    ID3D11DeviceContext_ClearRenderTargetView(context, view, green);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)target);
    CHECK("map", ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map));
    pixel = *(unsigned int *)map.pData;
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    if (pixel != 0xff00ff00) { printf("FAIL readback %08x\n", pixel); return 1; }
    printf("ok   readback %08x\n", pixel);
    return 0;
}
