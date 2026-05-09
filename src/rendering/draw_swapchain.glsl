#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "draw_swapchain.inl"

DAXA_DECL_PUSH_CONSTANT(DrawSwapchainPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
    if (tex_coord.x >= push.size.x || tex_coord.y >= push.size.y) return;

    vec2 uv = vec2(tex_coord.xy) / (gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);

    vec4 in_col = imageLoad(daxa_image2D(push.attachments.draw_image), tex_coord);

    vec4 out_col = vec4(pow(in_col.rgb, vec3(1.0 / 2.2)), in_col.a);
    imageStore(daxa_image2D(push.attachments.swapchain_image), tex_coord, out_col);
}

#endif
