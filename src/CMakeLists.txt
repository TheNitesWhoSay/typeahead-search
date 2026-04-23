# Compile the search example executable
cmake_minimum_required (VERSION 3.20)

set(This SearchExample)

project(${This})

set(CMAKE_BUILD_PARALLEL_LEVEL)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(/permissive- ${ASAN})

include(FetchContent)
FetchContent_Declare(
  slot-map
  GIT_REPOSITORY https://github.com/TheNitesWhoSay/slot-map.git
  GIT_TAG 1c2aa29
)
FetchContent_MakeAvailable(slot-map)
include_directories(${slot-map_SOURCE_DIR}/include)

set(ExamplePath ${CMAKE_CURRENT_SOURCE_DIR})
cmake_path(GET ExamplePath PARENT_PATH TypeaheadSearchRootPath)
include_directories(${TypeaheadSearchRootPath}/include)

set(Headers
  example-data.h
)
set(Sources
  main.cpp
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${This} ${Sources} ${Headers})

find_package(ICU COMPONENTS uc i18n data REQUIRED)

target_link_libraries(${This} ICU::uc ICU::i18n ICU::data)
