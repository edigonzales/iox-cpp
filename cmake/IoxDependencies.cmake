# IoxDependencies.cmake — pinned third-party dependencies for iox-cpp
#
# Expat is fetched with a pinned, immutable revision recorded in
# docs/conformance.md.  Never use a floating branch here.

include(FetchContent)

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
