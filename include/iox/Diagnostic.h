#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace iox {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
    Fatal
};

enum class DiagnosticCode {
    XmlMalformed,
    XmlDtdForbidden,
    XmlExternalEntityForbidden,
    XmlLimitExceeded,
    UnexpectedElement,
    UnexpectedAttribute,
    InvalidEventOrder,
    InvalidXtfNamespace,
    UnsupportedXtfVersion,
    MissingRequiredHeader,
    MissingModelEntry,
    MissingBasketId,
    MissingObjectId,
    InvalidReference,
    InvalidGeometry,
    UnknownInterlisName,
    UnknownExtensionPreserved,
    ModelMismatch,
    WriterStateError,
    JsonMalformed,
    IoError,
    InvalidArgument,
    InvalidState,
    BasketLimitExceeded,
    BasketStateViolation,
    FormatUnknown,
    IomCycle,
    AbiInvalidArgument,
    InternalError
};

std::string_view diagnosticCodeName(DiagnosticCode code) noexcept;

struct SourceLocation final {
    std::string sourceName;
    std::uint64_t byteOffset = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;

    bool empty() const noexcept {
        return sourceName.empty() && byteOffset == 0 && line == 0 && column == 0;
    }
};

struct Diagnostic final {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    DiagnosticCode code = DiagnosticCode::InternalError;
    std::string message;
    SourceLocation location;
    std::vector<std::string> contextPath;
};

class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual void report(const Diagnostic& diagnostic) = 0;
};

class VectorDiagnosticSink final : public DiagnosticSink {
public:
    void report(const Diagnostic& diagnostic) override;
    const std::vector<Diagnostic>& diagnostics() const noexcept;
    std::vector<Diagnostic> take();
    void clear() noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
};

class IoxError final : public std::runtime_error {
public:
    IoxError(DiagnosticCode code,
             std::string message,
             SourceLocation location = {});

    DiagnosticCode code() const noexcept;
    const SourceLocation& location() const noexcept;

private:
    DiagnosticCode code_;
    SourceLocation location_;
};

} // namespace iox
