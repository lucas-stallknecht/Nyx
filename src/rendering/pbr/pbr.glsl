#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "pbr.inl"

DAXA_DECL_PUSH_CONSTANT(DrawPBRPush, push)

struct VOut {
    vec3 pos;
    vec2 uv;
    vec3 norm;
    mat3 tbn;
    vec4 light_space_pos;
};

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out VOut v_out;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    CameraInfo cam = deref(push.cam_buffer);
    LightInfo light_info = deref(push.light_buffer);

    vec4 world_pos = push.model_matrix * vec4(vert.position, 1.0);
    gl_Position = cam.proj * cam.view * world_pos;

    v_out.pos = world_pos.xyz;
    v_out.uv = vert.uv;
    // Make a normal matrix if scales become non-uniform
    mat3 model = mat3(push.model_matrix);
    v_out.norm = normalize(model * vert.normal);
    vec3 tangent = normalize(model * vert.tangent.xyz);
    vec3 bitangent = normalize(model * cross(v_out.norm, tangent) * vert.tangent.w);
    v_out.tbn = mat3(tangent, bitangent, v_out.norm);
    v_out.light_space_pos = light_info.dir_matrix * vec4(v_out.pos, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

#define MIN_SHADOW_BIAS 0.001

float calculate_shadow(vec3 normal, vec3 light_dir) {
    vec3 proj_coords = f_in.light_space_pos.xyz / f_in.light_space_pos.w;

    vec2 tex_coord = proj_coords.xy * 0.5 + 0.5; // [-1, 1] -> [0, 1]
    float closest_depth = texture(daxa_sampler2D(push.shadow_map, push.shadow_sampler), tex_coord).r;
    float current_depth = proj_coords.z;

    return current_depth - MIN_SHADOW_BIAS > closest_depth ? 1.0 : 0.0;
}

void main()
{
    LightInfo light_info = deref(push.light_buffer);
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);

    vec3 base_color = mat.base_color;
    vec3 normal = f_in.norm;

    if (mat.base_color_texture.value != 0) {
        vec4 tex_color = texture(daxa_sampler2D(mat.base_color_texture, push.default_sampler), f_in.uv);
        if (tex_color.a < 0.5)
            discard;
        base_color.rgb = tex_color.rgb;
    }
    if (mat.normal_texture.value != 0) {
        vec3 tex_normal = texture(daxa_sampler2D(mat.normal_texture, push.default_sampler), f_in.uv).rgb;
        tex_normal = tex_normal * 2.0 - 1.0; // [0, 1] to [-1, 1]
        normal = normalize(f_in.tbn * tex_normal);
    }

    vec3 light_dir = normalize(light_info.dir_pos);
    float ambient = 0.15;
    float diff = dot(light_dir, normal);
    float shadow = calculate_shadow(normal, light_dir);

    vec3 color = (ambient + (1.0 - shadow) * diff) * base_color;
    out_color = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}

#endif
