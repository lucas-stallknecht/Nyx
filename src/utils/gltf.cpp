#include "gltf.hpp"
#include "ktx.hpp"
#include "gpu_scene.inl"
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fastgltf/tools.hpp>
#include <future>
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

        mat4 S = glm::scale(mat4(1.0f), vec3(trs.scale.x(), trs.scale.y(), trs.scale.z()));

        return T * R * S;
    }

    std::optional<usize> find_cached_image_from_texture_impl(fastgltf::Asset const & asset,
                                                             fastgltf::Texture const & texture,
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
            return std::nullopt;
        }

        auto it = image_cache.find(img);
        if (it == image_cache.end())
        {
            return std::nullopt;
        }

        return it->second;
    }

} // namespace

namespace utils::gltf
{

    std::vector<MeshData> build_meshes(fastgltf::Asset & asset)
    {
        // Each glTF mesh is independent
        std::vector<std::future<MeshData>> futures = {};
        futures.reserve(asset.meshes.size());

        for (auto const & gltf_mesh : asset.meshes)
        {
            futures.push_back(std::async(
                std::launch::async,
                [&asset, &gltf_mesh]() -> MeshData
                {
                    MeshData mesh = {};
                    for (auto const & prim : gltf_mesh.primitives)
                    {
                        fastgltf::Attribute const * position_attr = prim.findAttribute("POSITION");
                        if (prim.type != fastgltf::PrimitiveType::Triangles)
                        {
                            fmt::println("WARN: skipping non-triangulated primitive in '{}'", gltf_mesh.name);
                            continue;
                        }
                        if (position_attr == prim.attributes.end())
                        {
                            fmt::println("WARN: skipping primitive without POSITION in '{}'", gltf_mesh.name);
                            continue;
                        }

                        fastgltf::Accessor const & position_accessor = asset.accessors[position_attr->accessorIndex];
                        if (!position_accessor.bufferViewIndex)
                        {
                            continue;
                        }

                        auto const vertex_offset = static_cast<u32>(mesh.vertices.size());
                        mesh.vertices.resize(vertex_offset + position_accessor.count); // zero-initialised

                        // copyFromAccessor does a single strided bulk-copy directly into the
                        // interleaved Vertex buffer
                        fastgltf::copyFromAccessor<fastgltf::math::fvec3, sizeof(Vertex)>(
                            asset, position_accessor, &mesh.vertices[vertex_offset].position);

                        if (auto const * attr = prim.findAttribute("NORMAL"); attr != prim.attributes.end())
                        {
                            fastgltf::copyFromAccessor<fastgltf::math::fvec3, sizeof(Vertex)>(
                                asset, asset.accessors[attr->accessorIndex], &mesh.vertices[vertex_offset].normal);
                        }

                        if (auto const * attr = prim.findAttribute("TEXCOORD_0"); attr != prim.attributes.end())
                        {
                            fastgltf::copyFromAccessor<fastgltf::math::fvec2, sizeof(Vertex)>(
                                asset, asset.accessors[attr->accessorIndex], &mesh.vertices[vertex_offset].uv);
                        }

                        if (auto const * attr = prim.findAttribute("TANGENT"); attr != prim.attributes.end())
                        {
                            fastgltf::copyFromAccessor<fastgltf::math::fvec4, sizeof(Vertex)>(
                                asset, asset.accessors[attr->accessorIndex], &mesh.vertices[vertex_offset].tangent);
                        }

                        if (!prim.indicesAccessor)
                        {
                            continue;
                        }

                        fastgltf::Accessor const & index_accessor = asset.accessors[prim.indicesAccessor.value()];
                        auto const index_offset = static_cast<u32>(mesh.indices.size());
                        mesh.indices.resize(mesh.indices.size() + index_accessor.count);

                        // vertex_offset must be added per-index, thus the use of the lambda
                        fastgltf::iterateAccessorWithIndex<u32>(
                            asset, index_accessor,
                            [&](u32 idx, usize i) { mesh.indices[index_offset + i] = idx + vertex_offset; });

                        u32 material_idx = 0;
                        if (prim.materialIndex)
                        {
                            material_idx = static_cast<u32>(prim.materialIndex.value()) + 1u;
                        }

                        // Local psoition optimum
                        vec3 min_pos = {};
                        vec3 max_pos = {};
                        if (position_accessor.min.has_value() && position_accessor.max.has_value())
                        {
                            auto const & min = position_accessor.min.value();
                            auto const & max = position_accessor.max.value();

                            min_pos = {static_cast<f32>(min.get<double>(0)), static_cast<f32>(min.get<double>(1)),
                                       static_cast<f32>(min.get<double>(2))};
                            max_pos = {static_cast<f32>(max.get<double>(0)), static_cast<f32>(max.get<double>(1)),
                                       static_cast<f32>(max.get<double>(2))};
                        }

                        mesh.sub_meshes.emplace_back(SubMesh{
                            .index_count = static_cast<u32>(index_accessor.count),
                            .index_offset = index_offset,
                            .material_idx = material_idx,
                            .bounds_origin = (max_pos + min_pos) / 2.0f,
                            .bounds_extents = (max_pos - min_pos) / 2.0f,
                        });
                    }
                    return mesh;
                }));
        }

        std::vector<MeshData> out = {};
        out.reserve(futures.size());
        for (auto & f : futures)
        {
            out.push_back(f.get());
        }
        return out;
    }

