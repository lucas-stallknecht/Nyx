#include <daxa/daxa.inl>

#include "shadow_mapping.inl"

DAXA_DECL_PUSH_CONSTANT(ShadowMappingPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GPUGlobals global = deref(push.global_buffer);
    GPUFrameData frame_data = global.frame_data;
    gl_Position = frame_data.dir_light_matrix * push.model_matrix * vec4(vert.position, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

void main()
{}

#endif
