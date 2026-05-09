#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "forward.inl"
#include "brdf.glsl"

DAXA_DECL_PUSH_CONSTANT(ForwardPassPC, push)

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
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = deref(global.camera_buffer);
    GPULightInfo light_info = deref(global.light_buffer);

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
    v_out.light_space_pos = light_info.dir_light_matrix * vec4(v_out.pos, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

#define PI 3.14159
#define EPSILON 0.0001

struct Surface {
    vec3 albedo;
    float roughness;
    vec3 normal;
    float metallic;
};

vec3 brdf(Surface surface, vec3 v, vec3 l) {
    vec3 h = normalize(v + l);
    float n_dot_v = max(dot(surface.normal, v), 0.0);
    float n_dot_h = max(dot(surface.normal, h), 0.0);
    float n_dot_l = max(dot(surface.normal, l), 0.0);
    float v_dot_h = max(dot(v, h), 0.0);

    float alpha = surface.roughness * surface.roughness;
    float D = distribution_ggx(n_dot_h, alpha);
    float r = alpha + 1.0;
    float k = (r * r) / 8.0;
    float G = geometry_smith(surface.normal, l, v, k);
    vec3 f0 = mix(vec3(0.04), surface.albedo, surface.metallic);
    vec3 F = fresnel_schlick(v_dot_h, f0);

    vec3 num = D * F * G;
    float denom = 4.0 * n_dot_v * n_dot_l + EPSILON;
    vec3 specular = num / denom;

    vec3 diffuse = surface.albedo / PI;

    vec3 k_specular = F;
    vec3 k_diffuse = (1.0 - k_specular) * (1.0 - surface.metallic);

    return k_diffuse * diffuse + specular;
}

vec3 calc_directional_lighting(Surface surface, vec3 view_dir, vec3 light_dir, vec3 radiance) {
    vec3 brdf = brdf(surface, view_dir, light_dir);

    return brdf * radiance * max(dot(surface.normal, light_dir), 0.0);
}

vec3 calc_point_lighting(Surface surface, PointLight light, vec3 view_dir) {
    vec3 light_dir = light.position - f_in.pos;
    float distance = length(light_dir);
    light_dir = normalize(light_dir);

    vec3 brdf = brdf(surface, view_dir, light_dir);

    vec3 radiance = light.color * 1.0;
    float attenuation = 1.0 / (1.0 + light.linear * distance + (light.quadratic * distance * distance));

    return brdf * attenuation * radiance * max(dot(surface.normal, light_dir), 0.0);
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
    GPUGlobals global = deref(push.global_buffer);
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);
    GPUCamera cam = deref(global.camera_buffer);
    GPULightInfo light_info = deref(global.light_buffer);

    Surface surface;
    surface.albedo = mat.base_color;
    surface.normal = f_in.norm;
    surface.roughness = mat.roughness;
    surface.metallic = mat.metallic;

    if (mat.base_color_texture.value != 0) {
        vec4 tex_color = texture(daxa_sampler2D(mat.base_color_texture, global.default_linear_sampler), f_in.uv);
        if (tex_color.a < 0.5)
            discard;
        surface.albedo.rgb = tex_color.rgb;
    }
    if (mat.normal_texture.value != 0) {
        vec3 tex_normal = texture(daxa_sampler2D(mat.normal_texture, global.default_linear_sampler), f_in.uv).rgb;
        tex_normal = tex_normal * 2.0 - 1.0; // [0, 1] to [-1, 1]
        surface.normal = normalize(f_in.tbn * tex_normal);
    }
    if (mat.metallic_roughness_texture.value != 0) {
        vec3 tex_value = texture(daxa_sampler2D(mat.metallic_roughness_texture, global.default_linear_sampler), f_in.uv).rgb;
        surface.roughness = tex_value.g;
        surface.metallic = tex_value.b;
    }

    vec3 view_dir = normalize(cam.position - f_in.pos);

    float ambient = 0.06;
    vec3 color = ambient * surface.albedo;

    float shadow = calc_shadow(global.shadow_sampler);
    color += (1.0 - shadow) * calc_directional_lighting(
                surface,
                view_dir,
                light_info.dir_light_direction,
                light_info.dir_light_color * light_info.dir_light_intensity
            );

    for (uint i = 0; i < light_info.num_point_lights; i++) {
        color += calc_point_lighting(
                surface,
                light_info.point_lights[i],
                view_dir
            );
    }

    out_color = vec4(color, 1.0);
}

#endif
