#pragma once

#include "include/model.hpp"
#include <daxa/daxa.hpp>
#include <vector>

struct DrawCall
{
    daxa::DeviceAddress vertex_buffer = 0;
    daxa::BufferId index_buffer = {};
    daxa::DeviceAddress material_buffer = 0;
    daxa_f32mat4x4 transform = {};
    u32 index_count = 0;
    u32 first_index = 0;
    u32 material_idx = 0;
};

struct Scene
{
    std::vector<DrawCall> opaque_draws = {};

    void clear();
    void add_model(Model const & model);
};
