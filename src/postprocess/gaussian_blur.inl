#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct GaussianBlurPC
{
    daxa_b32         horizontal;
    daxa_ImageViewId input_image;
    daxa_ImageViewId output_image;
    daxa_BufferPtr(GPUGlobals) global_buffer;
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 gaussian_blur_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/gaussian_blur.glsl"},
        .push_constant_size = sizeof(GaussianBlurPC),
        .name = "gaussian blur pipeline",
    };
}

inline void gaussian_blur_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline,
                                   daxa_b32 const * enabled, daxa::BufferId global_buffer,
                                   daxa::TaskImageView input_image, daxa::TaskImageView ping0,
                                   daxa::TaskImageView ping1, int num_passes)
{
    if (!(*enabled))
    {
        return;
    }

    daxa::CommandRecorder & cr = ti.recorder;

    // Ping pong gaussian blur h -> v -> h
    daxa::TaskImageView ping_pong_images[2] = {ping0, ping1};

    bool horizontal = true;
    for (int i = 0; i < num_passes; i++)
    {
        // Use output image size for dispatch so we write across the full target image.
        daxa::TaskImageView out_view = ping_pong_images[horizontal];
        daxa::Extent3D      out_size = ti.info(out_view).value().size;

        cr.set_pipeline(*pipeline);
        cr.push_constant(GaussianBlurPC{
            .horizontal = horizontal,
            .input_image = i == 0 ? ti.view(input_image) : ti.view(ping_pong_images[!horizontal]),
            .output_image = ti.view(out_view),
            .global_buffer = ti.device.device_address(global_buffer).value(),
        });
        cr.dispatch({
            .x = (out_size.x + 7u) / 8u,
            .y = (out_size.y + 7u) / 8u,
            .z = 1,
        });

        horizontal = !horizontal;
    }
}

#endif // __cplusplus
