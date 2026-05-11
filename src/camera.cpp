#include "camera.hpp"
#include <fmt/core.h>

mat4 Camera::get_proj(f32 aspect_ratio) const
{
    mat4 proj = glm::perspective(glm::radians(fov), aspect_ratio, near, far);
    proj[1][1] *= -1.0f; // Flip for Vulkan clip space
    return proj;
}

mat4 Camera::get_view() const
{
    glm::mat4 rot = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), -position);
    return rot * trans;
}

void Camera::move_forward(f32 d) { position += get_forward() * d * move_speed; }

void Camera::move_up(f32 d) { position += get_up() * d * move_speed; }

void Camera::move_right(f32 d) { position += get_right() * d * move_speed; }

void Camera::rotate(vec2 delta)
{
    f32 yaw = -delta.x * look_sensitivity;
    f32 pitch = -delta.y * look_sensitivity;

    // World up
    glm::quat yaw_quat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = yaw_quat * rotation;
    // Local right
    glm::quat pitch_quat = glm::angleAxis(pitch, get_right());
    rotation = pitch_quat * rotation;
    rotation = glm::normalize(rotation);
}
