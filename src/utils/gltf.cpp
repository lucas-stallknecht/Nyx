#include "gltf.hpp"

#include "upload.hpp"
#include "format.hpp"
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
    using LocalImageCache = std::unordered_map<fastgltf::Image const *, daxa::ImageId>;

    void build_meshes(Model * model, fastgltf::Asset & asset)
    {
        model->meshes.reserve(asset.meshes.size());
        for (auto const & mesh : asset.meshes)
        {
            Mesh out_mesh = {};
            std::vector<Vertex> vertices;
            std::vector<u32> indices;

            for (auto & prim : mesh.primitives)
            {
                auto const * position_attr = prim.findAttribute("POSITION");
                assert(prim.type == fastgltf::PrimitiveType::Triangles && "Using a non-triangulated mesh");
                assert(position_attr != nullptr && position_attr != prim.attributes.end() &&
                       "Primitive must contain a position attribute");

                fastgltf::Accessor & position_accessor = asset.accessors[position_attr->accessorIndex];
                if (!position_accessor.bufferViewIndex.has_value())
                    continue;

                auto vertex_offset = static_cast<u32>(vertices.size());
                vertices.resize(vertex_offset + position_accessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, position_accessor,
                    [&](fastgltf::math::fvec3 pos, std::size_t idx)
                    {
                        vertices[idx + vertex_offset] = {
                            .position = {pos.x(), pos.y(), pos.z()},
                            .normal = {1.0f, 0.0f, 0.0f},
                            .uv = {0.0f, 0.0f},
                        };
                    });

                if (auto const * texcoord_attr = prim.findAttribute("TEXCOORD_0");
                    texcoord_attr != nullptr && texcoord_attr != prim.attributes.end())
                {
                    fastgltf::Accessor & texcoord_accessor = asset.accessors[texcoord_attr->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset, texcoord_accessor, [&](fastgltf::math::fvec2 uv, std::size_t idx)
                        { vertices[idx + vertex_offset].uv = {uv.x(), uv.y()}; });
                }

                if (auto const * normal_attr = prim.findAttribute("NORMAL");
                    normal_attr != nullptr && normal_attr != prim.attributes.end())
                {
                    fastgltf::Accessor & normal_accessor = asset.accessors[normal_attr->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, normal_accessor, [&](fastgltf::math::fvec3 normal, std::size_t idx)
                        { vertices[idx + vertex_offset].normal = {normal.x(), normal.y(), normal.z()}; });
                }

                // Should not happen thanks to the gltf_option
                if (!prim.indicesAccessor.has_value())
                    continue;

                fastgltf::Accessor & index_accessor = asset.accessors[prim.indicesAccessor.value()];
                auto index_offset = static_cast<u32>(indices.size());
                indices.reserve(indices.size() + index_accessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(asset, index_accessor, [&](std::uint32_t idx)
                                                         { indices.push_back(idx + vertex_offset); });

                i32 material_idx = -1;
                if (prim.materialIndex.has_value())
                {
                    material_idx = static_cast<i32>(prim.materialIndex.value());
                }

                out_mesh.sub_meshes.push_back({
                    .index_count = static_cast<u32>(index_accessor.count),
                    .index_offset = index_offset,
                    .material_idx = material_idx,
                });
            }

            out_mesh.vertex_buffer =
                create_and_upload_buffer(vertices.data(), {
                                                              .size = vertices.size() * sizeof(Vertex),
                                                              .name = std::string(mesh.name) + " vertex buffer",
                                                          });
            out_mesh.index_buffer =
                create_and_upload_buffer(indices.data(), {
                                                             .size = indices.size() * sizeof(u32),
                                                             .name = std::string(mesh.name) + " index buffer",
                                                         });
            model->meshes.push_back(std::move(out_mesh));
        };
    }

    mat4 trs_to_mat4(fastgltf::TRS const & trs)
    {
        mat4 T = glm::translate(mat4(1.0f), vec3(trs.translation.x(), trs.translation.y(), trs.translation.z()));

        quat q = quat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(), trs.rotation.z());
        mat4 R = glm::toMat4(q);

        mat4 S = glm::scale(glm::mat4(1.0f), vec3(trs.scale.x(), trs.scale.y(), trs.scale.z()));

        return T * R * S;
    }

    void traverse_node(Model * model, fastgltf::Asset & asset, fastgltf::Node const & node, i32 parent_idx)
    {
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
        auto node_idx = static_cast<i32>(model->nodes.size());
        model->nodes.push_back({
            .local_transform = transform,
            .parent_idx = parent_idx,
            .mesh_idx = static_cast<i32>(node.meshIndex.value_or(-1)),
        });
        for (auto child_idx : node.children)
        {
            traverse_node(model, asset, asset.nodes.at(child_idx), node_idx);
        }
    }

    void build_images(Model * model, fastgltf::Asset & asset, LocalImageCache & image_cache)
    {
        for (usize image_idx = 0; image_idx < asset.images.size(); ++image_idx)
        {
            fastgltf::Image & image = asset.images[image_idx];
            std::visit(
                fastgltf::visitor{
                    [](auto & image_data) {},
                    [&](fastgltf::sources::URI & uri)
                    {
                        // TODO: Handle this
                    },
                    [&](fastgltf::sources::BufferView & view)
                    {
                        fastgltf::BufferView & buffer_view = asset.bufferViews.at(view.bufferViewIndex);
                        fastgltf::Buffer & buffer = asset.buffers.at(buffer_view.bufferIndex);
                        std::visit(
                            fastgltf::visitor{
                                [](auto const & buffer) {},
                                [&](fastgltf::sources::Array & array)
                                {
                                    auto const * bytes = reinterpret_cast<ktx_uint8_t const *>(array.bytes.data() +
                                                                                               buffer_view.byteOffset);
                                    ktx_size_t size = buffer_view.byteLength;

                                    ktxTexture2 * texture = nullptr;
                                    KTX_error_code result = ktxTexture2_CreateFromMemory(
                                        bytes, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

                                    if (result != KTX_SUCCESS)
                                    {
                                        fmt::println("Failed to load KTX texture {}: {}", image.name,
                                                     ktxErrorString(result));
                                        return;
                                    }

                                    if (ktxTexture2_NeedsTranscoding(texture))
                                    {
                                        KTX_error_code transcode_result =
                                            ktxTexture2_TranscodeBasis(texture, KTX_TTF_RGBA32, 0);

                                        if (transcode_result != KTX_SUCCESS)
                                        {
                                            fmt::println("Failed to transcode texture");
                                            ktxTexture2_Destroy(texture);
                                            return;
                                        }
                                    }
                                    // Upload
                                    std::vector<ImageUploadMipInfo> mip_infos = {};
                                    for (u32 i = 0; i < texture->numLevels; i++)
                                    {
                                        ktx_size_t mip_offset = 0;
                                        KTX_error_code ret = ktxTexture2_GetImageOffset(texture, i, 0, 0, &mip_offset);
                                        if (ret != KTX_SUCCESS)
                                        {
                                            fmt::println("Failed to get image offset {}", ktxErrorString(ret));
                                            break;
                                        }

                                        mip_infos.push_back({
                                            .offset = mip_offset,
                                            .extent =
                                                {
                                                    .x = texture->baseWidth >> i,
                                                    .y = texture->baseHeight >> i,
                                                    .z = 1,
                                                },
                                        });
                                    }

                                    daxa::ImageId new_image = create_and_upload_image(
                                        texture->pData, texture->dataSize, mip_infos,
                                        {
                                            .dimensions = 2,
                                            .format = vk_to_daxa_format(texture->vkFormat),
                                            .size = {.x = texture->baseWidth, .y = texture->baseHeight, .z = 1},
                                            .mip_level_count = texture->numLevels,
                                            .array_layer_count = 1,
                                            .sample_count = 1,
                                            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST |
                                                     daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                                                     daxa::ImageUsageFlagBits::SHADER_STORAGE,
                                            .name = std::string(image.name) + " texture",
                                        });
                                    image_cache.emplace(&image, new_image);
                                    model->images.push_back(new_image);

                                    ktxTexture2_Destroy(texture);
                                }},
                            buffer.data);
                    }},
                image.data);
        }
    }

    void build_materials(Model * model, fastgltf::Asset & asset, LocalImageCache const & image_cache)
    {
        model->materials.reserve(asset.materials.size());
        for (auto const & mat : asset.materials)
        {
            Material out_mat;

            if (mat.pbrData.baseColorTexture)
            {
                auto const & tex_info = *mat.pbrData.baseColorTexture;
                auto const & texture = asset.textures[tex_info.textureIndex];

                fastgltf::Image const * img = nullptr;

                if (texture.imageIndex.has_value())
                    img = &asset.images[texture.imageIndex.value()];
                else if (texture.basisuImageIndex.has_value())
                    img = &asset.images[texture.basisuImageIndex.value()];
                else if (texture.ddsImageIndex.has_value())
                    img = &asset.images[texture.ddsImageIndex.value()];
                else if (texture.webpImageIndex.has_value())
                    img = &asset.images[texture.webpImageIndex.value()];

                if (img)
                {
                    auto it = image_cache.find(img);
                    if (it != image_cache.end())
                    {
                        out_mat.base_color_texture = it->second;
                    }
                }
            }

            model->materials.push_back(out_mat);
        }
    }
} // namespace

void build_gltf_model(Model * model, fastgltf::Asset & asset)
{
    LocalImageCache image_cache = {};
    build_images(model, asset, image_cache);
    build_materials(model, asset, image_cache);
    build_meshes(model, asset);
    for (auto & scene : asset.scenes)
    {
        for (auto & node_idx : scene.nodeIndices)
        {
            fastgltf::Node & root = asset.nodes.at(node_idx);
            traverse_node(model, asset, root, -1);
        }
    }
}
