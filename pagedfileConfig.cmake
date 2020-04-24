include(CMakeFindDependencyMacro)
find_dependency(Boost)
find_dependency(PkgConfig)
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)

include("${CMAKE_CURRENT_LIST_DIR}/pagedfileTargets.cmake")