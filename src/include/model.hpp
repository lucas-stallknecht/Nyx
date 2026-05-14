#pragma once

#include "types.hpp"
#include "gpu_scene.inl"
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
    vec3 bounds_origin = {};
    vec3 bounds_extents = {};
};

struct MeshData
{
    std::vector<Vertex> vertices = {};
    std::vector<u32> indices = {};
    std::vector<SubMesh> sub_meshes = {};
};

struct Mesh
{
    daxa::BufferId vertex_buffer;
    daxa::BufferId index_buffer;
    std::vector<SubMesh> sub_meshes = {};
};

struct ImageMipInfo
{
    usize offset = 0;
    daxa::Extent3D extent = {};
};

struct ImageData
{
    std::vector<std::byte> data = {};
    std::vector<ImageMipInfo> mip_infos = {};
    daxa::ImageInfo info = {};
};

struct Model
{
    std::vector<Node> nodes = {};
    std::vector<Mesh> meshes = {};
    daxa::BufferId material_buffer;
    std::vector<bool> material_transparent = {};
    std::vector<daxa::ImageId> images = {};
};
