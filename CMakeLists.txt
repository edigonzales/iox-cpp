cmake_minimum_required(VERSION 3.20)
project(iox-cpp VERSION 0.2.0 LANGUAGES C CXX)

# Newer CMake versions know which linkers rescan static archives. Let them
# remove duplicate archive entries while retaining compatibility with 3.20.
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()
if(POLICY CMP0179)
    cmake_policy(SET CMP0179 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(IoxWarnings)
include(IoxOptions)
include(IoxDependencies)
include(IoxSanitizers)
include(IoxCoverage)
include(IoxFuzzing)

# --- iox-core -----------------------------------------------------------
add_library(iox-core STATIC
    source/core/Basket.cpp
    source/core/Diagnostic.cpp
    source/core/Version.cpp
    source/core/IomObject.cpp
    source/core/IomValue.cpp
    source/core/FormatRegistry.cpp
)

target_include_directories(iox-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

iox_set_warnings(iox-core)

# --- iox-json ------------------------------------------------------------
iox_fetch_yyjson()

add_library(iox-json STATIC
    source/json/JsonEventReader.cpp
    source/json/JsonEventWriter.cpp
)

target_include_directories(iox-json
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(iox-json PUBLIC iox-core PRIVATE yyjson)
iox_set_warnings(iox-json)

# --- iox-xml (private Expat dependency) ---------------------------------
iox_fetch_expat()

add_library(iox-xml STATIC
    source/xml/ExpatParser.cpp
    source/xml/XmlWriter.cpp
)

target_include_directories(iox-xml
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(iox-xml PRIVATE iox-core expat)
iox_set_warnings(iox-xml)

# --- iox-xtf ------------------------------------------------------------
add_library(iox-xtf STATIC
    source/xtf/common/XtfReader.cpp
    source/xtf/common/XtfWriter.cpp
    source/xtf/v23/Xtf23Dialect.cpp
    source/xtf/v23/Xtf23Writer.cpp
    source/xtf/v24/Xtf24Dialect.cpp
    source/xtf/v24/Xtf24Writer.cpp
)

target_include_directories(iox-xtf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

target_link_libraries(iox-xtf PUBLIC iox-core PRIVATE iox-xml)
iox_set_warnings(iox-xtf)

# --- built-in factories ---------------------------------------------------
add_library(iox-factory STATIC
    source/factory/Factory.cpp
)

target_include_directories(iox-factory
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(iox-factory PUBLIC iox-xtf iox-core)
if(IOX_ENABLE_JSON_FORMAT)
    target_compile_definitions(iox-factory PRIVATE IOX_FACTORY_ENABLE_JSON=1)
    target_link_libraries(iox-factory PUBLIC iox-json)
else()
    target_compile_definitions(iox-factory PRIVATE IOX_FACTORY_ENABLE_JSON=0)
endif()
iox_set_warnings(iox-factory)

# --- optional ilic-core integration --------------------------------------
if(IOX_ENABLE_ILIC)
    iox_make_ilic_available(_IOX_ILIC_SOURCE_DIR)

    add_library(iox-ilic STATIC
        source/ilic/IlicModelIndex.cpp
        source/ilic/IlicXtf.cpp
    )

    target_include_directories(iox-ilic
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${_IOX_ILIC_SOURCE_DIR}/source>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(iox-ilic PUBLIC iox-xtf iox-core ilic::core)
    iox_set_warnings(iox-ilic)
endif()

# --- iox-dump tool -------------------------------------------------------
if(IOX_BUILD_TOOLS)
    add_executable(iox-dump tools/iox-dump.cpp)
    target_link_libraries(iox-dump PRIVATE iox-xtf iox-json iox-core)
    iox_set_warnings(iox-dump)
endif()

# --- Examples ------------------------------------------------------------
if(IOX_BUILD_EXAMPLES)
    add_executable(cpp-read-events examples/cpp-read-events.cpp)
    target_link_libraries(cpp-read-events PRIVATE iox-xtf iox-core)
    iox_set_warnings(cpp-read-events)

    add_executable(cpp-roundtrip examples/cpp-roundtrip.cpp)
    target_link_libraries(cpp-roundtrip PRIVATE iox-xtf iox-core)
    iox_set_warnings(cpp-roundtrip)

    add_executable(cpp-custom-format examples/cpp-custom-format.cpp)
    target_link_libraries(cpp-custom-format PRIVATE iox-json iox-core)
    iox_set_warnings(cpp-custom-format)
endif()

# --- iox-abi -------------------------------------------------------------
add_library(iox-abi STATIC
    source/abi/iox.cpp
)

target_include_directories(iox-abi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(iox-abi PUBLIC iox-core iox-xtf iox-json)
iox_set_warnings(iox-abi)

if(IOX_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "IOX_BUILD_WASM requires the Emscripten toolchain")
    endif()
    add_executable(iox-wasm source/abi/iox.cpp)
    target_include_directories(iox-wasm PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(iox-wasm PRIVATE iox-core iox-xtf iox-json)
    target_compile_options(iox-wasm PRIVATE "-fexceptions")
    target_link_options(iox-wasm PRIVATE
        "--no-entry"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web,worker,node"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sWASM_BIGINT=1"
        "-sASSERTIONS=1"
        "-fexceptions"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']"
        "SHELL:-sEXPORTED_FUNCTIONS=['_iox_abi_version','_iox_version','_iox_alloc','_iox_free','_iox_reader_create','_iox_reader_destroy','_iox_reader_feed','_iox_reader_finish','_iox_reader_next','_iox_writer_create','_iox_writer_destroy','_iox_writer_write_event_json','_iox_writer_take_output','_iox_writer_finish','_iox_result_json','_iox_result_bytes','_iox_result_size','_iox_result_status','_iox_result_destroy']")
    set_target_properties(iox-wasm PROPERTIES OUTPUT_NAME "iox-wasm" SUFFIX ".mjs")
endif()

# Stable target aliases for consumers of the native SDK.
add_library(iox::core ALIAS iox-core)
add_library(iox::json ALIAS iox-json)
add_library(iox::xtf ALIAS iox-xtf)
add_library(iox::abi ALIAS iox-abi)
add_library(iox::factory ALIAS iox-factory)
if(IOX_ENABLE_ILIC)
    add_library(iox::ilic ALIAS iox-ilic)
endif()

# --- Tests ---------------------------------------------------------------
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
