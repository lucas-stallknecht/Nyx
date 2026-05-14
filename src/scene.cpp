#include "scene.hpp"

#include "gpu_context.hpp"
#include <bit>
#include <glm/glm.hpp>
#include <fmt/core.h>

namespace
{
    struct Plane
    {
        glm::vec3 normal = {0.f, 1.f, 0.f};
        f32 distance = 0.f;
    };
    struct Frustum
    {
        Plane left, right, bottom, top, near, far;
    };

    void normalize_plane(Plane & p)
    {
        f32 len = glm::length(p.normal);
        p.normal /= len;
        p.distance /= len;
    };

    // https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf
    Frustum extract_frustum_impl_fast(mat4 const & proj_view)
    {
        Frustum f;

        f.left.normal.x = proj_view[0][3] + proj_view[0][0];
        f.left.normal.y = proj_view[1][3] + proj_view[1][0];
        f.left.normal.z = proj_view[2][3] + proj_view[2][0];
        f.left.distance = proj_view[3][3] + proj_view[3][0];
        f.right.normal.x = proj_view[0][3] - proj_view[0][0];
        f.right.normal.y = proj_view[1][3] - proj_view[1][0];
        f.right.normal.z = proj_view[2][3] - proj_view[2][0];
        f.right.distance = proj_view[3][3] - proj_view[3][0];
        f.bottom.normal.x = proj_view[0][3] + proj_view[0][1];
        f.bottom.normal.y = proj_view[1][3] + proj_view[1][1];
        f.bottom.normal.z = proj_view[2][3] + proj_view[2][1];
        f.bottom.distance = proj_view[3][3] + proj_view[3][1];
        f.top.normal.x = proj_view[0][3] - proj_view[0][1];
        f.top.normal.y = proj_view[1][3] - proj_view[1][1];
        f.top.normal.z = proj_view[2][3] - proj_view[2][1];
        f.top.distance = proj_view[3][3] - proj_view[3][1];
        // Vulkan: 0..w clip space
        f.near.normal.x = proj_view[0][3] + proj_view[0][2];
        f.near.normal.y = proj_view[1][3] + proj_view[1][2];
        f.near.normal.z = proj_view[2][3] + proj_view[2][2];
        f.near.distance = proj_view[3][3] + proj_view[3][2];
        f.far.normal.x = proj_view[0][3] - proj_view[0][2];
        f.far.normal.y = proj_view[1][3] - proj_view[1][2];
        f.far.normal.z = proj_view[2][3] - proj_view[2][2];
        f.far.distance = proj_view[3][3] - proj_view[3][2];

        normalize_plane(f.left);
        normalize_plane(f.right);
        normalize_plane(f.bottom);
        normalize_plane(f.top);
        normalize_plane(f.near);
        normalize_plane(f.far);

        return f;
    }

    bool is_visible_impl(Frustum const & f, vec3 const & min, vec3 const & max)
    {
        vec3 corners[8] = {
            {min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {max.x, max.y, min.z},
            {min.x, min.y, max.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z},
        };

        auto test_plane = [&](Plane const & p)
        {
            for (auto & c : corners)
            {
                if (glm::dot(p.normal, c) + p.distance >= 0)
                {
                    return true;
                }
            }
            return false;
        };

        return test_plane(f.left) && test_plane(f.right) && test_plane(f.bottom) && test_plane(f.top) &&
               test_plane(f.near) && test_plane(f.far);
    }

} // namespace

void Scene::clear()
{
    opaque_draws.clear();
    transparent_draws.clear();
}

void Scene::update(Camera const & camera)
{
    glm::mat4 proj_view = camera.get_proj() * camera.get_view();
    Frustum frusutm = extract_frustum_impl_fast(proj_view);
    for (auto & draw : opaque_draws)
    {
        draw.culled = !is_visible_impl(frusutm, draw.aabb_min, draw.aabb_max);
    }

    for (auto & draw : transparent_draws)
    {
        draw.culled = !is_visible_impl(frusutm, draw.aabb_min, draw.aabb_max);
        if (!draw.culled)
        {
            draw.distance_to_camera = glm::length(vec3(std::bit_cast<mat4>(draw.transform)[3]) - camera.position);
        }
    }

    std::sort(transparent_draws.begin(), transparent_draws.end(),
              [](TransparentDrawCall const & a, TransparentDrawCall const & b)
              { return a.distance_to_camera > b.distance_to_camera; });
}

int Scene::add_model(Model const & model)
{
    int triangle_count = 0;
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
            // Build AABB min and max from local
            glm::vec3 world_center = glm::vec3(transform * glm::vec4(sub.bounds_origin, 1.0f));
            glm::vec3 world_extents = glm::abs(transform[0]) * sub.bounds_extents.x +
                                      glm::abs(transform[1]) * sub.bounds_extents.y +
                                      glm::abs(transform[2]) * sub.bounds_extents.z;
            DrawCall base_draw = {
                .index_buffer = mesh.index_buffer,
                .material_buffer = mb_addr,
                .vertex_buffer = vb_addr,
                .transform = std::bit_cast<daxa_f32mat4x4>(transform),
                .index_count = sub.index_count,
                .first_index = sub.index_offset,
                .material_idx = sub.material_idx,
                .aabb_min = world_center - world_extents,
                .aabb_max = world_center + world_extents,
            };
            triangle_count += static_cast<int>((sub.index_count / 3));

            bool transparent =
                sub.material_idx < model.material_transparent.size() && model.material_transparent[sub.material_idx];

            if (transparent)
            {
                auto transparent_draw_call = static_cast<TransparentDrawCall>(base_draw);
                transparent_draws.push_back(transparent_draw_call);

                continue;
            }
            opaque_draws.push_back(base_draw);
        }
    }

    return triangle_count;
}
