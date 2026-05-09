#pragma once
#include <daxa/daxa.inl>

#define MAX_POINT_LIGHTS 6

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

struct GPULightInfo
{
    daxa_f32vec3 dir_light_direction;
    daxa_f32 dir_light_intensity;
    daxa_f32vec3 dir_light_color;
    daxa_f32mat4x4 dir_light_matrix;
    daxa_u32 num_point_lights;
    PointLight point_lights[MAX_POINT_LIGHTS];
};
DAXA_DECL_BUFFER_PTR(GPULightInfo);
