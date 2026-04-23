# Compile the test executable
cmake_minimum_required (VERSION 3.20)

set(This SearchTest)

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

set(TestPath ${CMAKE_CURRENT_SOURCE_DIR})
cmake_path(GET TestPath PARENT_PATH TypeaheadSearchRootPath)
include_directories(${TypeaheadSearchRootPath}/include)

set(Headers
  
)
set(Sources
  search_test.cpp
  test_main.cpp
)

add_executable(${This} ${Sources} ${Headers})

find_package(ICU COMPONENTS uc i18n data REQUIRED)

enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

target_link_libraries(${This} PRIVATE ICU::uc ICU::i18n ICU::data GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

if ( WIN64_DEBUG_ASAN ) # The asan dll must be made available for gtest_discover_tests to use the test exe
    configure_file("$ENV{VCToolsInstallDir}/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll"
        "${CMAKE_CURRENT_BINARY_DIR}/clang_rt.asan_dynamic-x86_64.dll" COPYONLY)
endif ()

gtest_discover_tests(${This} DISCOVERY_MODE PRE_TEST)
