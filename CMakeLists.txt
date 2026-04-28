cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(DushaGPU
  VERSION 1.0
)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/window.cpp
  src/gpu_context.cpp
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE <daxa/types.hpp>
    PRIVATE <cstdint>
    PRIVATE <memory>
    PRIVATE <string>
    PRIVATE <string_view>
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE daxa::daxa glfw glm::glm fmt::fmt
)
