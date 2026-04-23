cmake_minimum_required (VERSION 3.20)

set(This TypeaheadSearch)

project(${This})

set(CMAKE_BUILD_PARALLEL_LEVEL)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

if ( PROJECT_IS_TOP_LEVEL )
  set(TYPEAHEAD_SEARCH_BUILD_EXAMPLES TRUE)
  set(TYPEAHEAD_SEARCH_BUILD_TESTS TRUE)
else()
  add_library(${PROJECT_NAME} INTERFACE)
  target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

if ( TYPEAHEAD_SEARCH_BUILD_EXAMPLES )
  add_subdirectory(src) # Compiles the search example executable
endif()

if ( TYPEAHEAD_SEARCH_BUILD_TESTS )
  enable_testing()
  add_subdirectory(test) # Compiles the search test executable
endif()
