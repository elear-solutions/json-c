from conans import ConanFile, CMake

class JsonclibConan(ConanFile):
    name = "json-c"
    version = "0.16.99"
    description = "JSON-C - A JSON implementation in C"
    topics = ("conan", "json-c", "json", "encoding", "decoding", "manipulation")
    url = "https://github.com/elear-solutions/json-c"
    license = "MIT"
    generators = "cmake"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False]
    }
    default_options = {
        "shared": False
    }

    def build(self):
        self.configure()
        cmake = CMake(self)
        cmake.configure(source_folder=".")
        cmake.build()
        cmake.install()

    def package(self):
        self.copy("*.h", dst="include", src="package/include")
        self.copy("*", dst="lib", src="package/lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = [ "json-c" ]
