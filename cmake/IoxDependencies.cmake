# IoxDependencies.cmake — pinned third-party dependencies for iox-cpp
#
# Expat is fetched with a pinned, immutable revision recorded in
# docs/conformance.md unless an installed package is explicitly requested.

include(FetchContent)

# ---------------------------------------------------------------------------
# Expat — pinned immutable revision or package-managed dependency
# ---------------------------------------------------------------------------
set(IOX_EXPAT_VERSION "R_2_6_4")
set(IOX_EXPAT_SHA "a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee")

FetchContent_Declare(
    expat
    URL "https://github.com/libexpat/libexpat/releases/download/${IOX_EXPAT_VERSION}/expat-2.6.4.tar.xz"
    URL_HASH "SHA256=${IOX_EXPAT_SHA}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Keep the historical FetchContent path as the default. Installable/package-
# manager builds opt into IOX_USE_SYSTEM_EXPAT so exported iox targets refer to
# a relocatable imported dependency rather than a source-tree target.
function(iox_fetch_expat)
    if(IOX_USE_SYSTEM_EXPAT)
        find_package(expat CONFIG REQUIRED)
        if(NOT TARGET expat::expat)
            message(FATAL_ERROR
                "IOX_USE_SYSTEM_EXPAT requires the expat::expat CMake target")
        endif()
        set(IOX_EXPAT_TARGET expat::expat PARENT_SCOPE)
        return()
    endif()

    set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(expat)
    set(IOX_EXPAT_TARGET expat PARENT_SCOPE)
endfunction()
