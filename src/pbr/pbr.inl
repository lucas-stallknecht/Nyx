#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>
#include "../shared.inl"

struct DrawPBRPush
{
    daxa_BufferPtr(Vertex) vertex_buffer;
};
