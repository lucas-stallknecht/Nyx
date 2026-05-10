#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "draw_swapchain.inl"

DAXA_DECL_PUSH_CONSTANT(DrawSwapchainPC, push)

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

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    GPUFrameData frame_data = deref(global.frame_data_buffer);
    ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
    if (tex_coord.x >= push.size.x || tex_coord.y >= push.size.y) return;

    vec2 uv = vec2(tex_coord.xy) / (gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);

    vec3 hdr_col = imageLoad(daxa_image2D(push.attachments.draw_image), tex_coord).rgb;

    // Tone mapping
    vec3 out_col = hdr_col * frame_data.exposure;
    out_col = ACES_film(out_col);
    // Gamma correction
    out_col = pow(out_col.rgb, vec3(1.0 / 2.2));

    imageStore(daxa_image2D(push.attachments.swapchain_image), tex_coord, vec4(out_col, 1.0));
}

#endif
