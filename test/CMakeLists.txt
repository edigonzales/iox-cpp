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
    core/Diagnostic.test.cpp
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
target_include_directories(iox-test-xml-writer PRIVATE "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(iox-test-xml-writer PRIVATE iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xml.writer COMMAND iox-test-xml-writer)

add_executable(iox-test-expat-security xml/ExpatSecurity.test.cpp)
target_include_directories(iox-test-expat-security PRIVATE "${CMAKE_SOURCE_DIR}/source")
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

add_executable(iox-test-xtf23-writer
    xtf23/Xtf23Writer.test.cpp
)
target_link_libraries(iox-test-xtf23-writer PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf23.writer COMMAND iox-test-xtf23-writer)

# --- XTF 2.4 tests ----------------------------------------------------------
add_executable(iox-test-xtf24-basic
    xtf24/Xtf24Basic.test.cpp
)
target_link_libraries(iox-test-xtf24-basic PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.basic COMMAND iox-test-xtf24-basic)

add_executable(iox-test-xtf24-namespace xtf24/Xtf24Namespace.test.cpp)
target_link_libraries(iox-test-xtf24-namespace PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.namespace COMMAND iox-test-xtf24-namespace)

add_executable(iox-test-xtf24-conformance
    xtf24/Xtf24Conformance.test.cpp
)
target_link_libraries(iox-test-xtf24-conformance PRIVATE
    iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.xtf24.conformance COMMAND iox-test-xtf24-conformance)

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
         "<ili:HEADERSECTION SENDER=\"CTest\" VERSION=\"2.3\">"
         "<ili:MODELS><ili:MODEL NAME=\"Smoke\"/></ili:MODELS>"
         "</ili:HEADERSECTION><ili:DATASECTION/>"
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

add_executable(iox-test-iox-ili-porting
    conformance/IoxIliPorted.test.cpp
)
target_include_directories(iox-test-iox-ili-porting PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(iox-test-iox-ili-porting PRIVATE iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.iox_ili.porting COMMAND iox-test-iox-ili-porting)
set_tests_properties(iox.test.iox_ili.porting PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(iox-test-release-conformance
    conformance/ReleaseConformance.test.cpp
)
target_include_directories(iox-test-release-conformance PRIVATE
    "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/conformance")
target_link_libraries(iox-test-release-conformance PRIVATE
    iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.release_conformance
         COMMAND iox-test-release-conformance)
set_tests_properties(iox.test.release_conformance PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME iox.test.iox_ili.fixture_manifest
         COMMAND "${CMAKE_SOURCE_DIR}/scripts/verify-iox-ili-fixtures.sh")
add_test(NAME iox.test.iox_ili.method_matrix
         COMMAND "${CMAKE_SOURCE_DIR}/scripts/verify-porting-matrix.sh")
set_tests_properties(iox.test.iox_ili.fixture_manifest PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_tests_properties(iox.test.iox_ili.method_matrix PROPERTIES
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
target_include_directories(iox-test-coverage-edges PRIVATE "${CMAKE_SOURCE_DIR}/source")
target_link_libraries(iox-test-coverage-edges PRIVATE iox-abi iox-factory iox-json iox-xtf iox-xml iox-core iox-test-support)
add_test(NAME iox.test.coverage_edges COMMAND iox-test-coverage-edges)

if(IOX_ENABLE_COVERAGE)
    set(IOX_COVERAGE_TEST_SOURCES
        ${CORE_TEST_FILES}
        core/Basket.test.cpp
        core/Factory.test.cpp
        json/JsonEventRoundtrip.test.cpp
        json/JsonChunked.test.cpp
        xml/XmlWriter.test.cpp
        xml/ExpatSecurity.test.cpp
        xtf23/Xtf23Header.test.cpp
        xtf23/Xtf23Objects.test.cpp
        xtf23/Xtf23ReaderState.test.cpp
        xtf23/Xtf23Geometry.test.cpp
        xtf23/Xtf23Writer.test.cpp
        xtf24/Xtf24Basic.test.cpp
        xtf24/Xtf24Namespace.test.cpp
        xtf24/Xtf24Conformance.test.cpp
        xtf24/Xtf24Geometry.test.cpp
        conformance/Conformance.test.cpp
        conformance/IoxIliPorted.test.cpp
        conformance/ReleaseConformance.test.cpp
        conformance/Associations.test.cpp
        conformance/LargeStreaming.test.cpp
        coverage/CoverageEdges.test.cpp
        coverage/AbiSmokeCoverage.test.cpp
        abi/abi_smoke.c
    )
    list(REMOVE_ITEM IOX_COVERAGE_TEST_SOURCES core/EventKind.test.cpp)
    if(IOX_ENABLE_ILIC)
        list(APPEND IOX_COVERAGE_TEST_SOURCES ilic/ModelBased.test.cpp)
    endif()
    add_executable(iox-test-coverage-all
        coverage/AllTestsMain.cpp
        ${IOX_COVERAGE_TEST_SOURCES}
    )
    target_compile_definitions(iox-test-coverage-all PRIVATE
        IOX_TEST_NO_MAIN=1 IOX_ABI_SMOKE_NO_MAIN=1)
    target_include_directories(iox-test-coverage-all PRIVATE
        "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/source"
        "${CMAKE_CURRENT_SOURCE_DIR}/conformance")
    target_link_libraries(iox-test-coverage-all PRIVATE
        iox-abi iox-factory iox-json iox-xtf iox-xml iox-core
        iox-test-support)
    if(IOX_ENABLE_ILIC)
        target_link_libraries(iox-test-coverage-all PRIVATE iox-ilic)
    endif()
endif()

if(IOX_ENABLE_FUZZING)
    set(IOX_FUZZ_TARGETS
        "iox_fuzz_xml_reader|fuzz/XmlReaderFuzz.cpp|xml|iox-xml"
        "iox_fuzz_xtf_reader|fuzz/XtfReaderFuzz.cpp|xtf|iox-xtf"
        "iox_fuzz_event_json|fuzz/EventJsonFuzz.cpp|json|iox-json"
        "iox_fuzz_iom_object|fuzz/IomObjectFuzz.cpp|iom|iox-core"
        "iox_fuzz_writer_events|fuzz/WriterEventsFuzz.cpp|writer|iox-xtf"
    )
    foreach(fuzz_spec IN LISTS IOX_FUZZ_TARGETS)
        string(REPLACE "|" ";" fuzz_fields "${fuzz_spec}")
        list(GET fuzz_fields 0 fuzz_target)
        list(GET fuzz_fields 1 fuzz_source)
        list(GET fuzz_fields 2 fuzz_corpus)
        list(GET fuzz_fields 3 fuzz_library)
        iox_add_fuzz_target(${fuzz_target} ${fuzz_source})
        target_include_directories(${fuzz_target} PRIVATE
            "${CMAKE_SOURCE_DIR}/source")
        target_link_libraries(${fuzz_target} PRIVATE ${fuzz_library})
        if(IOX_FUZZ_LIBFUZZER_AVAILABLE)
            add_test(NAME iox.fuzz.${fuzz_corpus}
                COMMAND ${fuzz_target} -runs=50
                    "${CMAKE_SOURCE_DIR}/test/fuzz/corpus/${fuzz_corpus}")
        else()
            add_test(NAME iox.fuzz.${fuzz_corpus}
                COMMAND ${fuzz_target} --runs 50
                    "${CMAKE_SOURCE_DIR}/test/fuzz/corpus/${fuzz_corpus}/seed")
        endif()
        set_tests_properties(iox.fuzz.${fuzz_corpus} PROPERTIES LABELS fuzz)
    endforeach()
endif()

# --- ilic model-based tests --------------------------------------------------
if(IOX_ENABLE_ILIC)
    add_executable(iox-test-ilic-version
        ilic/IlicDependencyVersion.test.cpp
    )
    target_link_libraries(iox-test-ilic-version PRIVATE
        ilic::core
        iox-test-support
    )
    target_compile_definitions(iox-test-ilic-version PRIVATE
        IOX_EXPECTED_ILIC_VERSION="${IOX_ILIC_VERSION}"
    )
    add_test(NAME iox.test.ilic.version COMMAND iox-test-ilic-version)

    add_executable(iox-test-model-based
        ilic/ModelBased.test.cpp
    )
    target_link_libraries(iox-test-model-based PRIVATE iox-ilic iox-xtf iox-xml iox-core iox-test-support)
    add_test(NAME iox.test.ilic.model_based COMMAND iox-test-model-based)
endif()