    BuildImagesResult build_images(fastgltf::Asset & asset, std::filesystem::path const & gltf_path)
    {
        usize const count = asset.images.size();
        BuildImagesResult out = {};
        out.images.resize(count); // pre-indexed 1:1 with asset.images

        // Each image is independent
        std::vector<std::future<ImageData>> futures = {};
        futures.reserve(count);
        for (usize i = 0; i < count; ++i)
        {
            fastgltf::Image & gltf_image = asset.images[i];
            futures.push_back(std::async(
                std::launch::async,
                [&gltf_image, &asset, &gltf_path]() -> ImageData
                {
                    ImageData result = {};
                    std::visit(
                        fastgltf::visitor{
                            [](auto &) {},
                            [&](fastgltf::sources::URI & uri)
                            {
                                std::filesystem::path image_path = gltf_path.parent_path().append(uri.uri.path());
                                auto img = utils::ktx::create_from_file(image_path.string().c_str());
                                if (img)
                                {
                                    result = std::move(*img);
                                }
                                else
                                {
                                    fmt::println("{}: {}", gltf_image.name, img.error());
                                }
                            },
                            [&](fastgltf::sources::BufferView & view)
                            {
                                fastgltf::BufferView & bv = asset.bufferViews[view.bufferViewIndex];
                                fastgltf::Buffer & buf = asset.buffers[bv.bufferIndex];
                                std::visit(
                                    fastgltf::visitor{[](auto const &) {},
                                                      [&](fastgltf::sources::Array & array)
                                                      {
                                                          auto const * bytes = reinterpret_cast<ktx_uint8_t const *>(
                                                              array.bytes.data() + bv.byteOffset);
                                                          auto img =
                                                              utils::ktx::create_from_memory(bytes, bv.byteLength);
                                                          if (img)
                                                          {
                                                              result = std::move(*img);
                                                          }
                                                          else
                                                          {
                                                              fmt::println("{}: {}", gltf_image.name, img.error());
                                                          }
                                                      }},
                                    buf.data);
                            }},
                        gltf_image.data);
                    return result;
                }));
        }

        // Collect in order and build the cache
        for (usize i = 0; i < count; ++i)
        {
            out.images[i] = futures[i].get();
            if (!out.images[i].data.empty())
            {
                out.image_cache.emplace(&asset.images[i], i);
            }
        }

        return out;
    }

    std::vector<GPUMaterial> build_materials(fastgltf::Asset & asset, LocalImageCache const & image_cache,
                                             std::vector<daxa::ImageId> const & images,
                                             std::vector<bool> & out_transparent)
    {
        // Resolve a texture index to its GPU view directly
        auto resolve = [&](std::size_t tex_idx) -> daxa_ImageViewId
        {
            fastgltf::Texture const & tex = asset.textures[tex_idx];
            auto idx = find_cached_image_from_texture_impl(asset, tex, image_cache);
            if (idx && !images[*idx].is_empty())
            {
                return images[*idx].default_view();
            }
            return {};
        };

        std::vector<GPUMaterial> out = {};
        out.reserve(1 + asset.materials.size());
        out.push_back({}); // sentinel, sub_meshes with no material use index 0
        out_transparent.push_back(false);

        for (auto const & gltf_mat : asset.materials)
        {
            auto & col = gltf_mat.pbrData.baseColorFactor;
            GPUMaterial mat = {};
            mat.base_color = {col.x(), col.y(), col.z(), col.w()};
            mat.metallic = gltf_mat.pbrData.metallicFactor;
            mat.roughness = gltf_mat.pbrData.roughnessFactor;
            mat.alpha_cutoff = gltf_mat.alphaMode == fastgltf::AlphaMode::Mask ? gltf_mat.alphaCutoff : 0.0f;

            if (gltf_mat.pbrData.baseColorTexture)
            {
                mat.base_color_texture = resolve(gltf_mat.pbrData.baseColorTexture->textureIndex);
            }
            if (gltf_mat.pbrData.metallicRoughnessTexture)
            {
                mat.metallic_roughness_texture = resolve(gltf_mat.pbrData.metallicRoughnessTexture->textureIndex);
            }
            if (gltf_mat.normalTexture)
            {
                mat.normal_texture = resolve(gltf_mat.normalTexture->textureIndex);
            }

            out.push_back(mat);
            out_transparent.push_back(gltf_mat.alphaMode == fastgltf::AlphaMode::Blend);
        }

        return out;
    }

    std::vector<Node> build_nodes(fastgltf::Asset & asset)
    {
        std::vector<Node> out = {};

        auto transform_of = [](fastgltf::Node const & node) -> mat4
        {
            return std::visit(
                [&](auto const & t) -> mat4
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
