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

## Install (VisionALVR users)

1. Download `VisionALVR-OptiScaler-<commit>.dll` from this repository's **Releases** page (or build it, below).
2. Copy it next to the game's real executable (Unreal titles: `<Game>\Binaries\Win64\`, where `*-Win64-Shipping.exe` lives) and rename
   it to one of OptiScaler's proxy names: `dxgi.dll` is the usual choice; `winmm.dll`, `version.dll`, `dbghelp.dll`, `winhttp.dll` or
   `wininet.dll` when another mod already owns `dxgi.dll`. One file, no registry change, no ini: OptiScaler intercepts the NGX SDK's lookup
   of `nvngx.dll` itself and loads NVIDIA's real core from the driver store.
3. The game must run DLSS Super Resolution (any quality mode) for vectors to exist. In VisionALVR, tick **DLSS motion vectors** under the
   Frame generation toggle; the shim log then reads `motion vectors: attached to ...dxgi.dll`, `vectors WxH ... -> shared copy` and the
   10-second `frame generation perf` line shows `mv` frames counting. Delete the file to uninstall.

Known incompatibilities: Luke Ross's R.E.A.L. VR mods hook NGX for their own DLSS use and crash with this DLL under any proxy name; titles with
kernel anti-cheat reject it (as they do upstream OptiScaler). Flat-to-VR mods that do not touch DLSS (UEVR) and native DLSS titles work.

## Build

Visual Studio 2022, `git clone --recursive -b visionalvr`, `msbuild OptiScaler.sln /p:Configuration=Release /p:Platform=x64`; the DLL lands in
`x64\Release\a\OptiScaler.dll`. Everything else: upstream's README, Config.md and licence (GPL-3.0; the source of every released DLL is the
tagged commit it is named after).
