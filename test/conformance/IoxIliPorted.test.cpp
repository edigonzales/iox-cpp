// Behavioral port coverage for the pinned iox-ili XTF test corpus.
// The Java source remains the provenance; assertions target the C++ event API.

#include "IoxIliTestSupport.h"
#include "iox/test/Test.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace {

const std::filesystem::path fixtureRoot = "test/fixtures";

std::vector<iox::IoxEvent> orderedReferenceEvents() {
    std::vector<iox::IoxEvent> events;
    iox::StartTransferEvent transfer;
    transfer.version = 23;
    transfer.iliVersion = "2.3";
    events.push_back(transfer);

    iox::StartBasketEvent basket;
    basket.basketType = iox::IomName("Matrix.Topic.Data");
    basket.bid = "B1";
    events.push_back(basket);

    iox::ObjectEvent object;
    object.operation = "insert";
    object.objectId = "T1";
    object.object = iox::IomObject(iox::IomName("Matrix.Topic.Class"));
    auto& first = object.object.setAttribute(iox::IomName("First"));
    first.values.push_back(iox::IomValue::text("one"));
    auto& repeated = object.object.setAttribute(iox::IomName("Repeated"));
    repeated.values.push_back(iox::IomValue::text("a"));
    repeated.values.push_back(iox::IomValue::text("b"));
    auto& reference = object.object.setAttribute(iox::IomName("Reference"));
    reference.ref = "T2";
    reference.orderPos = 2;
    reference.values.push_back(iox::IomValue::text("target"));
    events.push_back(object);

    events.push_back(iox::EndBasketEvent{"B1"});
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

            IOX_CHECK(oneShot.ended);
            IOX_CHECK(oneByte.ended);
            IOX_CHECK(sevenBytes.ended);
            IOX_CHECK(sixtyFourBytes.ended);
            IOX_CHECK_EQ(iox::conformance::eventFingerprints(oneShot.events),
                         iox::conformance::eventFingerprints(oneByte.events));
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
                const auto& attribute = object.attributeAt(index);
                if (attribute.ref) {
                    foundReference = true;
                }
                for (const auto& value : attribute.values) {
                    if (const auto* nested = std::get_if<iox::IomObject>(&value)) {
                        inspect(*nested);
                    }
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
            input.events, iox::xtf::XtfVersion::Xtf24);
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
        events, iox::xtf::XtfVersion::Xtf23);
    const auto secondOutput = iox::conformance::writeEvents(
        events, iox::xtf::XtfVersion::Xtf23);
    IOX_CHECK_EQ(firstOutput, secondOutput);

    const auto parsed = iox::conformance::parseBytes(firstOutput);
    IOX_CHECK(parsed.ended);
    IOX_CHECK_EQ(iox::conformance::semanticEventFingerprints(events),
                 iox::conformance::semanticEventFingerprints(parsed.events));
}

#include "iox/test/TestMain.h"
