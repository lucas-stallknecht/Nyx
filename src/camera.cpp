#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include "camera.hpp"
#include <fmt/core.h>

void Camera::update_proj(f32 aspect_ratio)
{
    proj = glm::perspective(glm::radians(fov), aspect_ratio, far, near);
    proj[1][1] *= -1.0f; // Flip for Vulkan clip space
}

mat4 Camera::get_view() const
{
    mat4 rot = mat4_cast(glm::conjugate(rotation));
    mat4 trans = glm::translate(mat4(1.0f), -position);
    return rot * trans;
}

void Camera::move_forward(f32 d) { position += get_forward() * d * move_speed; }

void Camera::move_up(f32 d) { position += get_up() * d * move_speed; }

void Camera::move_right(f32 d) { position += get_right() * d * move_speed; }

void Camera::rotate(vec2 delta)
{
    f32 yaw = -delta.x * (look_sensitivity * 0.01f);
    f32 pitch = -delta.y * (look_sensitivity * 0.01f);

    // World up
    quat yaw_quat = glm::angleAxis(yaw, vec3(0.0f, 1.0f, 0.0f));
    rotation = yaw_quat * rotation;
    // Local right
    quat pitch_quat = glm::angleAxis(pitch, get_right());
    rotation = pitch_quat * rotation;
    rotation = glm::normalize(rotation);
}
