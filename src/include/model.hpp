#pragma once

#include "types.hpp"
#include "shared.inl"
#include <daxa/daxa.hpp>

struct Node
{
    mat4 local_matrix = mat4(1.0f);
    i32 parent_idx = -1;
};

struct MeshPrimitive
{
    u32 index_count = 0;
    u32 index_offset = 0;
    i32 material_idx = -1;
};

struct Mesh
{
    i32 node_idx = -1;
    daxa::BufferId vertex_buffer;
    daxa::BufferId index_buffer;
    std::vector<MeshPrimitive> primitives = {};
};

struct Model
{
    Handle handle = {};
    std::vector<Node> nodes = {};
    std::vector<Mesh> meshes = {};
    std::vector<MeshPrimitive> primitives = {};
    std::vector<Material> materials = {};
};
