# Test support header
add_library(iox-test-support INTERFACE)
target_include_directories(iox-test-support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
)

# --- Core tests (each in its own executable) -------------------------------
set(CORE_TEST_FILES
    core/Version.test.cpp
    core/ByteView.test.cpp
    core/IomValue.test.cpp
    core/IomObject.test.cpp
    core/FormatRegistry.test.cpp
    core/EventKind.test.cpp
)

foreach(test_file ${CORE_TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(iox-test-${test_name} ${test_file})
    target_link_libraries(iox-test-${test_name} PRIVATE iox-core iox-test-support)
    add_test(NAME iox.test.${test_name} COMMAND iox-test-${test_name})
endforeach()

add_executable(iox-test-Basket core/Basket.test.cpp)
target_link_libraries(iox-test-Basket PRIVATE iox-json iox-core iox-test-support)
add_test(NAME iox.test.Basket COMMAND iox-test-Basket)

add_executable(iox-test-Factory core/Factory.test.cpp)
target_link_libraries(iox-test-Factory PRIVATE iox-factory iox-json iox-xtf iox-core iox-test-support)
add_test(NAME iox.test.Factory COMMAND iox-test-Factory)

# --- JSON tests -----------------------------------------------------------
add_executable(iox-test-json-roundtrip
    json/JsonEventRoundtrip.test.cpp
)
target_link_libraries(iox-test-json-roundtrip PRIVATE iox-json iox-core iox-test-support)
add_test(NAME iox.test.json.roundtrip COMMAND iox-test-json-roundtrip)

add_executable(iox-test-json-chunked
    json/JsonChunked.test.cpp
)
target_link_libraries(iox-test-json-chunked PRIVATE iox-json iox-core iox-test-support)
add_test(NAME iox.test.json.chunked COMMAND iox-test-json-chunked)

# --- ABI C smoke test ----------------------------------------------------
add_executable(iox-test-abi-c
    abi/abi_smoke.c
)
target_link_libraries(iox-test-abi-c PRIVATE iox-abi)
add_test(NAME iox.test.abi.c_smoke COMMAND iox-test-abi-c)

# --- XML tests ------------------------------------------------------------
add_executable(iox-test-xml-writer
    xml/XmlWriter.test.cpp
)
target_link_libraries(iox-test-xml-writer PRIVATE iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xml.writer COMMAND iox-test-xml-writer)

add_executable(iox-test-expat-security xml/ExpatSecurity.test.cpp)
target_link_libraries(iox-test-expat-security PRIVATE iox-xml iox-core iox-test-support)
add_test(NAME iox.test.expat.security COMMAND iox-test-expat-security)

# --- XTF 2.3 header tests --------------------------------------------------
add_executable(iox-test-xtf23-header
    xtf23/Xtf23Header.test.cpp
)
target_link_libraries(iox-test-xtf23-header PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf23.header COMMAND iox-test-xtf23-header)

# --- XTF 2.3 object tests --------------------------------------------------
add_executable(iox-test-xtf23-objects
    xtf23/Xtf23Objects.test.cpp
)
target_link_libraries(iox-test-xtf23-objects PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf23.objects COMMAND iox-test-xtf23-objects)

add_executable(iox-test-xtf23-state xtf23/Xtf23ReaderState.test.cpp)
target_link_libraries(iox-test-xtf23-state PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf23.state COMMAND iox-test-xtf23-state)

# --- XTF 2.3 geometry tests -------------------------------------------------
add_executable(iox-test-xtf23-geometry
    xtf23/Xtf23Geometry.test.cpp
)
target_link_libraries(iox-test-xtf23-geometry PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf23.geometry COMMAND iox-test-xtf23-geometry)

# --- XTF 2.4 tests ----------------------------------------------------------
add_executable(iox-test-xtf24-basic
    xtf24/Xtf24Basic.test.cpp
)
target_link_libraries(iox-test-xtf24-basic PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.basic COMMAND iox-test-xtf24-basic)

add_executable(iox-test-xtf24-namespace xtf24/Xtf24Namespace.test.cpp)
target_link_libraries(iox-test-xtf24-namespace PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.namespace COMMAND iox-test-xtf24-namespace)

# --- XTF 2.4 geometry tests --------------------------------------------------
add_executable(iox-test-xtf24-geometry
    xtf24/Xtf24Geometry.test.cpp
)
target_link_libraries(iox-test-xtf24-geometry PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.geometry COMMAND iox-test-xtf24-geometry)
set_tests_properties(iox.test.xtf24.geometry PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# --- iox-dump smoke test ----------------------------------------------------
# Create a minimal XTF fixture and run iox-dump on it
if(IOX_BUILD_TOOLS)
    set(IOX_DUMP_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/smoke.xtf")
    file(WRITE "${IOX_DUMP_FIXTURE}"
         "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
         "<ili:TRANSFER xmlns:ili=\"http://www.interlis.ch/INTERLIS2.3\">"
         "<ili:HEADERSECTION>"
         "<ili:SENDER>CTest</ili:SENDER>"
         "<ili:SOFTWARE>iox-cpp</ili:SOFTWARE>"
         "</ili:HEADERSECTION>"
         "</ili:TRANSFER>")
    add_test(NAME iox.test.tools.iox_dump
             COMMAND iox-dump "${IOX_DUMP_FIXTURE}")
    set_tests_properties(iox.test.tools.iox_dump PROPERTIES
                         DEPENDS iox-dump)

    set(IOX_DATA_FIXTURE "${CMAKE_SOURCE_DIR}/test/fixtures/xtf23/dataSection/EmptyBasket.xtf")
    add_test(NAME iox.test.tools.iox_dump.events
             COMMAND iox-dump --events "${IOX_DATA_FIXTURE}")
    add_test(NAME iox.test.tools.iox_dump.roundtrip
             COMMAND iox-dump --roundtrip "${IOX_DATA_FIXTURE}"
                     "${CMAKE_CURRENT_BINARY_DIR}/dump-roundtrip.xtf")
    set_tests_properties(iox.test.tools.iox_dump.events
                         iox.test.tools.iox_dump.roundtrip
                         PROPERTIES DEPENDS iox-dump)
endif()

if(IOX_BUILD_EXAMPLES)
    set(IOX_EXAMPLE_FIXTURE "${CMAKE_SOURCE_DIR}/test/fixtures/xtf23/dataSection/EmptyBasket.xtf")
    add_test(NAME iox.test.example.cpp_read_events
             COMMAND cpp-read-events "${IOX_EXAMPLE_FIXTURE}")
    add_test(NAME iox.test.example.cpp_roundtrip
             COMMAND cpp-roundtrip "${IOX_EXAMPLE_FIXTURE}"
                     "${CMAKE_CURRENT_BINARY_DIR}/example-roundtrip.xtf")
    add_test(NAME iox.test.example.cpp_custom_format
             COMMAND cpp-custom-format)
endif()

# --- Conformance tests (iox-ili fixtures) ------------------------------------
add_executable(iox-test-conformance
    conformance/Conformance.test.cpp
)
target_include_directories(iox-test-conformance PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(iox-test-conformance PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.conformance COMMAND iox-test-conformance)
set_tests_properties(iox.test.conformance PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# --- Association tests -------------------------------------------------------
add_executable(iox-test-associations
    conformance/Associations.test.cpp
)
target_link_libraries(iox-test-associations PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.associations COMMAND iox-test-associations)

# --- Large streaming and public consumer tests ----------------------------
add_executable(iox-test-large-streaming
    conformance/LargeStreaming.test.cpp
)
target_link_libraries(iox-test-large-streaming PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.large_streaming COMMAND iox-test-large-streaming)

add_executable(iox-test-public-headers
    public/PublicHeaders.cpp
)
target_link_libraries(iox-test-public-headers PRIVATE iox-factory iox-abi iox-json iox-xtf iox-xml iox-core)
add_test(NAME iox.test.public_headers COMMAND iox-test-public-headers)

add_executable(iox-test-coverage-edges
    coverage/CoverageEdges.test.cpp
)
target_link_libraries(iox-test-coverage-edges PRIVATE iox-abi iox-factory iox-json iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.coverage_edges COMMAND iox-test-coverage-edges)

if(IOX_ENABLE_FUZZING)
    iox_add_fuzz_target(iox-fuzz-xtf
        fuzz/XtfReaderFuzz.cpp
    )
    if(IOX_FUZZ_LIBFUZZER_AVAILABLE)
        add_test(NAME iox.fuzz.xtf
            COMMAND iox-fuzz-xtf -runs=50 "${CMAKE_SOURCE_DIR}/test/fuzz/corpus")
    else()
        add_test(NAME iox.fuzz.xtf
            COMMAND iox-fuzz-xtf --runs 50 "${CMAKE_SOURCE_DIR}/test/fuzz/corpus/empty.xtf")
    endif()
endif()

# --- ilic model-based tests --------------------------------------------------
if(IOX_ENABLE_ILIC)
    add_executable(iox-test-model-based
        ilic/ModelBased.test.cpp
    )
    target_link_libraries(iox-test-model-based PRIVATE iox-ilic iox-xtf iox-xml iox-core iox-test-support)
    add_test(NAME iox.test.ilic.model_based COMMAND iox-test-model-based)
endif()
