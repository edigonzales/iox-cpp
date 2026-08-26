include_guard(GLOBAL)

function(iox_configure_packaging)
    if(NOT IOX_ENABLE_INSTALL OR EMSCRIPTEN)
        return()
    endif()

    if(NOT IOX_USE_SYSTEM_EXPAT)
        message(FATAL_ERROR
            "IOX_ENABLE_INSTALL requires IOX_USE_SYSTEM_EXPAT=ON so the exported package has a relocatable Expat dependency")
    endif()
    if(NOT IOX_USE_SYSTEM_YYJSON)
        message(FATAL_ERROR
            "IOX_ENABLE_INSTALL requires IOX_USE_SYSTEM_YYJSON=ON so the exported package has a relocatable yyjson dependency")
    endif()

    if(IOX_ENABLE_ILIC AND (IOX_ILIC_SOURCE_DIR OR IOX_FETCH_ILIC))
        message(FATAL_ERROR
            "IOX_ENABLE_INSTALL with IOX_ENABLE_ILIC requires an installed ilic package; IOX_ILIC_SOURCE_DIR and IOX_FETCH_ILIC are source-build modes")
    endif()

    set(_iox_export_targets
        iox-core
        iox-geometry
        iox-json
        iox-xml
        iox-xtf
        iox-factory
        iox-abi
    )
    if(IOX_ENABLE_ILIC)
        list(APPEND _iox_export_targets iox-ilic)
    endif()

    foreach(required_target IN LISTS _iox_export_targets)
        if(NOT TARGET ${required_target})
            message(FATAL_ERROR
                "IOX_ENABLE_INSTALL requires target ${required_target}")
        endif()
    endforeach()

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set_target_properties(iox-core PROPERTIES EXPORT_NAME core)
    set_target_properties(iox-geometry PROPERTIES EXPORT_NAME geometry)
    set_target_properties(iox-json PROPERTIES EXPORT_NAME json)
    set_target_properties(iox-xml PROPERTIES EXPORT_NAME xml)
    set_target_properties(iox-xtf PROPERTIES EXPORT_NAME xtf)
    set_target_properties(iox-factory PROPERTIES EXPORT_NAME factory)
    set_target_properties(iox-abi PROPERTIES EXPORT_NAME abi)
    if(IOX_ENABLE_ILIC)
        set_target_properties(iox-ilic PROPERTIES EXPORT_NAME ilic)
    endif()

    install(TARGETS ${_iox_export_targets}
        EXPORT ioxTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )

    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/iox"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
            PATTERN "*.h"
            PATTERN "ilic" EXCLUDE
    )
    if(IOX_ENABLE_ILIC)
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/iox/ilic"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/iox"
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

    set(IOX_PACKAGE_HAS_ILIC "${IOX_ENABLE_ILIC}")
    set(IOX_PACKAGE_HAS_GEOS "${IOX_ENABLE_GEOS}")
    set(_iox_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/iox")
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/cmake")

    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/ioxConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/cmake/ioxConfig.cmake"
        INSTALL_DESTINATION "${_iox_package_dir}"
    )
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/cmake/ioxConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT ioxTargets
        FILE ioxTargets.cmake
        NAMESPACE iox::
        DESTINATION "${_iox_package_dir}"
    )
    install(FILES
        "${PROJECT_BINARY_DIR}/cmake/ioxConfig.cmake"
        "${PROJECT_BINARY_DIR}/cmake/ioxConfigVersion.cmake"
        DESTINATION "${_iox_package_dir}"
    )
endfunction()

if(IOX_ENABLE_INSTALL AND NOT EMSCRIPTEN)
    cmake_language(DEFER CALL iox_configure_packaging)
endif()
