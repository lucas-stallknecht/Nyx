#include <daxa/daxa.inl>

#include "depth_prepass.inl"

DAXA_DECL_PUSH_CONSTANT(DepthPrepassPC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

void main()
{
    Vertex vert = deref_i(push.vertex_buffer, gl_VertexIndex);
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = deref(global.camera_buffer);
    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(vert.position, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

void main()
{}

#endif
