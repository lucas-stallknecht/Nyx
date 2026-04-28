#pragma once

#include <daxa/daxa.inl>

struct Vertex
{
    daxa_f32vec3 position;
    daxa_f32vec3 color;
};
DAXA_DECL_BUFFER_PTR(Vertex)
