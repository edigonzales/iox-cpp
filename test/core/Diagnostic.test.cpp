#include "iox/Diagnostic.h"
#include "iox/test/Test.h"

#include <array>
#include <string>

IOX_TEST(diagnostic_codes_have_stable_nonempty_names) {
    constexpr std::array codes{
        iox::DiagnosticCode::XmlMalformed,
        iox::DiagnosticCode::XmlDtdForbidden,
        iox::DiagnosticCode::XmlExternalEntityForbidden,
        iox::DiagnosticCode::XmlLimitExceeded,
        iox::DiagnosticCode::UnexpectedElement,
        iox::DiagnosticCode::UnexpectedAttribute,
        iox::DiagnosticCode::InvalidEventOrder,
        iox::DiagnosticCode::InvalidXtfNamespace,
        iox::DiagnosticCode::UnsupportedXtfVersion,
        iox::DiagnosticCode::MissingRequiredHeader,
        iox::DiagnosticCode::MissingModelEntry,
        iox::DiagnosticCode::MissingBasketId,
        iox::DiagnosticCode::MissingObjectId,
        iox::DiagnosticCode::InvalidReference,
        iox::DiagnosticCode::InvalidGeometry,
        iox::DiagnosticCode::UnknownInterlisName,
        iox::DiagnosticCode::UnknownExtensionPreserved,
        iox::DiagnosticCode::ModelMismatch,
        iox::DiagnosticCode::WriterStateError,
        iox::DiagnosticCode::JsonMalformed,
        iox::DiagnosticCode::IoError,
        iox::DiagnosticCode::InvalidArgument,
        iox::DiagnosticCode::InvalidState,
        iox::DiagnosticCode::BasketLimitExceeded,
        iox::DiagnosticCode::BasketStateViolation,
        iox::DiagnosticCode::FormatUnknown,
        iox::DiagnosticCode::IomCycle,
        iox::DiagnosticCode::AbiInvalidArgument,
        iox::DiagnosticCode::InternalError};
    for (const auto code : codes) {
        IOX_CHECK(!iox::diagnosticCodeName(code).empty());
    }
    IOX_CHECK_EQ(std::string_view("internal.error"),
                 iox::diagnosticCodeName(
                     static_cast<iox::DiagnosticCode>(999)));
}

IOX_TEST(diagnostic_sink_and_error_preserve_structured_data) {
    iox::VectorDiagnosticSink sink;
    iox::Diagnostic first{iox::DiagnosticSeverity::Warning,
                          iox::DiagnosticCode::UnexpectedElement,
                          "message", {"source", 3U, 2U, 1U}, {"M", "T"}};
    sink.report(first);
    IOX_CHECK_EQ(static_cast<std::size_t>(1), sink.diagnostics().size());
    const auto taken = sink.take();
    IOX_CHECK_EQ(static_cast<std::size_t>(1), taken.size());
    IOX_CHECK(sink.diagnostics().empty());
    sink.report(first);
    sink.clear();
    IOX_CHECK(sink.diagnostics().empty());

    iox::IoxError error(iox::DiagnosticCode::XmlMalformed, "broken",
                        {"input.xtf", 9U, 4U, 2U});
    IOX_CHECK_EQ(iox::DiagnosticCode::XmlMalformed, error.code());
    IOX_CHECK_EQ(std::string("broken"), std::string(error.what()));
    IOX_CHECK_EQ(std::string("input.xtf"), error.location().sourceName);
    IOX_CHECK_EQ(static_cast<std::size_t>(9), error.location().byteOffset);
}

#include "iox/test/TestMain.h"
