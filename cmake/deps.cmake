include(FetchContent)

if (NOT TARGET glfw)
  option(GLFW_BUILD_TESTS "" OFF)
  option(GLFW_BUILD_DOCS "" OFF)
  option(GLFW_INSTALL "" OFF)
  option(GLFW_BUILD_EXAMPLES "" OFF)
  FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        3.4
        EXCLUDE_FROM_ALL
    )
  FetchContent_MakeAvailable(glfw)
endif()

FetchContent_Declare(
    daxa
    GIT_REPOSITORY https://github.com/Ipotrick/Daxa
    GIT_TAG        master
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(daxa)

if (NOT TARGET glm::glm)
  FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG        1.0.3
        EXCLUDE_FROM_ALL
    )
  FetchContent_MakeAvailable(glm)
endif()

if (NOT TARGET fastgltf::fastgltf)
  FetchContent_Declare(
        fastgltf
        GIT_REPOSITORY https://github.com/spnda/fastgltf
        GIT_TAG        v0.9.0
        EXCLUDE_FROM_ALL
    )
  FetchContent_MakeAvailable(fastgltf)
endif()

if (NOT TARGET KTX::ktx)
  option(KTX_FEATURE_TESTS "" OFF)
  FetchContent_Declare(
        ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
        GIT_TAG        v4.4.2
        EXCLUDE_FROM_ALL
    )
  FetchContent_MakeAvailable(ktx)
  add_library(KTX::ktx ALIAS ktx)
endif()
