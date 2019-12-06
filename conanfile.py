from conans import ConanFile, CMake, tools
import os

class JsonclibConan(ConanFile):
    name = "json-c"
    description = "JSON-C - A JSON implementation in C"
    topics = ("conan", "json-c", "json", "encoding", "decoding", "manipulation")
    url = "<Package recipe repository url here, for issues about the package>"
    homepage = "https://github.com/elear-solutions/json-c"
    license = "<Put the package license here>"
    generators = "cmake"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {
        'shared': False,
        'fPIC': True
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        del self.settings.compiler.libcxx
        del self.settings.compiler.cppstd

    def _configure_cmake(self):
        if tools.cross_building(self.settings) and self.settings.os != "Windows":
            host = tools.get_gnu_triplet(str(self.settings.os), str(self.settings.arch))
            tools.replace_in_file("./CMakeLists.txt",
                                  "execute_process(COMMAND ./configure ",
                                  "execute_process(COMMAND ./configure --host %s " % host)
        cmake = CMake(self)
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()
        cmake.install()

    def package(self):
        self.copy("*.h", dst="include", src="include")
        self.copy("*", dst="lib", src="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = [ "json-c" ]
