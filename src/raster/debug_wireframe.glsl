#include <daxa/daxa.inl>

#include "debug_wireframe.inl"

DAXA_DECL_PUSH_CONSTANT(DebugWireframePC, push)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX

const vec3 CUBE_VERTS[8] = vec3[](
        vec3(-0.5, -0.5, -0.5),
        vec3(0.5, -0.5, -0.5),
        vec3(0.5, 0.5, -0.5),
        vec3(-0.5, 0.5, -0.5),
        vec3(-0.5, -0.5, 0.5),
        vec3(0.5, -0.5, 0.5),
        vec3(0.5, 0.5, 0.5),
        vec3(-0.5, 0.5, 0.5)
    );

const ivec2 EDGES[12] = ivec2[](
        ivec2(0, 1),
        ivec2(1, 2),
        ivec2(2, 3),
        ivec2(3, 0),

        ivec2(4, 5),
        ivec2(5, 6),
        ivec2(6, 7),
        ivec2(7, 4),

        ivec2(0, 4),
        ivec2(1, 5),
        ivec2(2, 6),
        ivec2(3, 7)
    );

void main()
{
    GPUGlobals global = deref(push.global_buffer);
    GPUCamera cam = deref(global.camera_buffer);

    uint edge_index = uint(gl_VertexIndex) / 2;
    uint endpoint = uint(gl_VertexIndex) % 2;

    ivec2 edge = EDGES[edge_index];
    uint vert_index = endpoint == 0u ? uint(edge.x) : uint(edge.y);
    vec3 local_pos = CUBE_VERTS[vert_index];

    gl_Position = cam.proj * cam.view * push.model_matrix * vec4(local_pos, 1.0);
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = mix(vec4(0.0, 1.0, 1.0, 1.0), vec4(1.0, 0.0, 0.0, 1.0), float(push.culled));
}

#endif
