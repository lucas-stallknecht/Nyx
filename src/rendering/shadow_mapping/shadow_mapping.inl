#pragma once

#include "../../include/shared.inl"
#include <daxa/daxa.inl>

struct DrawDirectionalDepthMap
{
    daxa_f32mat4x4 model_matrix;
    daxa_BufferPtr(LightInfo) light_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};
