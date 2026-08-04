# IoxWarnings.cmake — consistent compiler warning flags for iox-cpp

function(iox_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(DEFINED ENV{IOX_WERROR} OR IOX_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(DEFINED ENV{IOX_WERROR} OR IOX_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
