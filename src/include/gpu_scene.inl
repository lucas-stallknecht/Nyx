#pragma once
#include <daxa/daxa.inl>

struct Vertex
{
    daxa_f32vec3 position;
    daxa_f32vec3 normal;
    daxa_f32vec4 tangent;
    daxa_f32vec2 uv;
};
DAXA_DECL_BUFFER_PTR(Vertex)

struct GPUCamera
{
    daxa_f32mat4x4 proj;
    daxa_f32mat4x4 inv_proj;
    daxa_f32mat4x4 view;
    daxa_f32mat4x4 inv_view;
    daxa_f32vec3   position;
};

struct GPUMaterial
{
    daxa_f32vec4     base_color;
    daxa_f32         metallic;
    daxa_f32         roughness;
    daxa_f32         alpha_cutoff; // >0 = MASK, 0 = OPAQUE/BLEND (handled by pipeline)
    daxa_ImageViewId base_color_texture;
    daxa_ImageViewId metallic_roughness_texture;
    daxa_ImageViewId normal_texture;
};
DAXA_DECL_BUFFER_PTR(GPUMaterial)
