#pragma once

#include "types.hpp"

struct Camera
{
    static constexpr f32 move_speed = 1.0f;
    static constexpr f32 look_sensitivity = 0.25f;
    static constexpr f32 near = 0.01f;
    static constexpr f32 plane = 100.0f;
    f32 fov = 60.0f;
    vec3 position = {4.0f, 1.0f, 0.0f};
    quat rotation = {0.707f, 0.0f, 0.707f, 0.0f};

    mat4 get_proj(f32 aspect_ratio);
    mat4 get_view();
    vec3 get_forward() { return rotation * vec3(0.0f, 0.0f, -1.0f); };
    vec3 get_up() { return rotation * vec3(0.0f, 1.0f, 0.0f); };
    vec3 get_right() { return rotation * vec3(1.0f, 0.0f, 0.0f); };
    void move_forward(f32 d);
    void move_up(f32 d);
    void move_right(f32 d);
    void rotate(vec2 delta);
};
