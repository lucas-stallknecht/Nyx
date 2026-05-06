#pragma once

#include <daxa/daxa.inl>

#define MAX_POINT_LIGHTS 6

struct CameraInfo
{
    daxa_f32mat4x4 proj;
    daxa_f32mat4x4 view;
};
DAXA_DECL_BUFFER_PTR(CameraInfo);

struct PointLight
{
    daxa_f32vec3 position;
    daxa_f32 linear;
    daxa_f32vec3 color;
    daxa_f32 quadratic;

#ifdef __cplusplus
    PointLight() : position(0.0f, 0.0f, 0.0f), linear(0.7f), color(1.0f, 1.0f, 1.0f), quadratic(1.8f) {}
#endif
};

struct LightInfo
{
    daxa_f32vec3 sun_dir;
    daxa_f32vec3 sun_color;
    daxa_f32mat4x4 sun_matrix;
    daxa_u32 num_point_lights;
    PointLight point_lights[MAX_POINT_LIGHTS];
};
DAXA_DECL_BUFFER_PTR(LightInfo);

struct GlobalRenderingBuffer
{
    daxa_SamplerId default_linear_sampler;
    daxa_SamplerId shadow_sampler;
    daxa_BufferPtr(CameraInfo) camera_buffer;
    daxa_BufferPtr(LightInfo) light_buffer;
};
DAXA_DECL_BUFFER_PTR(GlobalRenderingBuffer);

struct Vertex
{
    daxa_f32vec3 position;
    daxa_f32vec3 normal;
    daxa_f32vec4 tangent;
    daxa_f32vec2 uv;
};
DAXA_DECL_BUFFER_PTR(Vertex)

struct GPUMaterial
{
    daxa_f32vec3 base_color;
    daxa_f32 metallic;
    daxa_f32 roughness;
    daxa_ImageViewId base_color_texture;
    daxa_ImageViewId metallic_roughness_texture;
    daxa_ImageViewId normal_texture;
};
DAXA_DECL_BUFFER_PTR(GPUMaterial)
