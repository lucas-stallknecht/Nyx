#include "gltf.hpp"
#include "ktx.hpp"
#include "shared.inl"
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fastgltf/tools.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ktx.h>

namespace
{

    mat4 trs_to_mat4_impl(fastgltf::TRS const & trs)
    {
        mat4 T = glm::translate(mat4(1.0f), vec3(trs.translation.x(), trs.translation.y(), trs.translation.z()));

        quat q = quat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
        mat4 R = glm::toMat4(q);

        mat4 S = glm::scale(glm::mat4(1.0f), vec3(trs.scale.x(), trs.scale.y(), trs.scale.z()));

        return T * R * S;
    }

    u32 find_cached_image_from_texture_impl(fastgltf::Asset const & asset, fastgltf::Texture const & texture,
                                            utils::gltf::LocalImageCache const & image_cache)
    {
        fastgltf::Image const * img = nullptr;

        if (texture.imageIndex)
        {
            img = &asset.images[texture.imageIndex.value()];
        }
        else if (texture.basisuImageIndex)
        {
            img = &asset.images[texture.basisuImageIndex.value()];
        }
        else if (texture.ddsImageIndex)
        {
            img = &asset.images[texture.ddsImageIndex.value()];
        }
        else if (texture.webpImageIndex)
        {
            img = &asset.images[texture.webpImageIndex.value()];
        }

        if (!img)
        {
            return {};
        }

        auto it = image_cache.find(img);
        if (it == image_cache.end())
        {
            return {};
        }

        return it->second;
    }

} // namespace

namespace utils::gltf
{

