#include "scene.hpp"

#include "gpu_context.hpp"
#include <bit>
#include <glm/glm.hpp>

void Scene::clear()
{
    opaque_draws.clear();
    transparent_draws.clear();
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
        daxa::DeviceAddress const vb_addr = gpu.device.device_address(mesh.vertex_buffer).value();
        daxa::DeviceAddress const mb_addr = gpu.device.device_address(model.material_buffer).value();

        for (auto const & sub : mesh.sub_meshes)
        {
            DrawCall dc = {
                .vertex_buffer = vb_addr,
                .index_buffer = mesh.index_buffer,
                .material_buffer = mb_addr,
                .transform = std::bit_cast<daxa_f32mat4x4>(transform),
                .index_count = sub.index_count,
                .first_index = sub.index_offset,
                .material_idx = sub.material_idx,
            };

            bool const transparent =
                sub.material_idx < model.material_transparent.size() && model.material_transparent[sub.material_idx];
            (transparent ? transparent_draws : opaque_draws).push_back(dc);
        }
    }
}
