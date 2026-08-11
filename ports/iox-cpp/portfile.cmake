vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO edigonzales/iox-cpp
    REF fa2269aacdfcc5adb3a9c7ce557d73b622b3633e
    SHA512 3a2c42a73bd0200c6485cacef8d4cb3dd9cbb39d8fd4316db873bd96d15eb9d182452e14a5bdc6debeb2938300ed73984da017a30f0c4465eac3c9a037fbb03f
    HEAD_REF codex-port
)

vcpkg_check_features(
    OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        ilic IOX_ENABLE_ILIC
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
