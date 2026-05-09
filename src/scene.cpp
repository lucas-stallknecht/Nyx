#include "scene.hpp"

#include "gpu_context.hpp"
#include <bit>
#include <glm/glm.hpp>

void Scene::clear() { opaque_draws.clear(); }

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

        Mesh const & mesh = model.meshes[static_cast<u32>(node.mesh_idx)];
        mat4 const & transform = world_transforms.back();
        daxa::DeviceAddress vb_addr = gpu.device.device_address(mesh.vertex_buffer).value();
        daxa::DeviceAddress mb_addr = gpu.device.device_address(model.material_buffer).value();

        for (auto const & sub : mesh.sub_meshes)
        {
            opaque_draws.emplace_back(DrawCall{
                .vertex_buffer = vb_addr,
                .index_buffer = mesh.index_buffer,
                .material_buffer = mb_addr,
                .transform = std::bit_cast<daxa_f32mat4x4>(transform),
                .index_count = sub.index_count,
                .first_index = sub.index_offset,
                .material_idx = sub.material_idx,
            });
        }
    }
}
