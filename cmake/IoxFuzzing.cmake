# IoxFuzzing.cmake — libFuzzer support for iox-cpp
#
# Only active when IOX_ENABLE_FUZZING=ON and the compiler is Clang.

if(IOX_ENABLE_FUZZING)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        execute_process(
            COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
            OUTPUT_VARIABLE _iox_clang_resource_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        file(GLOB_RECURSE _iox_fuzzer_runtimes
            "${_iox_clang_resource_dir}/lib/libclang_rt.fuzzer*.a"
            "${_iox_clang_resource_dir}/lib/*/libclang_rt.fuzzer*.a")
        if(_iox_fuzzer_runtimes)
            # Dependencies use the no-link variant so the libFuzzer main
            # function is supplied only by each fuzz executable.
            set(IOX_FUZZ_LIBFUZZER_AVAILABLE ON)
            set(IOX_FUZZ_COMPILE_FLAGS -fsanitize=fuzzer-no-link,address -fno-omit-frame-pointer)
            set(IOX_FUZZ_LINK_FLAGS -fsanitize=fuzzer,address -fno-omit-frame-pointer)
        else()
            # Apple Command Line Tools ship the fuzzer headers but not the
            # compiler-rt archive. Use the deterministic standalone driver in
            # the harness so fuzz targets remain buildable and runnable.
            set(IOX_FUZZ_LIBFUZZER_AVAILABLE OFF)
            set(IOX_FUZZ_COMPILE_FLAGS -fno-omit-frame-pointer)
            set(IOX_FUZZ_LINK_FLAGS)
        endif()
    else()
        message(FATAL_ERROR "IOX_ENABLE_FUZZING requires Clang")
    endif()

    function(iox_add_fuzz_target name)
        add_executable(${name} ${ARGN})
        target_compile_options(${name} PRIVATE ${IOX_FUZZ_COMPILE_FLAGS})
        target_link_options(${name} PRIVATE ${IOX_FUZZ_LINK_FLAGS})
        if(NOT IOX_FUZZ_LIBFUZZER_AVAILABLE)
            target_compile_definitions(${name} PRIVATE IOX_STANDALONE_FUZZ=1)
        endif()
        target_link_libraries(${name} PRIVATE iox-core)
    endfunction()
endif()
