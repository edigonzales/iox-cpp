vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO edigonzales/iox-cpp
    REF da8340517fb9603c6729bddd51c0626180cd7a52
    SHA512 bf53c44dd360371ded1fa2c15662101296e796b4f4eafdb1f81ccd49bfb8e98b5b8ec036853dabac6531fb1a1ffe84dd705057527d5ff91f3409a03fead4ecb9
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
