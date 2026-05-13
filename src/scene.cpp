#include "scene.hpp"

#include "gpu_context.hpp"
#include <bit>
#include <glm/glm.hpp>
#include <fmt/core.h>

void Scene::clear()
{
    opaque_draws.clear();
    transparent_draws.clear();
}

void Scene::update(Camera const & camera)
{
    for (auto & draw : transparent_draws)
    {
        draw.distance_to_camera = glm::length(draw.world_position - camera.position);
    }

    std::sort(transparent_draws.begin(), transparent_draws.end(),
              [](TransparentDrawCall const & a, TransparentDrawCall const & b)
              { return a.distance_to_camera > b.distance_to_camera; });
}

void Scene::add_model(Model const & model)
{
    // Nodes in a glTF asset are guaranteed to be ordered so that a parent always
    // comes before its children in the list, making a single-pass accumulation safe.
    std::vector<mat4> world_transforms = {};
    world_transforms.reserve(model.nodes.size());

    for (auto const & node : model.nodes)
    {
        if (node.parent_idx < 0)
        {
            world_transforms.push_back(node.local_transform);
        }
        else
        {
            world_transforms.push_back(world_transforms[static_cast<usize>(node.parent_idx)] * node.local_transform);
        }

        if (node.mesh_idx < 0)
        {
            continue;
        }

        Mesh const & mesh = model.meshes[static_cast<usize>(node.mesh_idx)];
        mat4 const & transform = world_transforms.back();
        daxa::DeviceAddress vb_addr = gpu.device.device_address(mesh.vertex_buffer).value();
        daxa::DeviceAddress mb_addr = gpu.device.device_address(model.material_buffer).value();

        for (auto const & sub : mesh.sub_meshes)
        {
            DrawCall base_draw = {
                .vertex_buffer = vb_addr,
                .index_buffer = mesh.index_buffer,
                .material_buffer = mb_addr,
                .transform = std::bit_cast<daxa_f32mat4x4>(transform),
                .index_count = sub.index_count,
                .first_index = sub.index_offset,
                .material_idx = sub.material_idx,
            };

            bool transparent =
                sub.material_idx < model.material_transparent.size() && model.material_transparent[sub.material_idx];

            if (transparent)
            {
                auto transparent_draw_call = static_cast<TransparentDrawCall>(base_draw);
                transparent_draw_call.world_position = vec3(transform[3]);
                transparent_draws.push_back(transparent_draw_call);

                continue;
            }
            opaque_draws.push_back(base_draw);
        }
    }
}
