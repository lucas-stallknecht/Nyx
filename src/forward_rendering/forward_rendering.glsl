#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "forward_rendering.inl"

DAXA_DECL_PUSH_CONSTANT(DrawForwardPC, push)

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
    GlobalRenderingBuffer global = deref(push.global_buffer);
    CameraInfo cam = deref(global.camera_buffer);
    LightInfo light_info = deref(global.light_buffer);

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
    v_out.light_space_pos = light_info.sun_matrix * vec4(v_out.pos, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

vec3 calc_sun_contribution(vec3 sun_dir, vec3 sun_color, vec3 base_color, vec3 normal) {
    vec3 diffuse = sun_color * max(dot(sun_dir, normal), 0.0) * base_color;

    return diffuse;
}

vec3 calc_point_light_contribution(PointLight light, vec3 base_color, vec3 normal) {
    vec3 light_dir = light.position - f_in.pos;
    float distance = length(light_dir);
    light_dir = normalize(light_dir);

    float attenuation = 1.0 / (1.0 + light.linear * distance + (light.quadratic * distance * distance));

    vec3 diffuse = light.color * max(dot(normal, light_dir), 0.0) * base_color;

    return diffuse * attenuation;
}

float calc_shadow(daxa_SamplerId shadow_sampler) {
    vec3 proj_coords = f_in.light_space_pos.xyz / f_in.light_space_pos.w;

    vec2 tex_coord = proj_coords.xy * 0.5 + 0.5; // [-1, 1] -> [0, 1]
    float closest_depth = texture(daxa_sampler2D(push.shadow_depth_image, shadow_sampler), tex_coord).r;
    float current_depth = proj_coords.z;

    return current_depth > closest_depth ? 1.0 : 0.0;
}

void main()
{
    GlobalRenderingBuffer global = deref(push.global_buffer);
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);
    CameraInfo cam = deref(global.camera_buffer);
    LightInfo light_info = deref(global.light_buffer);

    vec3 base_color = mat.base_color;
    vec3 normal = f_in.norm;

    if (mat.base_color_texture.value != 0) {
        vec4 tex_color = texture(daxa_sampler2D(mat.base_color_texture, global.default_linear_sampler), f_in.uv);
        if (tex_color.a < 0.5)
            discard;
        base_color.rgb = tex_color.rgb;
    }
    if (mat.normal_texture.value != 0) {
        vec3 tex_normal = texture(daxa_sampler2D(mat.normal_texture, global.default_linear_sampler), f_in.uv).rgb;
        tex_normal = tex_normal * 2.0 - 1.0; // [0, 1] to [-1, 1]
        normal = normalize(f_in.tbn * tex_normal);
    }

    float ambient = 0.02;
    vec3 color = ambient * base_color;

    float shadow = calc_shadow(global.shadow_sampler);
    color += (1.0 - shadow) * calc_sun_contribution(
                light_info.sun_dir,
                light_info.sun_color,
                base_color,
                normal
            );

    for (uint i = 0; i < light_info.num_point_lights; i++) {
        color += calc_point_light_contribution(
                light_info.point_lights[i],
                base_color,
                normal
            );
    }

    out_color = vec4(color, 1.0);
}

#endif
