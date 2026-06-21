import os
from conan import ConanFile
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd


class QbuemJsonConan(ConanFile):
    name = "qbuem-json"
    version = "1.11.1"
    license = "Apache-2.0"
    url = "https://github.com/qbuem/qbuem-json"
    homepage = "https://qbuem.com/qbuem-json/"
    description = (
        "Single-header C++20 JSON library: dual-engine SIMD DOM + zero-tape "
        "struct mapping, CBOR (RFC 8949), JSONPath (RFC 9535), canonicalization "
        "(RFC 8785), JSON diff/patch (RFC 6902). Zero dependencies."
    )
    topics = ("json", "cbor", "jsonpath", "simd", "header-only", "cpp20",
              "serialization", "rfc8259")
    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True
    exports_sources = "include/*", "LICENSE"

    def validate(self):
        check_min_cppstd(self, 20)

    def package_id(self):
        self.info.clear()  # header-only — one package for all configurations

    def package(self):
        copy(self, "*.hpp",
             src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        # Match the upstream CMake package so `find_package(qbuem_json)` +
        # `qbuem_json::qbuem_json` work identically under Conan.
        self.cpp_info.set_property("cmake_file_name", "qbuem_json")
        self.cpp_info.set_property("cmake_target_name", "qbuem_json::qbuem_json")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
