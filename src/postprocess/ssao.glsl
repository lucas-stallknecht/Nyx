#include <daxa/daxa.inl>

#include "ssao.inl"

DAXA_DECL_PUSH_CONSTANT(SSAOPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

vec3 calc_view_position_from_uv(vec2 uv, mat4 inv_proj, daxa_SamplerId depth_sampler)
{
    float depth = texture(
            daxa_sampler2D(push.attachments.depth_image, depth_sampler),
            uv
        ).r;
    vec4 clip = vec4(
            uv * 2.0 - 1.0,
            depth,
            1.0
        );

    vec4 view_pos = inv_proj * clip;
    return view_pos.xyz / view_pos.w;
}

void main()
{
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.ssao_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = global.camera;
    GPUFrameData frame_data = global.frame_data;

    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);

    // Reconstruct view space position and normal from depth-only
    // (using screen-space derivative)
    vec3 view_pos = calc_view_position_from_uv(uv, cam.inv_proj, global.postprocess_linear_sampler);

    vec3 view_normal = 2.0 * texture(
                daxa_sampler2D(push.attachments.slim_gbuffer, global.postprocess_linear_sampler),
                uv
            ).rgb - 1.0;

    vec2 noise_scale = size / float(SSAO_NOISE_DIM);
    vec3 random_vec = texture(
            daxa_sampler2D(push.noise_image, global.nearest_repeat_sampler),
            uv * noise_scale
        ).xyz;
    vec3 tangent = normalize(random_vec - view_normal * dot(random_vec, view_normal));
    vec3 bitangent = cross(view_normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, view_normal);

    float occlusion = 0.0;
    // Hemisphere sampling
    for (int i = 0; i < SSAO_N_SAMPLES; i++)
    {
        vec3 sample_offset = TBN * deref_i(push.kernel_buffer, i);
        vec3 sample_pos = view_pos + sample_offset * frame_data.ssao_radius;

        // Project sampled position
        vec4 clip = cam.proj * vec4(sample_pos, 1.0);
        vec2 sample_uv = clip.xy / clip.w;
        sample_uv = sample_uv * 0.5 + 0.5;

        if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0))))
            continue;

        // Retrieve actual fragment depth at the sample uv
        float geometry_depth = calc_view_position_from_uv(
                sample_uv,
                cam.inv_proj,
                global.postprocess_linear_sampler
            ).z;
        float sample_depth = sample_pos.z;

        float range_check = smoothstep(0.0, 1.0, frame_data.ssao_radius / abs(view_pos.z - geometry_depth));
        occlusion += float(geometry_depth >= sample_depth + frame_data.ssao_bias) * range_check;
    }

    occlusion = 1.0 - occlusion / float(SSAO_N_SAMPLES);
    occlusion = pow(occlusion, 2.0);

    imageStore(
        daxa_image2D(push.attachments.ssao_image),
        tex_coords,
        vec4(occlusion, 0.0, 0.0, 1.0)
    );
}

#endif
