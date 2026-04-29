#include "camera.hpp"

mat4 Camera::get_proj(f32 aspect_ratio)
{
    mat4 proj = glm::perspective(glm::radians(fov), aspect_ratio, NEAR, FAR);
    proj[1][1] *= -1.0f; // Flip for Vulkan clip space
    return proj;
}

mat4 Camera::get_view()
{
    glm::mat4 rot = glm::mat4_cast(glm::conjugate(rotation));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), -position);
    return rot * trans;
}

void Camera::move(f32 dfor, f32 dup, f32 dright)
{
    position += get_forward() * dfor * MOVE_SPEED;
    position += get_up() * dup * MOVE_SPEED;
    position += get_right() * dright * MOVE_SPEED;
}

void Camera::rotate(vec2 delta)
{
    float yaw = -delta.x * LOOK_SENSITIVITY;
    float pitch = -delta.y * LOOK_SENSITIVITY;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    // World up
    glm::quat yaw_quat = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    rotation = yaw_quat * rotation;
    // Local right
    glm::quat pitch_quat = glm::angleAxis(pitch, get_right());
    rotation = pitch_quat * rotation;
    rotation = glm::normalize(rotation);
}
