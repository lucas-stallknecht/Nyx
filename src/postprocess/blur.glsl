#include <daxa/daxa.inl>

#include "blur.inl"

DAXA_DECL_PUSH_CONSTANT(BlurPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    GPUGlobals global = deref(push.global_buffer);

    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.input_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(size);
    vec2 texel = 1.0 / vec2(size);

    vec4 result = vec4(0.0);
    for (int x = -2; x < 2; x++) {
        for (int y = -2; y < 2; y++) {
            ivec2 offset = ivec2(x, y);
            result += texture(daxa_sampler2D(push.attachments.input_image, global.default_nearest_sampler), uv + offset * texel);
        }
    }

    imageStore(
        daxa_image2D(push.attachments.blurred_image),
        tex_coords,
        result / 16.0
    );
}

#endif
