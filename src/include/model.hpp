#pragma once

#include "types.hpp"
#include <daxa/daxa.hpp>

struct Node
{
    mat4 local_transform = mat4(1.0f);
    i32 parent_idx = -1;
    i32 mesh_idx = -1;
};

struct SubMesh
{
    u32 index_count = 0;
    u32 index_offset = 0;
    u32 material_idx = 0;
};

struct Mesh
{
    daxa::BufferId vertex_buffer;
    daxa::BufferId index_buffer;
    std::vector<SubMesh> sub_meshes = {};
};

struct Material
{
    daxa_f32vec3 base_color = {1.0f, 1.0f, 1.0f};
    daxa_f32 metallic = 0.0f;
    daxa_f32 roughness = 1.0f;
    daxa::ImageId base_color_texture;
    daxa::ImageId metallic_roughness_texture;
    daxa::ImageId normal_texture;
};

struct Model
{
    Handle handle = {};
    std::vector<Node> nodes = {};
    std::vector<Mesh> meshes = {};
    daxa::BufferId material_buffer;
    std::vector<Material> materials = {};
    std::vector<daxa::ImageId> images = {};
};
