#pragma once

#include "types.hpp"
#include "shared.inl"
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

struct MaterialData
{
    vec3 base_color = vec3(1.0f);
    f32 metallic = 0.0f;
    f32 roughness = 1.0f;
    std::optional<u32> base_color_texture = {};
    std::optional<u32> metallic_roughness_texture = {};
    std::optional<u32> normal_texture = {};
};

struct Material
{
    vec3 base_color = vec3(1.0f);
    f32 metallic = 0.0f;
    f32 roughness = 1.0f;
    daxa::ImageId base_color_texture;
    daxa::ImageId metallic_roughness_texture;
    daxa::ImageId normal_texture;
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
    Handle handle = {};
    std::vector<Node> nodes = {};
    std::vector<Mesh> meshes = {};
    daxa::BufferId material_buffer;
    std::vector<Material> materials = {};
    std::vector<daxa::ImageId> images = {};
};
