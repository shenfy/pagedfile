cmake_minimum_required(VERSION 3.8)
project(pfar VERSION 1.3.0 LANGUAGES CXX)

if (EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
  include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Boost COMPONENTS program_options REQUIRED)

# static library
add_library(pagedfile STATIC)
target_sources(pagedfile PRIVATE
  src/BufferStreamBuf.cpp src/PagedFile.cpp src/PathHelper.cpp)
set_target_properties(pagedfile PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(pagedfile PUBLIC cxx_std_17)
set_target_properties(pagedfile PROPERTIES PUBLIC_HEADER
  "include/pagedfile/BufferStreamBuf.h;include/pagedfile/PagedFile.h;include/pagedfile/PathHelper.h")
target_link_libraries(pagedfile PUBLIC Boost::boost lz4)
target_include_directories(pagedfile PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

install(TARGETS pagedfile EXPORT pagedfileTargets
  LIBRARY DESTINATION lib
  PUBLIC_HEADER DESTINATION include/pagedfile)
install(EXPORT pagedfileTargets
  FILE pagedfileTargets.cmake
  NAMESPACE pagedfile::
  DESTINATION lib/cmake/pagedfile)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("pagedfileConfigVersion.cmake"
  VERSION ${pagedfile_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  "pagedfileConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/pagedfileConfigVersion.cmake"
  DESTINATION lib/cmake/pagedfile)


# pfar executable

add_executable(pfar src/main.cpp)
configure_file(src/version.h.in version.h @ONLY)
target_include_directories(pfar PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(pfar PRIVATE pagedfile Boost::program_options stdc++fs)
install(TARGETS pfar RUNTIME DESTINATION bin)

