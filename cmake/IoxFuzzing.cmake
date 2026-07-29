# IoxFuzzing.cmake — libFuzzer support for iox-cpp
#
# Only active when IOX_ENABLE_FUZZING=ON and the compiler is Clang.

if(IOX_ENABLE_FUZZING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(IOX_FUZZ_FLAGS -fsanitize=fuzzer,address -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR "IOX_ENABLE_FUZZING requires Clang")
    endif()

    function(iox_add_fuzz_target name)
        add_executable(${name} ${ARGN})
        target_compile_options(${name} PRIVATE ${IOX_FUZZ_FLAGS})
        target_link_options(${name} PRIVATE ${IOX_FUZZ_FLAGS})
        target_link_libraries(${name} PRIVATE iox-core iox-xtf)
    endfunction()
endif()
