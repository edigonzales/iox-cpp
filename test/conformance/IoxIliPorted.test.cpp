// Behavioral port coverage for the pinned iox-ili XTF test corpus.
// The Java source remains the provenance; assertions target the C++ event API.

#include "IoxIliTestSupport.h"
#include "iox/test/Test.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

const std::filesystem::path fixtureRoot = "test/fixtures";

std::vector<iox::IoxEvent> orderedReferenceEvents() {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent transfer;
    transfer.header.version = iox::XtfVersion::V23;
    transfer.header.sender = "matrix";
    events.push_back(transfer);

    iox::StartBasketEvent basket;
    basket.basket.topic = iox::IomName("Matrix.Topic.Data");
    basket.basket.basketId = "B1";
    events.push_back(basket);

    iox::ObjectEvent object;
    object.object = iox::IomObject(
        iox::IomName("Matrix.Topic.Class"), "T1");
    object.object.setOperation(iox::ObjectOperation::Insert);
    object.object.setPrimitive(iox::IomName("First"), "one");
    object.object.appendPrimitive(iox::IomName("Repeated"), "a");
    object.object.appendPrimitive(iox::IomName("Repeated"), "b");
    iox::IomObject reference(iox::IomName("REFERENCE"));
    reference.setReference({"T2", std::nullopt, 2U});
    object.object.setObject(iox::IomName("Reference"), reference);
    events.push_back(object);

    events.push_back(iox::EndBasketEvent{});
    events.push_back(iox::EndTransferEvent{});
    return events;
}

} // namespace

IOX_TEST(iox_ili_fixture_matrix_preserves_event_and_diagnostic_streams) {
    const std::vector<std::filesystem::path> roots = {
        fixtureRoot / "xtf23",
        fixtureRoot / "xtf24",
        fixtureRoot / "xtf24writer"
    };

    std::size_t fixtureCount = 0;
    for (const auto& root : roots) {
        for (const auto& path : iox::conformance::transferFixtures(root)) {
            ++fixtureCount;
            const auto oneShot = iox::conformance::parseFixture(path);
            const auto oneByte = iox::conformance::parseFixture(path, 1);
            const auto sevenBytes = iox::conformance::parseFixture(path, 7);
            const auto sixtyFourBytes = iox::conformance::parseFixture(path, 64);

            const auto terminal = [](const auto& parsed) {
                return parsed.ended || std::any_of(
                    parsed.diagnostics.begin(), parsed.diagnostics.end(),
                    [](const auto& diagnostic) {
                        return diagnostic.severity ==
                               iox::DiagnosticSeverity::Fatal;
                    });
            };
            IOX_CHECK(terminal(oneShot));
            IOX_CHECK(terminal(oneByte));
            IOX_CHECK(terminal(sevenBytes));
            IOX_CHECK(terminal(sixtyFourBytes));
            const auto oneShotEvents =
                iox::conformance::eventFingerprints(oneShot.events);
            const auto oneByteEvents =
                iox::conformance::eventFingerprints(oneByte.events);
            if (oneShotEvents != oneByteEvents) {
                std::cerr << "chunk mismatch: " << path << '\n';
            }
            IOX_CHECK_EQ(oneShotEvents, oneByteEvents);
            IOX_CHECK_EQ(iox::conformance::eventFingerprints(oneShot.events),
                         iox::conformance::eventFingerprints(sevenBytes.events));
            IOX_CHECK_EQ(iox::conformance::eventFingerprints(oneShot.events),
                         iox::conformance::eventFingerprints(sixtyFourBytes.events));
            const auto oneShotDiagnostics =
                iox::conformance::diagnosticContract(oneShot.diagnostics);
            const auto oneByteDiagnostics =
                iox::conformance::diagnosticContract(oneByte.diagnostics);
            IOX_CHECK_EQ(oneShotDiagnostics, oneByteDiagnostics);
            IOX_CHECK_EQ(oneShotDiagnostics,
                         iox::conformance::diagnosticContract(sevenBytes.diagnostics));
            IOX_CHECK_EQ(oneShotDiagnostics,
                         iox::conformance::diagnosticContract(sixtyFourBytes.diagnostics));
        }
    }
    IOX_CHECK_EQ(static_cast<std::size_t>(211), fixtureCount);
}

