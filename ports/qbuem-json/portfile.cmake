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
    SHA512 1984935ee469ddaa5096595467009b8e63fd4f44a1783523ae42f1e2fa59f3e5bce10c5092396d0a66301b9e9d81efe091b0f6c19cf5f5015049d18f08363c2d
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
