#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <daxa/gpu_resources.hpp>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;
using i8 = std::int8_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = std::float_t;
using f64 = std::double_t;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using quat = glm::quat;
using mat4 = glm::mat4;

struct Handle
{
    usize idx = 0;
    usize gen = 0;

    bool valid() const { return idx != 0 && gen != 0; };
    bool operator==(Handle const & other) const = default;
};

struct Texture
{
    Handle handle;
    daxa::ImageId image;
};
