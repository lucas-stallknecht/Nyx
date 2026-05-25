#include <daxa/daxa.inl>

#include "composite.inl"

DAXA_DECL_PUSH_CONSTANT(CompositePC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define TONEMAP_NONE       0
#define TONEMAP_ACES       1
#define TONEMAP_REINHARD   2
#define TONEMAP_UNCHARTED2 3

vec3 tonemap_aces(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

vec3 tonemap_reinhard(vec3 x)
{
    // Extended Reinhard: preserves more highlight detail
    const float max_white = 4.0;
    vec3 num = x * (1.0 + x / (max_white * max_white));
    return num / (1.0 + x);
}

vec3 hable(vec3 x)
{
    const float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemap_uncharted2(vec3 x)
{
    const float W = 11.2;
    vec3 curr = hable(x * 2.0);
    vec3 white_scale = vec3(1.0) / hable(vec3(W));
    return clamp(curr * white_scale, 0.0, 1.0);
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
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.draw_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    GPUGlobals global = deref(push.global_buffer);
    GPUFrameData frame_data = global.frame_data;
    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);

    vec3 hdr_col = imageLoad(daxa_image2D(push.attachments.draw_image), tex_coords).rgb;

    if (frame_data.ssr_enabled) {
        hdr_col += imageLoad(daxa_image2D(push.attachments.ssr_image), tex_coords).rgb;
    }
    if (frame_data.bloom_enabled) {
        vec3 bloom = texture(daxa_sampler2D(push.attachments.bloom_image, global.postprocess_linear_sampler), uv).rgb;
        hdr_col += frame_data.bloom_intensity * bloom;
    }
    if (frame_data.vlight_enabled) {
        vec4 vlight = texture(daxa_sampler2D(push.attachments.volumetric_lighting_image, global.postprocess_linear_sampler), uv);
        hdr_col = hdr_col * vlight.a + vlight.rgb;
    }

    vec3 out_col = hdr_col * frame_data.exposure;

    if (frame_data.tonemapping_mode == TONEMAP_ACES)
        out_col = tonemap_aces(out_col);
    else if (frame_data.tonemapping_mode == TONEMAP_REINHARD)
        out_col = tonemap_reinhard(out_col);
    else if (frame_data.tonemapping_mode == TONEMAP_UNCHARTED2)
        out_col = tonemap_uncharted2(out_col);
    else
        out_col = clamp(out_col, 0.0, 1.0);

    // Gamma correction
    out_col = linear_to_srgb(out_col);

    imageStore(daxa_image2D(push.attachments.output_image), tex_coords, vec4(out_col, 1.0));
}

#endif
