#pragma once

#include "gpu_scene.inl"

#define MAX_POINT_LIGHTS 6

struct PointLight
{
    daxa_f32vec3 position;
    daxa_f32 intensity;
    daxa_f32vec3 color;
    daxa_f32 radius;

#ifdef __cplusplus
    PointLight() : position(0.0f, 0.0f, 0.0f), intensity(1.0f), color(1.0f, 1.0f, 1.0f), radius(0.2f) {}
#endif
};

struct GPUFrameData
{
    daxa_i32 debug_view;

    daxa_f32vec3 ambient_light_color;
    daxa_f32 ambient_light_intensity;
    daxa_f32vec3 dir_light_direction;
    daxa_f32 dir_light_intensity;
    daxa_f32vec3 dir_light_color;
    daxa_f32mat4x4 dir_light_matrix;
    daxa_u32 num_point_lights;
    PointLight point_lights[MAX_POINT_LIGHTS];

    daxa_b32 pcf_enabled;

    daxa_f32 exposure;

    daxa_b32 ssao_enabled;
    daxa_f32 ssao_radius;
    daxa_f32 ssao_bias;

    daxa_b32 ssr_enabled;
    daxa_f32 ssr_min_mask;
    daxa_f32 ssr_max_mask;
    daxa_f32 ssr_reflection_intensity;
    daxa_f32 ssr_screen_edge_fade;
    daxa_i32 ssr_num_samples;
    daxa_f32 ssr_max_distance;
};
DAXA_DECL_BUFFER_PTR(GPUFrameData);

struct GPUGlobals
{
    daxa_SamplerId default_linear_sampler;
    daxa_SamplerId default_nearest_sampler;
    daxa_SamplerId shadow_sampler;
    daxa_BufferPtr(GPUCamera) camera_buffer;
    daxa_BufferPtr(GPUFrameData) frame_data_buffer;
};
DAXA_DECL_BUFFER_PTR(GPUGlobals);
