#include <daxa/daxa.inl>

#include "composite.inl"

DAXA_DECL_PUSH_CONSTANT(CompositePC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

vec3 ACES_film(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

vec3 linear_to_srgb(vec3 x)
{
    vec3 lo = x * 12.92;
    vec3 hi = 1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055;

    bvec3 cutoff = lessThanEqual(x, vec3(0.0031308));
    return mix(hi, lo, cutoff);
}

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    GPUFrameData frame_data = deref(global.frame_data_buffer);
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.draw_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y) return;

    vec3 hdr_col = imageLoad(daxa_image2D(push.attachments.draw_image), tex_coords).rgb;

    if (frame_data.ssr_enabled) {
        hdr_col += imageLoad(daxa_image2D(push.attachments.ssr_image), tex_coords).rgb;
    }
    // Tone mapping
    vec3 out_col = hdr_col * frame_data.exposure;
    out_col = ACES_film(out_col);
    // Gamma correction
    out_col = linear_to_srgb(out_col);

    imageStore(daxa_image2D(push.attachments.output_image), tex_coords, vec4(out_col, 1.0));
}

#endif
