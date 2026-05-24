#include <daxa/daxa.inl>

#include "forward.inl"
#include "shadow_mapping.inl"

DAXA_DECL_PUSH_CONSTANT(ForwardPassPC, push)

struct VOut {
    vec3 world_pos;
    vec2 uv;
    vec3 normal;
    mat3 tbn;
    vec4 light_space_pos;
};

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

layout(location = 0) out VOut v_out;

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    vec4 world_pos = push.model_matrix * vec4(vert.position, 1.0);
    gl_Position = cam.proj * cam.view * world_pos;

    v_out.world_pos = world_pos.xyz;
    v_out.uv = vert.uv;
    // Make a normal matrix if scales become non-uniform
    mat3 model = mat3(push.model_matrix);
    v_out.normal = normalize(model * vert.normal);
    vec3 tangent = normalize(model * vert.tangent.xyz);
    vec3 bitangent = normalize(model * cross(v_out.normal, tangent) * vert.tangent.w);
    v_out.tbn = mat3(tangent, bitangent, v_out.normal);
    v_out.light_space_pos = frame_data.dir_light_matrix * vec4(v_out.world_pos, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) in VOut f_in;
layout(location = 0) out vec4 out_color;

#define PI 3.14159

struct Surface {
    vec3 albedo;
    float roughness;
    vec3 normal;
    float metallic;
};

#define FORWARD_DEBUG_VIEW
#include "debug.glsl"
#define FORWARD_BRDF
#include "brdf.glsl"

vec3 calc_directional_lighting(Surface surface, vec3 view_dir, vec3 light_dir, vec3 radiance) {
    vec3 brdf = brdf(surface, view_dir, light_dir);

    return brdf * radiance * max(dot(surface.normal, light_dir), 0.0);
}

float point_light_falloff(float d, float r) {
    float x = clamp(d / r, 0.0, 1.0);
    return 1.0 - (x * x * (3.0 - 2.0 * x));
}

vec3 calc_point_lighting(Surface surface, PointLight light, vec3 view_dir) {
    vec3 light_dir = light.position - f_in.world_pos;
    float distance = length(light_dir);
    light_dir = normalize(light_dir);

    vec3 brdf = brdf(surface, view_dir, light_dir);

    vec3 radiance = light.color * light.intensity;
    float attenuation = (1.0 / max(distance * distance, light.radius * light.radius));
    attenuation *= point_light_falloff(distance, light.radius);

    return brdf * attenuation * radiance * max(dot(surface.normal, light_dir), 0.0);
}

float calc_shadow(daxa_SamplerId shadow_sampler, bool pcf_enabled) {
    // proj_coords in light NDC space
    vec3 proj_coords = f_in.light_space_pos.xyz / f_in.light_space_pos.w;
    float current_depth = proj_coords.z;
    vec2 tex_coords = proj_coords.xy * 0.5 + 0.5;

    if (!pcf_enabled) {
        return texture(daxa_sampler2DShadow(push.attachments.shadow_map, shadow_sampler), vec3(tex_coords, current_depth));
    }

    vec2 texel_size = vec2(1.0) / float(SHADOW_MAP_SIZE);
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texel_size;
            shadow += texture(daxa_sampler2DShadow(push.attachments.shadow_map, shadow_sampler), vec3(tex_coords + offset, current_depth));
        }
    }
    return clamp(shadow / 9.0, 0.0, 1.0);
}

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    GPUMaterial mat = deref_i(push.material_buffer, push.material_idx);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    Surface surface;
    surface.albedo = mat.base_color.rgb;
    surface.normal = f_in.normal;
    surface.roughness = mat.roughness;
    surface.metallic = mat.metallic;
    float alpha = mat.base_color.a;

    if (mat.base_color_texture.value != 0) {
        vec4 tex_color = texture(daxa_sampler2D(mat.base_color_texture, global.material_linear_sampler), f_in.uv);
        surface.albedo.rgb = tex_color.rgb;
        alpha *= tex_color.a;
        if (alpha < mat.alpha_cutoff)
            discard;
    }
    if (mat.normal_texture.value != 0) {
        vec3 tex_normal = texture(daxa_sampler2D(mat.normal_texture, global.material_linear_sampler), f_in.uv).rgb;
        tex_normal = tex_normal * 2.0 - 1.0; // [0, 1] to [-1, 1]
        surface.normal = normalize(f_in.tbn * tex_normal);
    }
    if (mat.metallic_roughness_texture.value != 0) {
        vec3 tex_value = texture(daxa_sampler2D(mat.metallic_roughness_texture, global.material_linear_sampler), f_in.uv).rgb;
        surface.roughness = tex_value.g;
        surface.metallic = tex_value.b;
    }

    vec3 view_dir = normalize(cam.position - f_in.world_pos);

    float ao = 1.0;
    if (frame_data.ssao_enabled) {
        vec2 ss_uv = gl_FragCoord.xy / push.draw_size;
        ao = texture(daxa_sampler2D(push.attachments.ssao_image, global.postprocess_linear_sampler), ss_uv).r;
    }
    vec3 color = frame_data.ambient_light_intensity * frame_data.ambient_light_color * ao * surface.albedo;

    float shadow = calc_shadow(global.shadow_sampler, frame_data.pcf_enabled);
    color += (1.0 - shadow) * calc_directional_lighting(
                surface,
                view_dir,
                frame_data.dir_light_direction,
                frame_data.dir_light_color * frame_data.dir_light_intensity
            );

    for (uint i = 0; i < frame_data.num_point_lights; i++) {
        color += calc_point_lighting(
                surface,
                frame_data.point_lights[i],
                view_dir
            );
    }

    out_color = vec4(color, alpha);

    vec3 debug_col = get_debug_col(frame_data.debug_view, surface, ao, shadow);
    if (any(lessThan(debug_col, vec3(0.0)))) return;
    out_color = vec4(debug_col, alpha);
}

#endif
