#include <pch.h>
#include "VaProvider.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

#include <d3d11.h>
#include <d3d12.h>

namespace
{
std::atomic<DLSSVEC_ConsumerV1> g_consumer { nullptr };
std::atomic<void*> g_user { nullptr };
std::mutex g_framesMutex;
std::unordered_map<uint32_t, uint64_t> g_frames;   // evaluate counter per stream

uint64_t NextFrame(uint32_t stream)
{
    std::lock_guard<std::mutex> lock(g_framesMutex);
    return ++g_frames[stream];
}

template <typename TRes>
void* Res(NVSDK_NGX_Parameter* p, const char* key)
{
    TRes* r = nullptr;
    if (p->Get(key, &r) != NVSDK_NGX_Result_Success)
        return nullptr;
    return r;
}

float F(NVSDK_NGX_Parameter* p, const char* key, float def = 0.f)
{
    float v = def;
    return p->Get(key, &v) == NVSDK_NGX_Result_Success ? v : def;
}

uint32_t U(NVSDK_NGX_Parameter* p, const char* key, uint32_t def = 0)
{
    unsigned int v = def;
    return p->Get(key, &v) == NVSDK_NGX_Result_Success ? v : def;
}

int I(NVSDK_NGX_Parameter* p, const char* key, int def = 0)
{
    int v = def;
    return p->Get(key, &v) == NVSDK_NGX_Result_Success ? v : def;
}

template <typename TRes>
void Publish(uint32_t api, void* cmd, NVSDK_NGX_Parameter* params, uint32_t stream, int featureFlags, uint32_t renderW,
             uint32_t renderH, uint32_t outputW, uint32_t outputH)
{
    const uint64_t frame = NextFrame(stream);
    DLSSVEC_ConsumerV1 consumer = g_consumer.load(std::memory_order_acquire);
    // Without a consumer the only work is a log line now and then, which also serves as the smoke test of the hook.
    if (consumer == nullptr)
    {
        if (frame == 1 || frame % 2000 == 0)
        {
            LOG_INFO("VisionALVR provider: stream {} (dx{}) evaluate #{} render {}x{} output {}x{} mv {} depth {} flags 0x{:X}, no consumer",
                     stream, api, frame, renderW, renderH, outputW, outputH,
                     Res<TRes>(params, NVSDK_NGX_Parameter_MotionVectors) != nullptr,
                     Res<TRes>(params, NVSDK_NGX_Parameter_Depth) != nullptr, (uint32_t) featureFlags);
        }
        return;
    }

    DLSSVEC_PublicationV1 pub {};
    pub.struct_size = sizeof(pub);
    pub.version = DLSSVEC_PROVIDER_VERSION;
    pub.api = api;
    pub.stream = stream;
    pub.frame = frame;
    pub.cmd = cmd;
    pub.color = Res<TRes>(params, NVSDK_NGX_Parameter_Color);
    pub.motion_vectors = Res<TRes>(params, NVSDK_NGX_Parameter_MotionVectors);
    pub.depth = Res<TRes>(params, NVSDK_NGX_Parameter_Depth);
    pub.output = Res<TRes>(params, NVSDK_NGX_Parameter_Output);
    pub.exposure = Res<TRes>(params, NVSDK_NGX_Parameter_ExposureTexture);
    pub.render_w = U(params, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, renderW);
    pub.render_h = U(params, NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, renderH);
    pub.output_w = outputW;
    pub.output_h = outputH;
    pub.color_x = U(params, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X);
    pub.color_y = U(params, NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y);
    pub.mv_x = U(params, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X);
    pub.mv_y = U(params, NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y);
    pub.depth_x = U(params, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X);
    pub.depth_y = U(params, NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y);
    pub.jitter_x = F(params, NVSDK_NGX_Parameter_Jitter_Offset_X);
    pub.jitter_y = F(params, NVSDK_NGX_Parameter_Jitter_Offset_Y);
    pub.mv_scale_x = F(params, NVSDK_NGX_Parameter_MV_Scale_X, 1.f);
    pub.mv_scale_y = F(params, NVSDK_NGX_Parameter_MV_Scale_Y, 1.f);
    pub.reset = I(params, NVSDK_NGX_Parameter_Reset);
    pub.flags = (uint32_t) featureFlags;
    pub.params = params;

    // The consumer's mistakes must never take the game down: swallow structured exceptions around the callback.
    __try
    {
        consumer(&pub, g_user.load(std::memory_order_acquire));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        LOG_ERROR("VisionALVR provider: consumer raised an exception at stream {} frame {}; consumer removed", stream, frame);
        g_consumer.store(nullptr, std::memory_order_release);
    }
}
} // namespace

extern "C" __declspec(dllexport) uint32_t DLSSVEC_ProviderVersion() { return DLSSVEC_PROVIDER_VERSION; }

extern "C" __declspec(dllexport) int DLSSVEC_SetConsumerV1(DLSSVEC_ConsumerV1 consumer, void* user)
{
    g_user.store(user, std::memory_order_release);
    g_consumer.store(consumer, std::memory_order_release);
    LOG_INFO("VisionALVR provider: consumer {}", consumer != nullptr ? "registered" : "removed");
    return 1;
}

namespace VaProvider
{
void PublishDx12(ID3D12GraphicsCommandList* cmd, NVSDK_NGX_Parameter* params, uint32_t stream, int featureFlags,
                 uint32_t renderW, uint32_t renderH, uint32_t outputW, uint32_t outputH)
{
    Publish<ID3D12Resource>(12, cmd, params, stream, featureFlags, renderW, renderH, outputW, outputH);
}

void PublishDx11(ID3D11DeviceContext* ctx, NVSDK_NGX_Parameter* params, uint32_t stream, int featureFlags,
                 uint32_t renderW, uint32_t renderH, uint32_t outputW, uint32_t outputH)
{
    Publish<ID3D11Resource>(11, ctx, params, stream, featureFlags, renderW, renderH, outputW, outputH);
}
} // namespace VaProvider
