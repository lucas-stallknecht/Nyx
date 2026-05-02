#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "pbr.inl"

DAXA_DECL_PUSH_CONSTANT(DrawPBRPush, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec3 v_norm;
layout(location = 2) out mat3 v_tbn;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    CameraInfo cam = deref(push.cam_buffer);

    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(vert.position, 1.0);
    v_uv = vert.uv;
    // Make a normal matrix if scales become non-uniform
    mat3 model = mat3(push.model_matrix);
    v_norm = normalize(model * vert.normal);
    vec3 tangent = normalize(model * vert.tangent.xyz);
    vec3 bitangent = normalize(model * cross(v_norm, tangent) * vert.tangent.w);
    v_tbn = mat3(tangent, bitangent, v_norm);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_norm;
layout(location = 2) in mat3 v_tbn;
layout(location = 0) out vec4 out_color;

#define LIGHT_DIR vec3(0.0, 1.0, 0.0)

void main()
{
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);
    vec3 color = vec3(0.6);
    vec3 normal = v_norm;

    if (mat.base_color_texture.value != 0) {
        vec4 tex_value = texture(daxa_sampler2D(mat.base_color_texture, push.default_sampler), v_uv);
        if (tex_value.a < 0.5)
            discard;
        color.rgb = tex_value.rgb;
    }
    if (mat.normal_texture.value != 0) {
        vec3 tex_normal = texture(daxa_sampler2D(mat.normal_texture, push.default_sampler), v_uv).rgb;
        tex_normal = tex_normal * 2.0 - 1.0; // [0, 1] to [-1, 1]
        normal = normalize(v_tbn * tex_normal);
    }

    float diff = 0.5 + 0.5 * dot(LIGHT_DIR, normal);
    out_color = vec4(pow(color * diff, vec3(1.0 / 2.2)), 1.0);
}

#endif
