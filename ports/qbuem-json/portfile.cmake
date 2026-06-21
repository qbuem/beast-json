# Header-only library. Build the (tests/benchmarks-off) install tree, which now
# ships qbuem_jsonConfig.cmake, then let vcpkg relocate it.
#
# NB: SHA512 is finalized when the matching release tag is published — replace
# the placeholder below with the SHA512 of the v${VERSION} source tarball
# (vcpkg will print the correct value on the first build attempt).
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO qbuem/qbuem-json
    REF "v${VERSION}"
    SHA512 0
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DQBUEM_JSON_BUILD_TESTS=OFF
        -DQBUEM_JSON_BUILD_BENCHMARKS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME qbuem_json CONFIG_PATH lib/cmake/qbuem_json)

# Header-only: no libraries or debug tree to keep.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/lib" "${CURRENT_PACKAGES_DIR}/debug")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
