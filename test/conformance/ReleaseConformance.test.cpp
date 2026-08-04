#include "IoxIliTestSupport.h"
#include "iox/test/Test.h"

#include <filesystem>
#include <string>
#include <vector>

namespace {

std::vector<iox::EventKind> kinds(const std::vector<iox::IoxEvent>& events) {
    std::vector<iox::EventKind> result;
    for (const auto& event : events) result.push_back(iox::eventKind(event));
    return result;
}

const iox::IomObject& onlyObject(
    const iox::conformance::ParsedFixture& parsed) {
    for (const auto& event : parsed.events) {
        if (const auto* object = std::get_if<iox::ObjectEvent>(&event)) {
            return object->object;
        }
    }
    iox::test::fail(__FILE__, __LINE__, "fixture has no object event");
    std::abort();
}

void expectFatal(const std::filesystem::path& path,
                 iox::DiagnosticCode code) {
    const auto parsed = iox::conformance::parseFixture(path);
    IOX_CHECK(!parsed.ended);
    IOX_CHECK_EQ(std::vector<std::string>{
                     "fatal:" + std::string(iox::diagnosticCodeName(code))},
                 iox::conformance::diagnosticContract(parsed.diagnostics));
}

} // namespace

IOX_TEST(release_positive_fixtures_have_independent_field_oracles) {
    const std::vector<iox::EventKind> objectTransfer{
        iox::EventKind::StartTransfer, iox::EventKind::StartBasket,
        iox::EventKind::Object, iox::EventKind::EndBasket,
        iox::EventKind::EndTransfer};

    const auto text23 = iox::conformance::parseFixture(
        "test/fixtures/xtf23/dataSection/TextTypes.xtf");
    IOX_CHECK(text23.ended);
    IOX_CHECK_EQ(objectTransfer, kinds(text23.events));
    const auto& text23Header =
        std::get<iox::StartTransferEvent>(text23.events[0]).header;
    IOX_CHECK_EQ(std::string("xtf23Reader"), text23Header.sender);
    IOX_CHECK_EQ(std::string("DataTest1"), text23Header.models[0].name);
    const auto& text23Basket =
        std::get<iox::StartBasketEvent>(text23.events[1]).basket;
    IOX_CHECK_EQ(std::string("DataTest1.TopicA"),
                 text23Basket.topic.interlisName());
    IOX_CHECK_EQ(std::string("bidA"), text23Basket.basketId);
    const auto& text23Object = onlyObject(text23);
    IOX_CHECK_EQ(std::string("DataTest1.TopicA.ClassA"),
                 text23Object.tag().interlisName());
    IOX_CHECK_EQ(std::string("oidA"), *text23Object.oid());
    IOX_CHECK_EQ(std::string("attrText"),
                 text23Object.attributeName(0).interlisName());
    IOX_CHECK_EQ(std::string("attrMText"),
                 text23Object.attributeName(1).interlisName());
    IOX_CHECK_EQ(std::string_view("\"normal text"),
                 *text23Object.primitive("attrText"));

    const auto numeric23 = iox::conformance::parseFixture(
        "test/fixtures/xtf23/dataSection/NumericTypes.xtf");
    IOX_CHECK(numeric23.ended);
    const auto& numericObject = onlyObject(numeric23);
    IOX_CHECK_EQ(std::string_view("6.15"),
                 *numericObject.primitive("attrNrDec"));

    const auto text24 = iox::conformance::parseFixture(
        "test/fixtures/xtf24/dataSection/TextTypes.xml");
    IOX_CHECK(text24.ended);
    IOX_CHECK_EQ(objectTransfer, kinds(text24.events));
    const auto& text24Header =
        std::get<iox::StartTransferEvent>(text24.events[0]).header;
    IOX_CHECK_EQ(iox::XtfVersion::V24, text24Header.version);
    IOX_CHECK_EQ(std::string("DataTest1"), text24Header.models[0].name);
    const auto& text24Object = onlyObject(text24);
    IOX_CHECK_EQ(std::string("DataTest1.TopicA.ClassA"),
                 text24Object.tag().interlisName());
    IOX_CHECK(text24Object.tag().hasXmlName());
    IOX_CHECK_EQ(std::string("http://www.interlis.ch/xtf/2.4/DataTest1"),
                 text24Object.tag().xmlName().namespaceUri);
    IOX_CHECK_EQ(std::string_view("normal text"),
                 *text24Object.primitive("attrText"));

    const auto association = iox::conformance::parseFixture(
        "test/fixtures/xtf24/associations/EmbeddedAssociationWithAttributes.xml");
    IOX_CHECK(association.ended);
    const auto& associationEvents = association.events;
    IOX_CHECK_EQ(static_cast<std::size_t>(6), associationEvents.size());
    const auto& owner =
        std::get<iox::ObjectEvent>(associationEvents[3]).object;
    const auto role = owner.object("rolle_A");
    IOX_CHECK(role.has_value());
    IOX_CHECK_EQ(std::string("oid1"), *role->reference().targetOid);
    IOX_CHECK_EQ(std::string_view("12"),
                 *role->primitive("attr_Assoc"));
}

IOX_TEST(release_negative_fixtures_have_stable_fatal_codes) {
    expectFatal("test/fixtures/xtf23/WrongCaseSensitiveTransferElement.xtf",
                iox::DiagnosticCode::InvalidXtfNamespace);
    expectFatal("test/fixtures/xtf23/WrongSpelledEndTransferElement.xtf",
                iox::DiagnosticCode::XmlMalformed);
    expectFatal("test/fixtures/xtf24/WrongTopEleNamespace.xml",
                iox::DiagnosticCode::InvalidXtfNamespace);
    expectFatal("test/fixtures/xtf24/NoDataSectionDefined.xml",
                iox::DiagnosticCode::InvalidEventOrder);
}

#include "iox/test/TestMain.h"
