#pragma once

#include "../include/shared.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct DrawPBRPush
{
    daxa_f32mat4x4 model_matrix;
    daxa_u32 material_idx;
    daxa_SamplerId default_sampler;
    daxa_BufferPtr(CameraInfo) cam_buffer;
    daxa_BufferPtr(GPUMaterial) material_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};
