cmake_minimum_required(VERSION 3.20)
project(iox_installed_consumer LANGUAGES CXX)

option(IOX_CONSUMER_REQUIRE_ILIC "Require the optional installed iox::ilic target" OFF)

enable_testing()
find_package(iox CONFIG REQUIRED)

foreach(required_target
    iox::core
    iox::json
    iox::xml
    iox::xtf
    iox::factory
    iox::abi
)
    if(NOT TARGET ${required_target})
        message(FATAL_ERROR "Installed package does not provide ${required_target}")
    endif()
endforeach()

if(IOX_CONSUMER_REQUIRE_ILIC AND NOT TARGET iox::ilic)
    message(FATAL_ERROR "Installed package does not provide optional target iox::ilic")
endif()

add_executable(iox_installed_consumer main.cpp)
target_link_libraries(iox_installed_consumer PRIVATE
    iox::factory
    iox::abi
)
if(IOX_CONSUMER_REQUIRE_ILIC)
    target_compile_definitions(iox_installed_consumer PRIVATE IOX_CONSUMER_HAS_ILIC=1)
    target_link_libraries(iox_installed_consumer PRIVATE iox::ilic)
endif()

add_test(NAME iox_installed_consumer COMMAND iox_installed_consumer)
