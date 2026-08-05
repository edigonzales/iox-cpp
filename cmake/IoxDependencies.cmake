# IoxDependencies.cmake — pinned third-party dependencies for iox-cpp
#
# Expat is fetched with a pinned, immutable revision recorded in
# docs/conformance.md.  Never use a floating branch here.

include(FetchContent)

set(IOX_ILIC_VERSION "0.9.10" CACHE STRING
    "Stable ilic release version used by the optional integration")
set(IOX_ILIC_GIT_TAG "v${IOX_ILIC_VERSION}" CACHE STRING
    "Exact ilic tag or commit used by the optional integration")

# ---------------------------------------------------------------------------
# Expat — pinned immutable revision
# ---------------------------------------------------------------------------
set(IOX_EXPAT_VERSION "R_2_6_4")
set(IOX_EXPAT_SHA "a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee")

FetchContent_Declare(
    expat
    URL "https://github.com/libexpat/libexpat/releases/download/${IOX_EXPAT_VERSION}/expat-2.6.4.tar.xz"
    URL_HASH "SHA256=${IOX_EXPAT_SHA}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# We only fetch Expat when actually needed (Phase 2+).
# For Phase 0/1 we do not pull it.
function(iox_fetch_expat)
    set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(expat)
endfunction()

# ---------------------------------------------------------------------------
# yyjson — small private JSON parser/writer used only by iox-json
# ---------------------------------------------------------------------------
function(iox_fetch_yyjson)
    set(YYJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YYJSON_BUILD_FUZZER OFF CACHE BOOL "" FORCE)
    set(YYJSON_BUILD_MISC OFF CACHE BOOL "" FORCE)
    set(YYJSON_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(YYJSON_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        yyjson
        GIT_REPOSITORY https://github.com/ibireme/yyjson.git
        GIT_TAG 8b4a38dc994a110abaec8a400615567bd996105f
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(yyjson)
endfunction()

# ---------------------------------------------------------------------------
# GEOS — optional system/vcpkg dependency; never downloaded here
# ---------------------------------------------------------------------------
function(iox_find_geos)
    find_package(GEOS CONFIG REQUIRED)
endfunction()

# ---------------------------------------------------------------------------
# ilic — optional direct source integration pinned to a stable release tag
# ---------------------------------------------------------------------------
function(iox_make_ilic_available out_source_dir)
    set(ILIC_BUILD_CLI OFF CACHE BOOL "" FORCE)
    set(ILIC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ILIC_ENABLE_NATIVE_REPOSITORY OFF CACHE BOOL "" FORCE)

    if(IOX_ILIC_SOURCE_DIR)
        if(NOT EXISTS "${IOX_ILIC_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "IOX_ILIC_SOURCE_DIR does not contain ilic-fork: ${IOX_ILIC_SOURCE_DIR}")
        endif()
        add_subdirectory(
            "${IOX_ILIC_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/ilic-core-build"
            EXCLUDE_FROM_ALL
        )
        set(_iox_ilic_source_dir "${IOX_ILIC_SOURCE_DIR}")
    elseif(IOX_FETCH_ILIC)
        FetchContent_Declare(ilic
            GIT_REPOSITORY https://github.com/edigonzales/ilic-fork.git
            GIT_TAG "${IOX_ILIC_GIT_TAG}"
            GIT_SHALLOW FALSE
        )
        FetchContent_MakeAvailable(ilic)
        set(_iox_ilic_source_dir "${ilic_SOURCE_DIR}")
    else()
        message(FATAL_ERROR
            "IOX_ENABLE_ILIC requires IOX_ILIC_SOURCE_DIR or IOX_FETCH_ILIC")
    endif()

    if(NOT TARGET ilic::core)
        message(FATAL_ERROR
            "ilic ${IOX_ILIC_VERSION} must provide target ilic::core")
    endif()
    if(TARGET ilic OR TARGET ilic-format)
        message(FATAL_ERROR
            "ilic CLI targets must be disabled in the iox-cpp consumer build")
    endif()
    if(NOT EXISTS "${_iox_ilic_source_dir}/source/metamodel/MetaModelStore.h")
        message(FATAL_ERROR
            "ilic ${IOX_ILIC_VERSION} does not provide the expected metamodel source API")
    endif()

    set(${out_source_dir} "${_iox_ilic_source_dir}" PARENT_SCOPE)
endfunction()
