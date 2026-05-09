cmake_minimum_required(VERSION 3.21)

project(DushaGPU
  VERSION 1.0
)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL") # TEMP
include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/window.cpp
  src/gpu_context.cpp
  src/camera.cpp
  src/asset_manager.cpp
  src/renderer.cpp
  src/utils/gltf.cpp
  src/utils/ktx.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE src/include
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE <daxa/types.hpp>
    PRIVATE <cstdint>
    PRIVATE <memory>
    PRIVATE <string>
    PRIVATE <string_view>
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE daxa::daxa glfw glm::glm fmt::fmt fastgltf::fastgltf KTX::ktx imgui::imgui
)

add_compile_definitions(ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/")
