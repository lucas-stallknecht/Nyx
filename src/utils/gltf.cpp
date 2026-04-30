#include "gltf.hpp"
#include "upload.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fastgltf/tools.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace
{
    void build_mesh(Model * model, fastgltf::Asset & asset, fastgltf::Mesh const & mesh, i32 node_idx)
    {
        Mesh out_mesh = {};
        std::vector<Vertex> vertices;
        std::vector<u32> indices;

        for (auto & prim : mesh.primitives)
        {
            fastgltf::Attribute const * positin_it = prim.findAttribute("POSITION");
            assert(prim.type == fastgltf::PrimitiveType::Triangles && "Using a non-triangulated mesh");
            assert(positin_it != prim.attributes.end() && "Primitive must conatin a position attribute");

            fastgltf::Accessor & position_accessor = asset.accessors[positin_it->accessorIndex];
            if (!position_accessor.bufferViewIndex.has_value())
                continue;

            auto vertex_offset = static_cast<u32>(vertices.size());
            vertices.resize(vertex_offset + position_accessor.count);

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, position_accessor,
                                                                      [&](fastgltf::math::fvec3 pos, std::size_t idx)
                                                                      {
                                                                          vertices[idx + vertex_offset] = {
                                                                              .position = {pos.x(), pos.y(), pos.z()},
                                                                              .normal = {1.0f, 0.0f, 0.0f},
                                                                              .uv = {0.0f, 0.0f},
                                                                          };
                                                                      });

            if (auto const * texcoord = prim.findAttribute("TEXCOORD_0"); texcoord != prim.attributes.end())
            {
                fastgltf::Accessor & texcoord_accessor = asset.accessors[texcoord->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    asset, texcoord_accessor, [&](fastgltf::math::fvec2 uv, std::size_t idx)
                    { vertices[idx + vertex_offset].uv = {uv.x(), uv.y()}; });
            }

            if (auto const * normals = prim.findAttribute("NORMAL"); normals != prim.attributes.end())
            {
                fastgltf::Accessor & normal_accessor = asset.accessors[normals->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, normal_accessor, [&](fastgltf::math::fvec3 normal, std::size_t idx)
                    { vertices[idx + vertex_offset].normal = {normal.x(), normal.y(), normal.z()}; });
            }

            fastgltf::Accessor & index_accessor = asset.accessors[prim.indicesAccessor.value()];
            auto index_offset = static_cast<u32>(indices.size());
            indices.reserve(indices.size() + index_accessor.count);

            fastgltf::iterateAccessor<std::uint32_t>(asset, index_accessor, [&](std::uint32_t idx)
                                                     { indices.push_back(idx + vertex_offset); });

            out_mesh.primitives.push_back({
                .index_count = static_cast<u32>(index_accessor.count),
                .index_offset = index_offset,
            });
        }
        out_mesh.node_idx = node_idx,
        out_mesh.vertex_buffer = create_and_upload_buffer(vertices.data(), {.size = vertices.size() * sizeof(Vertex)}),
        out_mesh.index_buffer = create_and_upload_buffer(indices.data(), {.size = indices.size() * sizeof(u32)}),
        model->meshes.push_back(std::move(out_mesh));
    }

    mat4 trs_to_mat4(fastgltf::TRS const & trs)
    {
        mat4 T = glm::translate(mat4(1.0f), vec3(trs.translation.x(), trs.translation.y(), trs.translation.z()));

        quat q = quat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
        mat4 R = glm::toMat4(q);

        mat4 S = glm::scale(glm::mat4(1.0f), vec3(trs.scale.x(), trs.scale.y(), trs.scale.z()));

        return T * R * S;
    }

    void traverse_node(Model * model, fastgltf::Asset & asset, fastgltf::Node const & node)
    {
        if (node.meshIndex.has_value())
        {
            fastgltf::Mesh & mesh = asset.meshes[node.meshIndex.value()];
            auto node_idx = static_cast<i32>(model->nodes.size());
            build_mesh(model, asset, mesh, node_idx);
        }
        glm::mat4 transform = std::visit(
            [&](auto const & t) -> glm::mat4
            {
                using T = std::decay_t<decltype(t)>;
                if constexpr (std::is_same_v<T, fastgltf::TRS>)
                {
                    return trs_to_mat4(t);
                }
                else
                {
                    return glm::make_mat4(&t[0][0]);
                }
            },
            node.transform);
        auto parent_idx = static_cast<i32>(model->nodes.size());
        model->nodes.push_back({
            .local_matrix = transform,
            .parent_idx = parent_idx,
        });
        for (auto child_idx : node.children)
        {
            traverse_node(model, asset, asset.nodes.at(child_idx));
        }
    }
} // namespace

void build_gltf_model(Model * model, fastgltf::Asset & asset)
{
    // For now, build meshes from all the roots
    fastgltf::Scene & scene = asset.scenes.at(asset.defaultScene.value());
    fastgltf::Node & root = asset.nodes.at(scene.nodeIndices[0]);
    traverse_node(model, asset, root);
}
