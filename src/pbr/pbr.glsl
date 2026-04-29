#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "pbr.inl"

DAXA_DECL_PUSH_CONSTANT(DrawPBRPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out vec3 v_col;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    CameraInfo cam = deref(push.cam_buffer);

    gl_Position = cam.proj * cam.view * vec4(vert.position, 1.0);
    v_col = vert.color;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec3 v_col;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(v_col, 1.0);
}

#endif
