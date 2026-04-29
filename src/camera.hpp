#pragma once

#include "types.hpp"

struct Camera
{
    static constexpr f32 MOVE_SPEED = 1.0f;
    static constexpr f32 LOOK_SENSITIVITY = 0.5f;
    static constexpr f32 NEAR = 0.01f;
    static constexpr f32 FAR = 100.0f;
    f32 fov = 60.0f;
    vec3 position = {0.0f, 0.0f, 2.0f};
    quat rotation = {};

    mat4 get_proj(f32 aspect_ratio);
    mat4 get_view();
    vec3 get_forward() { return rotation * vec3(0.0f, 0.0f, -1.0f); };
    vec3 get_up() { return rotation * vec3(0.0f, 1.0f, 0.0f); };
    vec3 get_right() { return rotation * vec3(1.0f, 0.0f, 0.0f); };
    void move(f32 dfor, f32 dup, f32 dright);
    void rotate(vec2 delta);
};
