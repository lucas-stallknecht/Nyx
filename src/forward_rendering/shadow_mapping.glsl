#include <daxa/daxa.inl>

#extension GL_EXT_debug_printf : enable

#include "shadow_mapping.inl"

DAXA_DECL_PUSH_CONSTANT(DrawShadowDepthPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GlobalRenderingBuffer global = deref(push.global_buffer);
    LightInfo light_info = deref(global.light_buffer);
    gl_Position = light_info.sun_matrix * push.model_matrix * vec4(vert.position, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

void main()
{}

#endif
