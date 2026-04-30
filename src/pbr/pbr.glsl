#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "pbr.inl"

DAXA_DECL_PUSH_CONSTANT(DrawPBRPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out vec3 v_norm;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    CameraInfo cam = deref(push.cam_buffer);

    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(vert.position, 1.0);
    v_norm = vert.normal;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec3 v_norm;
layout(location = 0) out vec4 out_color;

#define LIGHT_DIR vec3(0.0, 1.0, 0.0)

void main()
{
    float d = 0.5 + 0.5 * dot(LIGHT_DIR, v_norm);
    out_color = vec4(vec3(d), 1.0);
}

#endif
