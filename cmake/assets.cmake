include(FetchContent)

FetchContent_Declare(
  assets
  GIT_REPOSITORY https://github.com/lucas-stallknecht/dushha-assets.git
  GIT_TAG master
)

FetchContent_MakeAvailable(assets)
FetchContent_GetProperties(assets)

message(STATUS "Assets path: ${assets_SOURCE_DIR}")
add_compile_definitions(ASSETS_DIR="${assets_SOURCE_DIR}/")
