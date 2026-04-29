#pragma once

#include <daxa/daxa.inl>

struct CameraInfo
{
    daxa_f32mat4x4 proj;
    daxa_f32mat4x4 view;
};
DAXA_DECL_BUFFER_PTR(CameraInfo);

struct Vertex
{
    daxa_f32vec3 position;
    daxa_f32vec3 color;
};
DAXA_DECL_BUFFER_PTR(Vertex)
