# VisionALVR-OptiScaler

A fork of [OptiScaler](https://github.com/optiscaler/OptiScaler) (GPL-3.0, unchanged) for [VisionALVR](https://github.com/jfbobier/vision-alvr):
it hands the game's DLSS inputs (motion vectors, depth, colour, jitter, scale, flags) to the VisionALVR OpenXR runtime running in the same
process, which uses them for frame generation. DLSS itself keeps running NVIDIA's own code through OptiScaler's DLSS backend.

Changes against upstream (branch `visionalvr`):
- `OptiScaler/visionalvr/VaProvider.{h,cpp}`: the provider, a C ABI (`VisionALVR_ProviderVersion`, `VisionALVR_SetConsumerV1`) that calls the
  registered consumer synchronously at every DLSS Super Resolution evaluate (D3D11 and D3D12) with the game's resources and parameters.
  No allocation, no copy, no blocking; without a consumer it costs a few parameter reads.
- Two hook lines in `upscalers/dlss/DLSSFeature_Dx11.cpp` and `DLSSFeature_Dx12.cpp`, right before the native evaluate.
- Compiled defaults so that no `OptiScaler.ini` is needed: DLSS backend for D3D11, D3D12 and Vulkan, overlay menu off, DXGI spoofing off.
  An ini next to the game still overrides them, as upstream.

Install: copy the built `OptiScaler.dll` next to the game's executable under one of OptiScaler's proxy names (`dxgi.dll`, `winmm.dll`,
`version.dll`, `dbghelp.dll`, `winhttp.dll`, `wininet.dll`). No registry change: OptiScaler intercepts the NGX SDK's lookup of
`nvngx.dll` itself and loads the real core from the driver store.

Build: Visual Studio 2022, `msbuild OptiScaler.sln /p:Configuration=Release /p:Platform=x64`; the DLL lands in `x64\Release\a\`.
Everything else: upstream's README, Config.md and licence.
