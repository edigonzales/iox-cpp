vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO edigonzales/iox-cpp
    REF 9190518e7f2e1965c946053fb171677551b5ce73
    SHA512 ca02f3c404ea5e42f83a2e0f47b62fe1c8238c250a3f1596a1a82a465125d6d1c09de5fbedfecb6730b231f6dec1ee71a76c154b235adf7585f779142cc80c58
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
