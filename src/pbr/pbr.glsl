#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "pbr.inl"

DAXA_DECL_PUSH_CONSTANT(DrawPBRPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec3 v_norm;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    CameraInfo cam = deref(push.cam_buffer);

    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(vert.position, 1.0);
    v_uv = vert.uv;
    v_norm = vert.normal;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_norm;
layout(location = 0) out vec4 out_color;

#define LIGHT_DIR vec3(0.0, 1.0, 0.0)

void main()
{
    vec3 color = vec3(1.0);
    if (push.color_texture.value != 0) {
        color = texture(daxa_sampler2D(push.color_texture, push.default_sampler), v_uv).rgb;
    }

    float diff = 0.5 + 0.5 * dot(LIGHT_DIR, v_norm);
    out_color = vec4(pow(color * diff, vec3(1.0 / 2.2)), 1.0);
}

#endif
