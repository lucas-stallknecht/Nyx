#pragma once

#include "../include/shared.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct DrawPBRPush
{
    daxa_f32mat4x4 model_matrix;
    daxa_BufferPtr(CameraInfo) cam_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};
