#pragma once

#include "../include/gpu_debug.inl"

#ifdef FORWARD_DEBUG_VIEW

#define X(name, value) const uint DEBUG_##name = value;
DEBUG_VIEW_LIST(X)
#undef X

vec3 get_debug_col(int view, Surface surface, float ao, float shadow) {
    switch (view) {
        case DEBUG_None:
        return vec3(-1.0);

        case DEBUG_FrustumCulling:
        return vec3(-1.0);

        case DEBUG_Normals:
        return max(surface.normal, vec3(0.0));

        case DEBUG_UV:
        return vec3(fract(f_in.uv), 0.0);

        case DEBUG_Albedo:
        return surface.albedo;

        case DEBUG_Roughness:
        return vec3(surface.roughness);

        case DEBUG_Metallic:
        return vec3(surface.metallic);

        case DEBUG_AO:
        return vec3(ao);

        case DEBUG_Shadow:
        return vec3(1.0 - shadow);

        default:
        return vec3(1.0, 0.0, 1.0);
    }
}

#endif
