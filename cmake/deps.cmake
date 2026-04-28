include(FetchContent)

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

if (NOT TARGET fmt::fmt)
  FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG        12.1.0
        EXCLUDE_FROM_ALL
    )
  FetchContent_MakeAvailable(fmt)
endif()