    std::vector<MeshData> build_meshes(fastgltf::Asset & asset)
    {
        std::vector<MeshData> out = {};
        out.reserve(asset.meshes.size());

        for (auto const & gltf_mesh : asset.meshes)
        {
            MeshData mesh = {};
            for (auto const & prim : gltf_mesh.primitives)
            {
                fastgltf::Attribute const * position_attr = prim.findAttribute("POSITION");
                assert(prim.type == fastgltf::PrimitiveType::Triangles && "Using a non-triangulated mesh");
                assert(position_attr != prim.attributes.end() && "Primitive must contain a position attribute");

                fastgltf::Accessor & position_accessor = asset.accessors[position_attr->accessorIndex];
                if (!position_accessor.bufferViewIndex)
                {
                    continue;
                }

                auto vertex_offset = static_cast<u32>(mesh.vertices.size());
                mesh.vertices.resize(vertex_offset + position_accessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, position_accessor,
                    [&](fastgltf::math::fvec3 pos, std::size_t idx)
                    {
                        mesh.vertices[idx + vertex_offset] = {
                            .position = {pos.x(), pos.y(), pos.z()},
                            .normal = {1.0f, 0.0f, 0.0f},
                            .tangent = {},
                            .uv = {0.0f, 0.0f},
                        };
                    });

                if (fastgltf::Attribute const * texcoord_attr = prim.findAttribute("TEXCOORD_0");
                    texcoord_attr != prim.attributes.end())
                {
                    fastgltf::Accessor & texcoord_accessor = asset.accessors[texcoord_attr->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset, texcoord_accessor, [&](fastgltf::math::fvec2 uv, std::size_t idx)
                        { mesh.vertices[idx + vertex_offset].uv = {uv.x(), uv.y()}; });
                }

                if (fastgltf::Attribute const * normal_attr = prim.findAttribute("NORMAL");
                    normal_attr != prim.attributes.end())
                {
                    fastgltf::Accessor & normal_accessor = asset.accessors[normal_attr->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, normal_accessor, [&](fastgltf::math::fvec3 normal, std::size_t idx)
                        { mesh.vertices[idx + vertex_offset].normal = {normal.x(), normal.y(), normal.z()}; });
                }

                if (fastgltf::Attribute const * tangent_attr = prim.findAttribute("TANGENT");
                    tangent_attr != prim.attributes.end())
                {
                    fastgltf::Accessor & tangent_accessor = asset.accessors[tangent_attr->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                        asset, tangent_accessor,
                        [&](fastgltf::math::fvec4 tangent, std::size_t idx)
                        {
                            mesh.vertices[idx + vertex_offset].tangent = {tangent.x(), tangent.y(), tangent.z(),
                                                                          tangent.w()};
                        });
                }

                // Should not happen thanks to the gltf_option
                if (!prim.indicesAccessor)
                {
                    continue;
                }

                fastgltf::Accessor const & index_accessor = asset.accessors[prim.indicesAccessor.value()];
                auto index_offset = static_cast<u32>(mesh.indices.size());
                mesh.indices.reserve(mesh.indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(asset, index_accessor, [&](std::uint32_t idx)
                                                         { mesh.indices.push_back(idx + vertex_offset); });

                // We keep a sentinel material at index 0 in model->materials, so shift glTF material
                // indices by +1 when storing them in SubMesh to point at the correct material.
                u32 material_idx = 0;
                if (prim.materialIndex)
                {
                    material_idx = static_cast<u32>(prim.materialIndex.value()) + 1u;
                }
                mesh.sub_meshes.emplace_back(SubMesh{
                    .index_count = static_cast<u32>(index_accessor.count),
                    .index_offset = index_offset,
                    .material_idx = material_idx,
                });
            }
            out.push_back(mesh);
        };

        return out;
    }

    BuildImagesResult build_images(fastgltf::Asset & asset)
    {
        BuildImagesResult out = {};
        for (u32 image_idx = 0; image_idx < asset.images.size(); ++image_idx)
        {
            ImageData image = {};
            fastgltf::Image & gltf_image = asset.images[image_idx];
            std::visit(
                fastgltf::visitor{
                    [](auto & /* image_data */) {},
                    [&](fastgltf::sources::URI & uri)
                    {
                        std::expected<ImageData, std::string> image = utils::ktx::create_from_file(uri.uri.c_str());

                        if (!image)
                        {
                            fmt::println("{}: {}", gltf_image.name, image.error());
                            return;
                        }

                        auto out_idx = static_cast<u32>(out.images.size());
                        out.image_cache.emplace(&gltf_image, out_idx);
                        out.images.push_back(std::move(image.value()));
                    },
                    [&](fastgltf::sources::BufferView & view)
                    {
                        fastgltf::BufferView & buffer_view = asset.bufferViews[view.bufferViewIndex];
                        fastgltf::Buffer & buffer = asset.buffers[buffer_view.bufferIndex];
                        std::visit(fastgltf::visitor{[](auto const & /* buffer */) {},
                                                     [&](fastgltf::sources::Array & array)
                                                     {
                                                         auto const * bytes = reinterpret_cast<ktx_uint8_t const *>(
                                                             array.bytes.data() + buffer_view.byteOffset);
                                                         ktx_size_t size = buffer_view.byteLength;

                                                         std::expected<ImageData, std::string> image =
                                                             utils::ktx::create_from_memory(bytes, size);

                                                         if (!image)
                                                         {
                                                             fmt::println("{}: {}", gltf_image.name, image.error());
                                                             return;
                                                         }

                                                         auto out_idx = static_cast<u32>(out.images.size());
                                                         out.image_cache.emplace(&gltf_image, out_idx);
                                                         out.images.push_back(std::move(image.value()));
                                                     }},
                                   buffer.data);
                    }},
                gltf_image.data);
        }

        return out;
    }

    std::vector<MaterialData> build_materials(fastgltf::Asset & asset, LocalImageCache const & image_cache)
    {
        std::vector<MaterialData> out = {};
        out.reserve(1 + asset.materials.size());
        // Sentinel material
        out.push_back({});

        for (auto const & gltf_mat : asset.materials)
        {
            auto & col = gltf_mat.pbrData.baseColorFactor;
            MaterialData mat = {
                .base_color = {col.x(), col.y(), col.z()},
                .metallic = gltf_mat.pbrData.metallicFactor,
                .roughness = gltf_mat.pbrData.roughnessFactor,
            };

            if (gltf_mat.pbrData.baseColorTexture)
            {
                fastgltf::Texture texture = asset.textures[gltf_mat.pbrData.baseColorTexture->textureIndex];
                mat.base_color_texture = find_cached_image_from_texture_impl(asset, texture, image_cache);
            }

            if (gltf_mat.pbrData.metallicRoughnessTexture)
            {
                fastgltf::Texture texture = asset.textures[gltf_mat.pbrData.metallicRoughnessTexture->textureIndex];
                mat.metallic_roughness_texture = find_cached_image_from_texture_impl(asset, texture, image_cache);
            }

            if (gltf_mat.normalTexture)
            {
                fastgltf::Texture texture = asset.textures[gltf_mat.normalTexture->textureIndex];
                mat.normal_texture = find_cached_image_from_texture_impl(asset, texture, image_cache);
            }

            out.push_back(mat);
        }

        return out;
    }

    std::vector<Node> build_nodes(fastgltf::Asset & asset)
    {
        std::vector<Node> out = {};

        auto transform_of = [](fastgltf::Node const & node) -> glm::mat4
        {
            return std::visit(
                [&](auto const & t) -> glm::mat4
                {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, fastgltf::TRS>)
                    {
                        return trs_to_mat4_impl(t);
                    }
                    else
                    {
                        return glm::make_mat4(&t[0][0]);
                    }
                },
                node.transform);
        };

        auto traverse = [&](this auto const & self, fastgltf::Node const & node, i32 parent_idx) -> void
        {
            i32 node_idx = static_cast<i32>(out.size());
            out.push_back({
                .local_transform = transform_of(node),
                .parent_idx = parent_idx,
                .mesh_idx = static_cast<i32>(node.meshIndex.value_or(-1)),
            });

            for (auto child_idx : node.children)
            {
                self(asset.nodes[child_idx], node_idx);
            }
        };

        for (auto const & scene : asset.scenes)
        {
            for (auto root_idx : scene.nodeIndices)
            {
                traverse(asset.nodes[root_idx], -1);
            }
        }

        return out;
    }
} // namespace utils::gltf
