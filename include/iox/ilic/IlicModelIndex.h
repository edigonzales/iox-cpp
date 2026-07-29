#pragma once

// ============================================================================
// iox-ilic — Direct model-aware XTF processing
// ============================================================================
//
// Uses concrete ModelDef types. No abstract provider framework.

#include "iox/ilic/ModelDef.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/Events.h"

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <memory>

namespace iox {
namespace ilic {

// ============================================================================
// IlicModelIndex
// ============================================================================

class IlicModelIndex final {
public:
    explicit IlicModelIndex(const ModelDef& model);
    ~IlicModelIndex();

    IlicModelIndex(const IlicModelIndex&) = delete;
    IlicModelIndex& operator=(const IlicModelIndex&) = delete;

    const TopicDef* findTopic(std::string_view scopedName) const;
    const ClassDef* findClass(std::string_view scopedName) const;
    const PropertyDef* findProperty(const ClassDef& owner,
                                    std::string_view propertyName) const;
    std::vector<const PropertyDef*> transferProperties(
        const ClassDef& owner) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// IlicXtfReader
// ============================================================================

struct IlicXtfReaderOptions final {
    bool strict = false;
    bool rejectUnknownTopics = false;
    bool rejectUnknownClasses = false;
    bool rejectUnknownProperties = false;
};

/// Model-aware XTF reader. Wraps a generic XtfReader and enriches
/// events with model information (validated names, transfer order).
class IlicXtfReader final : public Reader {
public:
    IlicXtfReader(const ModelDef& model,
                  IlicXtfReaderOptions options = {});
    ~IlicXtfReader() override;

    ReadOutcome next() override;
    void feed(ByteView data) override;
    void finish() override;
    bool isFinished() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// IlicXtfWriter
// ============================================================================

struct IlicXtfWriterOptions final {
    bool strict = false;
    bool enforceTransferOrder = true;
    bool rejectUnknownClasses = true;
    bool rejectUnknownProperties = true;
};

/// Model-aware XTF writer. Validates events against the model and
/// enforces transfer order.
class IlicXtfWriter final : public Writer {
public:
    IlicXtfWriter(const ModelDef& model,
                  std::shared_ptr<OutputSink> output,
                  IlicXtfWriterOptions options = {});
    ~IlicXtfWriter() override;

    void write(const IoxEvent& event) override;
    void flush() override;
    void close() override;
    bool isClosed() const noexcept override;
    std::vector<Diagnostic> takeDiagnostics() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ilic
} // namespace iox
