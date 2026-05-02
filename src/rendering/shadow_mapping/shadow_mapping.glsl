#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "shadow_mapping.inl"

DAXA_DECL_PUSH_CONSTANT(DrawDirectionalDepthMap, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    LightInfo light_info = deref(push.light_buffer);
    gl_Position = light_info.dir_matrix * push.model_matrix * vec4(vert.position, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

void main()
{}

#endif
