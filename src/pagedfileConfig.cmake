include(CMakeFindDependencyMacro)
find_dependency(Boost COMPONENTS program_options REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/pagedfileTargets.cmake")