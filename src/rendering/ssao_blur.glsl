#include <daxa/daxa.inl>

#include "ssao_blur.inl"

DAXA_DECL_PUSH_CONSTANT(BlurSSAOPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(daxa_image2D(push.attachments.ssao_image));
    if (tex_coords.x >= size.x || tex_coords.y >= size.y)
        return;

    float result = 0.0;
    for (int x = -2; x < 2; x++) {
        for (int y = -2; y < 2; y++) {
            ivec2 offset = ivec2(x, y);
            ivec2 sample_coords = tex_coords + offset;
            sample_coords = clamp(sample_coords, ivec2(0), size - 1);
            result += imageLoad(daxa_image2D(push.attachments.ssao_image), sample_coords).r;
        }
    }

    imageStore(
        daxa_image2D(push.attachments.ssao_blur_image),
        tex_coords,
        vec4(result / 16.0, 0.0, 0.0, 1.0)
    );
}

#endif
