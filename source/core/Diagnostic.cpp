#include "iox/Diagnostic.h"

#include <utility>

namespace iox {

std::string_view diagnosticCodeName(DiagnosticCode code) noexcept {
    switch (code) {
    case DiagnosticCode::XmlMalformed: return "xml.malformed";
    case DiagnosticCode::XmlDtdForbidden: return "xml.dtd_forbidden";
    case DiagnosticCode::XmlExternalEntityForbidden: return "xml.external_entity_forbidden";
    case DiagnosticCode::XmlLimitExceeded: return "xml.limit_exceeded";
    case DiagnosticCode::UnexpectedElement: return "xtf.unexpected_element";
    case DiagnosticCode::UnexpectedAttribute: return "xtf.unexpected_attribute";
    case DiagnosticCode::InvalidEventOrder: return "xtf.invalid_event_order";
    case DiagnosticCode::InvalidXtfNamespace: return "xtf.invalid_namespace";
    case DiagnosticCode::UnsupportedXtfVersion: return "xtf.unsupported_version";
    case DiagnosticCode::MissingRequiredHeader: return "xtf.missing_required_header";
    case DiagnosticCode::MissingModelEntry: return "xtf.missing_model_entry";
    case DiagnosticCode::MissingBasketId: return "xtf.missing_basket_id";
    case DiagnosticCode::MissingObjectId: return "xtf.missing_object_id";
    case DiagnosticCode::InvalidReference: return "xtf.invalid_reference";
    case DiagnosticCode::InvalidGeometry: return "xtf.invalid_geometry";
    case DiagnosticCode::UnknownInterlisName: return "ilic.unknown_name";
    case DiagnosticCode::UnknownExtensionPreserved: return "xtf.unknown_extension_preserved";
    case DiagnosticCode::ModelMismatch: return "ilic.model_mismatch";
    case DiagnosticCode::WriterStateError: return "writer.invalid_state";
    case DiagnosticCode::JsonMalformed: return "json.malformed";
    case DiagnosticCode::IoError: return "io.error";
    case DiagnosticCode::InvalidArgument: return "api.invalid_argument";
    case DiagnosticCode::InvalidState: return "api.invalid_state";
    case DiagnosticCode::BasketLimitExceeded: return "basket.limit_exceeded";
    case DiagnosticCode::BasketStateViolation: return "basket.state_violation";
    case DiagnosticCode::FormatUnknown: return "format.unknown";
    case DiagnosticCode::IomCycle: return "iom.cycle";
    case DiagnosticCode::AbiInvalidArgument: return "abi.invalid_argument";
    case DiagnosticCode::InternalError: return "internal.error";
    }
    return "internal.error";
}

void VectorDiagnosticSink::report(const Diagnostic& diagnostic) {
    diagnostics_.push_back(diagnostic);
}

const std::vector<Diagnostic>& VectorDiagnosticSink::diagnostics() const noexcept {
    return diagnostics_;
}

std::vector<Diagnostic> VectorDiagnosticSink::take() {
    auto result = std::move(diagnostics_);
    diagnostics_.clear();
    return result;
}

void VectorDiagnosticSink::clear() noexcept {
    diagnostics_.clear();
}

IoxError::IoxError(DiagnosticCode code,
                   std::string message,
                   SourceLocation location)
    : std::runtime_error(std::move(message)),
      code_(code),
      location_(std::move(location)) {}

DiagnosticCode IoxError::code() const noexcept {
    return code_;
}

const SourceLocation& IoxError::location() const noexcept {
    return location_;
}

} // namespace iox
