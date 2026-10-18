// VisionALVR provider (github.com/jfbobier/OptiScaler-DLSSMotionVectorExtractor, GPL-3.0 like the rest of OptiScaler).
//
// Hands the inputs of every DLSS Super Resolution evaluate (colour, motion vectors, depth, output, jitter, scale, flags) to an
// in-process consumer: the VisionALVR OpenXR runtime, which lives in the same game process and records the copies it needs on the
// game's own command list / device context, inside the call. OptiScaler keeps nothing, allocates nothing and never blocks; with no
// consumer registered the publication costs a few parameter reads. C ABI, version 1; the consumer finds this module by its export.
#pragma once
#include <cstdint>

struct NVSDK_NGX_Parameter;
struct ID3D12GraphicsCommandList;
struct ID3D11DeviceContext;

extern "C"
{
    constexpr uint32_t VISIONALVR_PROVIDER_VERSION = 1;

    struct VisionALVR_PublicationV1
    {
        uint32_t struct_size;   // sizeof(VisionALVR_PublicationV1)
        uint32_t version;       // VISIONALVR_PROVIDER_VERSION
        uint32_t api;           // 11 = D3D11 (cmd is ID3D11DeviceContext*, resources ID3D11Resource*), 12 = D3D12 (ID3D12GraphicsCommandList*, ID3D12Resource*)
        uint32_t stream;        // OptiScaler feature handle id: one per DLSS feature (one per eye in VR titles)
        uint64_t frame;         // evaluate counter of this stream
        void* cmd;              // the game's command list / context the evaluate is recorded on; record copies here, before DLSS runs
        void* color;            // DLSS inputs as set by the game; any may be null
        void* motion_vectors;
        void* depth;
        void* output;
        void* exposure;
        uint32_t render_w, render_h;   // DLSS input size (the render subrect)
        uint32_t output_w, output_h;   // display size
        uint32_t color_x, color_y;     // input subrect base (colour); mv/depth bases below
        uint32_t mv_x, mv_y, depth_x, depth_y;
        float jitter_x, jitter_y;      // NGX jitter offset (pixels of the render size)
        float mv_scale_x, mv_scale_y;  // NGX MV scale
        int32_t reset;                 // history reset requested by the game
        uint32_t flags;                // NVSDK_NGX_DLSS_Feature_Flags of the feature (MVLowRes, MVJittered, DepthInverted, IsHDR, ...)
        NVSDK_NGX_Parameter* params;   // everything else, valid during the call only
    };

    typedef void (*VisionALVR_ConsumerV1)(const VisionALVR_PublicationV1* publication, void* user);

    // The ABI version this module speaks (1).
    __declspec(dllexport) uint32_t VisionALVR_ProviderVersion();
    // Registers (or, with nullptr, removes) the consumer called synchronously at every DLSS evaluate. Returns 1 on success.
    __declspec(dllexport) int VisionALVR_SetConsumerV1(VisionALVR_ConsumerV1 consumer, void* user);
}

namespace VaProvider
{
void PublishDx12(ID3D12GraphicsCommandList* cmd, NVSDK_NGX_Parameter* params, uint32_t stream, int featureFlags,
                 uint32_t renderW, uint32_t renderH, uint32_t outputW, uint32_t outputH);
void PublishDx11(ID3D11DeviceContext* ctx, NVSDK_NGX_Parameter* params, uint32_t stream, int featureFlags,
                 uint32_t renderW, uint32_t renderH, uint32_t outputW, uint32_t outputH);
} // namespace VaProvider
