#pragma once

#include "include/model.hpp"
#include "camera.hpp"
#include <daxa/daxa.hpp>
#include <vector>

struct DrawCall
{
    bool                culled = false;
    daxa::BufferId      index_buffer = {};
    daxa::DeviceAddress material_buffer = 0;
    daxa::DeviceAddress vertex_buffer = 0;
    daxa_f32mat4x4      transform = {};
    u32                 index_count = 0;
    u32                 first_index = 0;
    u32                 material_idx = 0;
    vec3                aabb_min = {};
    vec3                aabb_max = {};
};

struct TransparentDrawCall : DrawCall
{
    f32 distance_to_camera = 0.0f;
};

struct DebugDrawCall
{
    daxa_b32       culled = false;
    daxa_f32mat4x4 transform = {};
};

struct Scene
{
    bool                             draw_aabb = false;
    std::vector<DrawCall>            opaque_draws = {};
    std::vector<TransparentDrawCall> transparent_draws = {};
    std::vector<DebugDrawCall>       debug_draws = {};

    void update(Camera const & camera);
    void clear();
    int  add_model(Model const & model);
};
