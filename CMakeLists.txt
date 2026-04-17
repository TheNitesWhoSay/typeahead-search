cmake_minimum_required (VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

cmake_policy(SET CMP0091 NEW)

set(This heuristic-search)

project(${This})

include(FetchContent)
FetchContent_Declare(
  slot-map
  GIT_REPOSITORY https://github.com/TheNitesWhoSay/slot-map.git
  GIT_TAG b8c263c
)
FetchContent_MakeAvailable(slot-map)
include_directories(${slot-map_SOURCE_DIR}/include)

set(CMAKE_BUILD_PARALLEL_LEVEL)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(/permissive- ${ASAN})
add_compile_definitions(_UNICODE UNICODE NOMINMAX)

enable_testing()


set(Headers
  src/heuristic-search.h
)
set(Sources
  src/main.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${This} ${Sources} ${Headers})

find_package(ICU COMPONENTS uc i18n data REQUIRED)

target_link_libraries(${This} ICU::uc ICU::i18n ICU::data)
target_include_directories(${This} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})