#pragma once

#include "gpu_lighting.inl"
#include "gpu_scene.inl"

struct GPUFrameData
{
    daxa_f32 exposure;
    daxa_b32 ssao_enabled;
    daxa_f32 ssao_radius;
    daxa_f32 ssao_bias;
    GPULightInfo lights;
};
DAXA_DECL_BUFFER_PTR(GPUFrameData);

struct GPUGlobals
{
    daxa_SamplerId default_linear_sampler;
    daxa_SamplerId shadow_sampler;
    daxa_BufferPtr(GPUCamera) camera_buffer;
    daxa_BufferPtr(GPUFrameData) frame_data_buffer;
};
DAXA_DECL_BUFFER_PTR(GPUGlobals);
