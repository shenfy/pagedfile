from conans import ConanFile, CMake, tools


class PagedfileConan(ConanFile):
    name = "pagedfile"
    version = "1.3.1"
    license = "MIT"
    author = "Fangyang Shen dev@shenfy.com"
    url = "https://github.com/shenfy/pagedfile"
    description = "Light weight multiple segment file class for C++"
    topics = ("File")
    settings = "os", "compiler", "build_type", "arch"
    requires = "lz4/1.9.2", "boost/1.73.0"
    options = {"shared": [False]}
    default_options = {"shared": False, "boost:shared": True}
    generators = "cmake_paths", "cmake_find_package"
    exports_sources = "src/*"

    def build(self):
        cmake = CMake(self)
        cmake.configure(source_folder="src")
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.configure(source_folder="src")
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["pagedfile"]

