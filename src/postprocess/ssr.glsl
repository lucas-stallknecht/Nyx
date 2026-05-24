#include <daxa/daxa.inl>
#include "ssr.inl"

DAXA_DECL_PUSH_CONSTANT(SSRPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

float edge_fade(vec2 uv, float e)
{
    vec2 d = abs(uv * 2.0 - 1.0);
    // 1 at center
    // 0 near borders
    float fade = 1.0 - max(d.x, d.y);
    return smoothstep(0.0, e, fade);
}

void main()
{
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.depth_image));

    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);
    vec3 original_col = texture(
            daxa_sampler2D(push.attachments.input_image, global.nearest_clamp_sampler),
            uv
        ).rgb;
    vec4 gbuffer_value = texture(
            daxa_sampler2D(push.attachments.slim_gbuffer, global.nearest_clamp_sampler),
            uv
        );
    if (gbuffer_value.a < frame_data.ssr_min_mask)
    {
        imageStore(
            daxa_image2D(push.attachments.ssr_image),
            tex_coords,
            vec4(0.0)
        );
        return;
    }

    vec3 view_normal = normalize(2.0 * gbuffer_value.rgb - 1.0);

    float texel_depth = texture(
            daxa_sampler2D(push.attachments.depth_image, global.nearest_clamp_sampler),
            uv
        ).r;

    // Flow:
    // - Get clip->view origin from depth
    // - Compute ray direction and ray end in view space thanks to normal
    // - Move everything to texture/screen space
    vec4 clip_origin = vec4(uv * 2.0 - 1.0 /* ndc */ , texel_depth, 1.0);
    vec4 view_origin = cam.inv_proj * clip_origin;
    view_origin.xyz /= view_origin.w;

    vec3 view_incident = normalize(view_origin.xyz);
    vec3 view_reflect_dir = reflect(view_incident, view_normal);

    vec3 view_end = view_origin.xyz + view_reflect_dir * frame_data.ssr_max_distance;

    vec4 clip_end = cam.proj * vec4(view_end, 1.0);
    clip_end.xyz /= clip_end.w;

    vec3 ts_origin = vec3(clip_origin.xy * 0.5 + 0.5, clip_origin.z);
    vec3 ts_end = vec3(clip_end.xy * 0.5 + 0.5, clip_end.z);

    ivec2 origin_px = ivec2(ts_origin.xy * size); // Should be the same as tex_coords
    ivec2 end_px = ivec2(ts_end.xy * size);
    ivec2 delta_px = end_px - origin_px;

    // Scale the ray marching step size
    int max_dist = max(abs(delta_px.x), abs(delta_px.y));
    max_dist = clamp(max_dist, 1, frame_data.ssr_num_samples);

    // DO NOT normalize as texture space is not linear
    vec3 ts_dir = (ts_end - ts_origin) / float(max_dist);

    vec3 pos = ts_origin + ts_dir;
    bool hit = false;
    for (int i = 0; i < max_dist; i++)
    {
        // Out of screen space
        if (pos.x < 0.0 || pos.y < 0.0 || pos.x >= 1.0 || pos.y >= 1.0)
            break;

        float scene_depth = texture(
                daxa_sampler2D(push.attachments.depth_image, global.nearest_clamp_sampler),
                pos.xy
            ).r;

        if (pos.z < scene_depth) {
            hit = true;
            break;
        }

        pos += ts_dir;
    }

    vec3 reflection = vec3(0.0);
    if (hit)
    {
        reflection = texture(
                daxa_sampler2D(push.attachments.input_image, global.nearest_clamp_sampler),
                pos.xy
            ).rgb;
        reflection *= frame_data.ssr_reflection_intensity;
        reflection *= edge_fade(pos.xy, frame_data.ssr_screen_edge_fade);
    }

    imageStore(
        daxa_image2D(push.attachments.ssr_image),
        tex_coords,
        vec4(mix(vec3(0.0), reflection, gbuffer_value.a), 1.0)
    );
}

#endif
