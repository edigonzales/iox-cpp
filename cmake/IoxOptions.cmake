# IoxOptions.cmake — build options for iox-cpp

option(IOX_BUILD_WASM "Build the WebAssembly ABI target" OFF)
option(IOX_ENABLE_ILIC "Build direct ilic-core integration" OFF)
option(IOX_FETCH_ILIC "Fetch the pinned ilic-fork dependency" OFF)
option(IOX_ENABLE_JSON_FORMAT "Register JSON events in the default factory" ON)
option(IOX_BUILD_EXAMPLES "Build example programs" ON)
option(IOX_BUILD_TOOLS "Build tools like iox-dump" ON)
option(IOX_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(IOX_WERROR "Deprecated alias for IOX_WARNINGS_AS_ERRORS" OFF)
if(IOX_WERROR)
    set(IOX_WARNINGS_AS_ERRORS ON CACHE BOOL "" FORCE)
endif()

# Coverage option — only for compatible compilers
option(IOX_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

# Fuzz option — only for Clang+libFuzzer
option(IOX_ENABLE_FUZZING "Build fuzz targets" OFF)

# Sanitizer options
option(IOX_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(IOX_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(IOX_ENABLE_LSAN "Enable LeakSanitizer (Linux only)" OFF)

# ilic-core integration
set(IOX_ILIC_SOURCE_DIR "" CACHE PATH "Path to ilic-fork source directory")
