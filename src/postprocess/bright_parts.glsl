#include <daxa/daxa.inl>

#include "bright_parts.inl"

DAXA_DECL_PUSH_CONSTANT(BrightPartsPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main()
{
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.input_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    GPUGlobals global = deref(push.global_buffer);
    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);
    vec3 color = texture(daxa_sampler2D(push.attachments.input_image, global.nearest_clamp_sampler), uv).rgb;
    vec3 out_bright_color = vec3(0.0);

    if (luma(color) >= 1.0) {
        out_bright_color = color;
    }

    imageStore(
        daxa_image2D(push.attachments.bloom_image),
        tex_coords,
        vec4(out_bright_color, 1.0)
    );
}

#endif
