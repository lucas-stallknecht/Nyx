#pragma once

#include "types.hpp"

struct Camera
{
    static constexpr f32 near = 0.01f;
    static constexpr f32 far = 100.0f;
    f32                  move_speed = 4.0f;
    f32                  look_sensitivity = 0.2f;
    f32                  fov = 60.0f;
    vec3                 position = {7.5f, 1.5f, -0.25f};
    quat                 rotation = {0.707f, 0.0f, 0.707f, 0.0f};

    void update_proj(f32 aspect_ratio);
    mat4 get_proj() const { return proj; };
    mat4 get_view() const;
    vec3 get_forward() const { return rotation * vec3(0.0f, 0.0f, -1.0f); };
    vec3 get_up() const { return rotation * vec3(0.0f, 1.0f, 0.0f); };
    vec3 get_right() const { return rotation * vec3(1.0f, 0.0f, 0.0f); };
    void move_forward(f32 d);
    void move_up(f32 d);
    void move_right(f32 d);
    void rotate(vec2 delta);

  private:
    mat4 proj = {};
};
