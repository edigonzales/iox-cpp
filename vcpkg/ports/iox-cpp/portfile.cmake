vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO edigonzales/iox-cpp
    REF c523d0e8bfa74a01f0661dffccca75a7de8c3d17
    SHA512 0
    HEAD_REF codex-port
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        ilic IOX_ENABLE_ILIC
        geos IOX_ENABLE_GEOS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DIOX_BUILD_WASM=OFF
        -DIOX_BUILD_EXAMPLES=OFF
        -DIOX_BUILD_TOOLS=OFF
        -DIOX_FETCH_ILIC=OFF
        -DIOX_USE_SYSTEM_EXPAT=ON
        -DIOX_USE_SYSTEM_YYJSON=ON
        -DIOX_ENABLE_INSTALL=ON
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME iox
    CONFIG_PATH lib/cmake/iox
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
