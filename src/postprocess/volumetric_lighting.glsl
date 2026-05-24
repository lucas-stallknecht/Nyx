#include <daxa/daxa.inl>

#include "volumetric_lighting.inl"

DAXA_DECL_PUSH_CONSTANT(VolumetricLightingPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.output_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);
    vec2 ndc = uv * 2.0 - 1.0;

    float depth = texture(daxa_sampler2D(push.attachments.depth_image, global.nearest_clamp_sampler), uv).r;

    vec4 clip = vec4(ndc, depth, 1.0);
    vec4 view = cam.inv_proj * clip;
    view /= view.w;

    vec3 world_pos = (cam.inv_view * vec4(view.xyz, 1.0)).xyz;

    vec3 ray_origin = cam.position.xyz;
    vec3 ray_dir = normalize(world_pos - ray_origin);

    float world_depth = length(world_pos - ray_origin);

    float jitter = fract(sin(dot(vec2(tex_coords), vec2(12.9898, 78.233))) * 43758.5453);
    // start inside first step to reduce banding
    float t = jitter * frame_data.vlight_step_size;

    int max_samples = frame_data.vlight_num_samples;

    vec3 out_light = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < max_samples; i++)
    {
        if (t >= world_depth || transmittance < 0.01)
            break;

        float step_len = min(frame_data.vlight_step_size, world_depth - t);
        float sample_t = t + 0.5 * step_len;

        vec3 ray_pos = ray_origin + sample_t * ray_dir;

        vec4 light_space_pos = frame_data.dir_light_matrix * vec4(ray_pos, 1.0);
        light_space_pos.xyz /= light_space_pos.w;

        float current_depth = light_space_pos.z;
        vec2 light_space_uv = light_space_pos.xy * 0.5 + 0.5;

        float visible = 1.0 - texture(
                    daxa_sampler2DShadow(push.attachments.shadow_map, global.shadow_sampler),
                    vec3(light_space_uv, current_depth - 0.005)
                );
        float extinction = frame_data.vlight_density * step_len * visible;

        out_light += transmittance * extinction * frame_data.dir_light_color * frame_data.dir_light_intensity;
        transmittance *= exp(-extinction);

        t += frame_data.vlight_step_size;
    }

    imageStore(
        daxa_image2D(push.attachments.output_image),
        tex_coords,
        vec4(out_light, transmittance)
    );
}

#endif
