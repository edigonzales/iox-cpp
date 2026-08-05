#include "iox/IomPath.h"
#include "iox/test/Test.h"

namespace {

iox::IomObject sampleObject() {
    iox::IomObject document(iox::IomName("Document"));
    document.setPrimitive(iox::IomName("publicationDate"), "2026-08-05");

    iox::IomObject first(iox::IomName("File"));
    first.setPrimitive(iox::IomName("fileName"), "first.xtf");
    iox::IomObject second(iox::IomName("File"));
    second.setPrimitive(iox::IomName("fileName"), "second.xtf");
    document.appendObject(iox::IomName("documents"), first);
    document.appendObject(iox::IomName("documents"), second);
    return document;
}

template<typename Operation>
bool throwsCode(const Operation& operation, iox::DiagnosticCode code) {
    try {
        operation();
    } catch (const iox::IoxError& error) {
        return error.code() == code;
    }
    return false;
}

} // namespace

IOX_TEST(iom_path_parses_steps_and_selectors) {
    const auto path = iox::IomPath::parse("documents[2].fileName");
    IOX_CHECK_EQ(std::string("documents[2].fileName"), path.expression());
    IOX_CHECK_EQ(static_cast<std::size_t>(2), path.steps().size());
    IOX_CHECK_EQ(std::string("documents"), path.steps()[0].attribute);
    IOX_CHECK(path.steps()[0].selector == iox::IomPathSelectorKind::Index);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), path.steps()[0].index);
    IOX_CHECK(path.steps()[1].selector == iox::IomPathSelectorKind::First);
    IOX_CHECK(!path.containsWildcard());
}

IOX_TEST(iom_path_reads_simple_nested_index_and_wildcard_values) {
    const auto object = sampleObject();
    const auto simple = iox::IomPath::parse("publicationDate").primitiveMatches(object);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), simple.size());
    IOX_CHECK_EQ(std::string("2026-08-05"), simple[0].value);

    const auto indexed = iox::IomPath::parse("documents[2].fileName")
                             .primitiveMatches(object);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), indexed.size());
    IOX_CHECK_EQ(std::string("second.xtf"), indexed[0].value);
    IOX_CHECK_EQ(static_cast<std::size_t>(2), indexed[0].valueIndexes.size());
    IOX_CHECK_EQ(static_cast<std::size_t>(1), indexed[0].valueIndexes[0]);

    const auto wildcard = iox::IomPath::parse("documents[*].fileName");
    const auto matches = wildcard.primitiveMatches(object);
    IOX_CHECK(wildcard.containsWildcard());
    IOX_CHECK_EQ(static_cast<std::size_t>(2), matches.size());
    IOX_CHECK_EQ(std::string("first.xtf"), matches[0].value);
    IOX_CHECK_EQ(std::string("second.xtf"), matches[1].value);
}

IOX_TEST(iom_path_replaces_nested_value_and_preserves_cow_parent) {
    auto original = sampleObject();
    auto copy = original;
    const auto path = iox::IomPath::parse("documents[2].fileName");
    const auto old = path.replaceSinglePrimitive(copy, "changed.xtf", "second.xtf");
    IOX_CHECK_EQ(std::string("second.xtf"), old);
    IOX_CHECK_EQ(std::string("second.xtf"),
                 original.object("documents", 1)->primitive("fileName").value());
    IOX_CHECK_EQ(std::string("changed.xtf"),
                 copy.object("documents", 1)->primitive("fileName").value());

    const auto simple = iox::IomPath::parse("publicationDate");
    IOX_CHECK_EQ(std::string("2026-08-05"),
                 simple.replaceSinglePrimitive(copy, "2026-08-06"));
    IOX_CHECK_EQ(std::string("2026-08-06"),
                 copy.primitive("publicationDate").value());
}

IOX_TEST(iom_path_reports_invalid_targets_and_expected_conflicts) {
    auto object = sampleObject();
    IOX_CHECK(throwsCode(
        [&] { (void)iox::IomPath::parse("missing").primitiveMatches(object); },
        iox::DiagnosticCode::UnknownInterlisName));
    IOX_CHECK(throwsCode(
        [&] { (void)iox::IomPath::parse("publicationDate.value").primitiveMatches(object); },
        iox::DiagnosticCode::InvalidState));
    IOX_CHECK(throwsCode(
        [&] { (void)iox::IomPath::parse("publicationDate[2]").primitiveMatches(object); },
        iox::DiagnosticCode::InvalidArgument));
    IOX_CHECK(throwsCode(
        [&] {
            (void)iox::IomPath::parse("documents[*].fileName")
                .replaceSinglePrimitive(object, "all");
        },
        iox::DiagnosticCode::InvalidArgument));
    IOX_CHECK(throwsCode(
        [&] {
            (void)iox::IomPath::parse("documents[1].fileName")
                .replaceSinglePrimitive(object, "changed", "wrong");
        },
        iox::DiagnosticCode::ModelMismatch));
}

IOX_TEST(iom_path_rejects_invalid_syntax) {
    for (const auto expression : {"", ".value", "value.", "a..b", "a[0]",
                                  "a[-1]", "a[]", "a[ *]", "@tid",
                                  "owner.@ref", "value[x]"}) {
        IOX_CHECK(throwsCode(
            [&] { (void)iox::IomPath::parse(expression); },
            iox::DiagnosticCode::InvalidArgument));
    }
}

#include "iox/test/TestMain.h"
