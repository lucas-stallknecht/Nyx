#pragma once

#include "gpu_lighting.inl"
#include "gpu_scene.inl"

struct GPUFrameData
{
    daxa_f32 exposure;
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
