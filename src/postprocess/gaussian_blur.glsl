#include <daxa/daxa.inl>

#include "gaussian_blur.inl"

DAXA_DECL_PUSH_CONSTANT(GaussianBlurPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_COMPUTE

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    GPUGlobals global = deref(push.global_buffer);

    ivec2 tex_coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 out_size = imageSize(daxa_image2D(push.output_image));
    ivec2 in_size = imageSize(daxa_image2D(push.input_image));
    if (tex_coords.x >= out_size.x || tex_coords.y >= out_size.y)
        return;

    vec2 uv = (vec2(tex_coords) + 0.5) / vec2(out_size);
    vec2 texel = 1.0 / vec2(in_size);

    vec3 result = texture(daxa_sampler2D(push.input_image, global.default_linear_sampler), uv).rgb * weight[0];
    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = push.horizontal ? vec2(texel.x * i, 0.0) : vec2(0.0, texel.y * i);
        result += texture(daxa_sampler2D(push.input_image, global.default_linear_sampler), uv + offset).rgb * weight[i];
        result += texture(daxa_sampler2D(push.input_image, global.default_linear_sampler), uv - offset).rgb * weight[i];
    }

    imageStore(
        daxa_image2D(push.output_image),
        tex_coords,
        vec4(result, 1.0)
    );
}

#endif
