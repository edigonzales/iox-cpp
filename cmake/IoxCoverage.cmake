# IoxCoverage.cmake — code coverage support for iox-cpp

if(IOX_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "IOX_ENABLE_COVERAGE requires Clang or GCC")
    endif()
    message(STATUS "Coverage instrumentation enabled")
endif()
