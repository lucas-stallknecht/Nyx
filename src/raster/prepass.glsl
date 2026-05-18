#include <daxa/daxa.inl>

#include "prepass.inl"

DAXA_DECL_PUSH_CONSTANT(PrepassPC, push)

struct VOut {
    vec3 world_pos;
    vec2 uv;
    vec3 world_normal;
    vec3 normal;
};

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out VOut v_out;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = global.camera;
    vec4 world_pos = push.model_matrix * vec4(vert.position, 1.0);
    gl_Position = cam.proj * cam.view * world_pos;
    v_out.world_normal = normalize(mat3(push.model_matrix) * vert.normal);
    v_out.normal = normalize(mat3(cam.view) * v_out.world_normal);
    v_out.uv = vert.uv;
    v_out.world_pos = world_pos.xyz;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    out_color = vec4(0.5 + 0.5 * normalize(f_in.normal), 1.0);

    if (!frame_data.ssr_enabled) return;

    float roughness = mat.roughness;
    float metallic = mat.metallic;

    if (mat.metallic_roughness_texture.value != 0) {
        vec3 tex_value = texture(daxa_sampler2D(mat.metallic_roughness_texture, global.default_linear_sampler), f_in.uv).rgb;
        roughness = tex_value.g;
        metallic = tex_value.b;
    }

    float n_dot_v = max(dot(normalize(cam.position - f_in.world_pos), f_in.world_normal), 0.0);
    float fresnel = pow(1.0 - n_dot_v, 5.0);
    float rough = 1.0 - roughness * roughness;
    float ssr_mask = fresnel * rough;
    ssr_mask = min(ssr_mask, frame_data.ssr_max_mask);
    out_color.a = ssr_mask;
}

#endif
