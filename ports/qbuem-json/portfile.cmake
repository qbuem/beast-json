# Header-only library. Build the (tests/benchmarks-off) install tree, which now
# ships qbuem_jsonConfig.cmake, then let vcpkg relocate it.
#
# SHA512 is of the GitHub source tarball for the tag matching this port's version
# (ports/qbuem-json/vcpkg.json). When bumping the version, recompute with:
#   curl -sL https://github.com/qbuem/qbuem-json/archive/refs/tags/v<VER>.tar.gz | shasum -a 512
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO qbuem/qbuem-json
    REF "v${VERSION}"
    SHA512 63b5e9bd59bc257d3600f7c4ef7a9abdf6aeb573f9de9526d40920298ff0eb919f90b96b30e3e0696f302c8dad57f2a8583016fc45ba2bf5c03ede5233ef86f5
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