IOX_TEST(iox_ili_xtf23_reference_fixture_preserves_attributes_and_values) {
    const auto parsed = iox::conformance::parseFixture(
        fixtureRoot / "xtf23/dataSection/References.xtf");
    IOX_CHECK(parsed.ended);

    bool foundObject = false;
    bool foundReference = false;
    std::function<void(const iox::IomObject&)> inspect =
        [&](const iox::IomObject& object) {
            for (std::size_t index = 0; index < object.attributeCount(); ++index) {
                const auto& name = object.attributeName(index);
                const auto count = object.valueCount(name.interlisName());
                for (std::size_t valueIndex = 0; valueIndex < count;
                     ++valueIndex) {
                    const auto& value =
                        object.value(name.interlisName(), valueIndex);
                    if (!value.isObject()) continue;
                    if (value.object().isReference()) foundReference = true;
                    inspect(value.object());
                }
            }
        };
    for (const auto& event : parsed.events) {
        const auto* object = std::get_if<iox::ObjectEvent>(&event);
        if (object == nullptr) continue;
        foundObject = true;
        inspect(object->object);
    }
    IOX_CHECK(foundObject);
    IOX_CHECK(foundReference);
}

IOX_TEST(iox_ili_xtf24_writer_fixtures_have_semantic_roundtrip) {
    const auto root = fixtureRoot / "xtf24writer";
    const auto paths = iox::conformance::transferFixtures(root);
    IOX_CHECK_EQ(static_cast<std::size_t>(5), paths.size());

    for (const auto& path : paths) {
        const auto input = iox::conformance::parseFixture(path);
        const auto output = iox::conformance::writeEvents(
            input.events, iox::xtf::XtfVersion::V24);
        const auto roundtrip = iox::conformance::parseBytes(output);
        IOX_CHECK(input.ended);
        IOX_CHECK(roundtrip.ended);
        IOX_CHECK_EQ(iox::conformance::semanticEventFingerprints(input.events),
                     iox::conformance::semanticEventFingerprints(roundtrip.events));
    }
}

IOX_TEST(iox_ili_writer_preserves_ordered_repeated_and_reference_values) {
    const auto events = orderedReferenceEvents();
    const auto firstOutput = iox::conformance::writeEvents(
        events, iox::xtf::XtfVersion::V23);
    const auto secondOutput = iox::conformance::writeEvents(
        events, iox::xtf::XtfVersion::V23);
    IOX_CHECK_EQ(firstOutput, secondOutput);

    iox::xtf::XtfReaderOptions readerOptions;
    readerOptions.requireAtLeastOneModel = false;
    const auto parsed = iox::conformance::parseBytes(
        firstOutput, 0, std::move(readerOptions));
    IOX_CHECK(parsed.ended);
    const auto expected = iox::conformance::semanticEventFingerprints(events);
    const auto actual =
        iox::conformance::semanticEventFingerprints(parsed.events);
    if (expected != actual) {
        std::cerr << "ordered roundtrip mismatch\n";
        for (const auto& value : expected) std::cerr << "E " << value << '\n';
        for (const auto& value : actual) std::cerr << "A " << value << '\n';
    }
    IOX_CHECK_EQ(expected, actual);
}

IOX_TEST(iox_ili_clean_reader_corpus_has_semantic_writer_roundtrip) {
    const std::vector<std::filesystem::path> roots = {
        fixtureRoot / "xtf23", fixtureRoot / "xtf24",
        fixtureRoot / "xtf24writer"};
    std::size_t eligible = 0;
    std::size_t accepted = 0;
    std::size_t semanticMatches = 0;
    for (const auto& root : roots) {
        for (const auto& path : iox::conformance::transferFixtures(root)) {
            const auto input = iox::conformance::parseFixture(path);
            const auto hasError = std::any_of(
                input.diagnostics.begin(), input.diagnostics.end(),
                [](const auto& diagnostic) {
                    return diagnostic.severity == iox::DiagnosticSeverity::Error ||
                           diagnostic.severity == iox::DiagnosticSeverity::Fatal;
                });
            if (!input.ended || hasError || input.events.empty()) continue;
            ++eligible;
            try {
                const auto version =
                    std::get<iox::StartTransferEvent>(input.events.front())
                        .header.version;
                const auto output =
                    iox::conformance::writeEvents(input.events, version);
                ++accepted;
                iox::xtf::XtfReaderOptions options;
                options.requireAtLeastOneModel = false;
                const auto roundtrip =
                    iox::conformance::parseBytes(output, 0, options);
                if (roundtrip.ended &&
                    iox::conformance::semanticEventFingerprints(input.events) ==
                        iox::conformance::semanticEventFingerprints(
                            roundtrip.events)) {
                    ++semanticMatches;
                } else {
                    std::cerr << "semantic corpus roundtrip mismatch: "
                              << path << '\n';
                }
            } catch (const iox::IoxError& error) {
                std::cerr << "clean corpus writer rejection: " << path
                          << " (" << iox::diagnosticCodeName(error.code())
                          << ")\n";
            }
        }
    }
    IOX_CHECK(eligible > 100U);
    IOX_CHECK_EQ(eligible, accepted);
    IOX_CHECK_EQ(eligible, semanticMatches);
}

#include "iox/test/TestMain.h"
