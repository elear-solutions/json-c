import fnmatch
from conans import ConanFile, CMake, tools

class JsonclibConan(ConanFile):
    name = "json-c"
    version = "0.13"
    description = "JSON-C - A JSON implementation in C"
    topics = ("conan", "json-c", "json", "encoding", "decoding", "manipulation")
    url = "https://github.com/elear-solutions/json-c"
    license = "MIT"
    generators = "cmake"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False]
    }
    default_options = {key: False for key in options.keys()}
    default_options ["shared"] = False
    default_options ["fPIC"] = True

    @property
    def _targets(self):
        return {
            "iOS-armv7-*": "arm-apple-darwin",
            "iOS-armv8-*": "aarch64-apple-darwin",
            "iOS-x86-*": "i386-apple-darwin",
            "iOS-x86_64-*": "x86_64-apple-darwin"
        }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        del self.settings.compiler.libcxx
        del self.settings.compiler.cppstd

    def _configure_cmake(self):
        if tools.cross_building(self.settings) and self.settings.os != "Windows":
            query = "%s-%s-%s" % (self.settings.os, self.settings.arch, self.settings.compiler)
            ancestor = next((self._targets[i] for i in self._targets if fnmatch.fnmatch(query, i)), None)
            host = tools.get_gnu_triplet(str(self.settings.os), str(self.settings.arch))
            tools.replace_in_file("../CMakeLists.txt",
                                  "execute_process(COMMAND ./configure ",
                                  "execute_process(COMMAND ./configure --host %s " % ancestor)
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()
        cmake.install()

    def package(self):
        self.copy("*.h", dst="include/json-c", src="package/include/json-c")
        self.copy("*", dst="lib", src="package/lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = [ "json-c" ]
