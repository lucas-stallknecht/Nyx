#pragma once

#define DEBUG_VIEW_LIST(X)                                                                                             \
    X(None, 0)                                                                                                         \
    X(Normals, 1)                                                                                                      \
    X(UV, 2)                                                                                                           \
    X(Albedo, 3)                                                                                                       \
    X(Roughness, 4)                                                                                                    \
    X(Metallic, 5)                                                                                                     \
    X(AO, 6)                                                                                                           \
    X(Shadow, 7)                                                                                                       \
    X(FrustumCulling, 8)

#if defined(__cplusplus)

enum class DebugView : int32_t
{
#define X(name, value) name = value,
    DEBUG_VIEW_LIST(X)
#undef X
};

static constexpr char const * DEBUG_VIEW_NAMES[] = {
#define X(name, value) #name,
    DEBUG_VIEW_LIST(X)
#undef X
};

#endif
