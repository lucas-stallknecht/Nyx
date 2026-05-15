#include <daxa/daxa.inl>

#include "prepass.inl"

DAXA_DECL_PUSH_CONSTANT(PrepassPC, push)

struct VOut {
    vec3 normal;
};

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out VOut v_out;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = deref(global.camera_buffer);
    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(vert.position, 1.0);
    v_out.normal = normalize(mat3(cam.view * push.model_matrix) * vert.normal);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(0.5 + 0.5 * normalize(f_in.normal), 1.0);
}

#endif
