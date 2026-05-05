#pragma once

#include "../include/shared.inl"
#include <daxa/daxa.inl>

struct DrawForwardPC
{
    daxa_f32mat4x4 model_matrix;
    daxa_u32 material_idx;
    daxa_ImageViewId shadow_depth_image;
    daxa_BufferPtr(GlobalRenderingBuffer) global_buffer;
    daxa_BufferPtr(GPUMaterial) material_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};
