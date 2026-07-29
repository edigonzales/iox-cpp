#pragma once

// Direct integration with the concrete metamodel types supplied by
// ilic-core. This header is only available when IOX_ENABLE_ILIC is enabled.

#include "metamodel/MetaModel.h"
#include "iox/Reader.h"
#include "iox/Writer.h"
#include "iox/Events.h"
#include "iox/xtf/XtfReaderOptions.h"

#include <memory>
#include <string_view>
#include <vector>

namespace iox {
namespace ilic {

class IlicModelIndex final {
public:
    explicit IlicModelIndex(const metamodel::Model& model);
    ~IlicModelIndex();

    IlicModelIndex(const IlicModelIndex&) = delete;
    IlicModelIndex& operator=(const IlicModelIndex&) = delete;

    const metamodel::SubModel* findTopic(std::string_view scopedName) const;
    const metamodel::Class* findClass(std::string_view scopedName) const;
    const metamodel::AttrOrParam* findProperty(
        const metamodel::Class& owner, std::string_view propertyName) const;
    std::vector<const metamodel::AttrOrParam*> transferProperties(
        const metamodel::Class& owner) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct IlicXtfReaderOptions final {
    xtf::XtfReaderOptions xtf;
    bool rejectUnknownTopics = false;
    bool rejectUnknownClasses = false;
    bool rejectUnknownProperties = false;
};

/// Model-aware XTF reader composed around the generic model-free reader.
class IlicXtfReader final : public Reader {
public:
    IlicXtfReader(const metamodel::Model& model,
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

struct IlicXtfWriterOptions final {
    xtf::XtfWriterOptions xtf;
    bool enforceTransferOrder = true;
    bool rejectUnknownClasses = true;
    bool rejectUnknownProperties = true;
};

/// Model-aware XTF writer composed around the generic model-free writer.
class IlicXtfWriter final : public Writer {
public:
    IlicXtfWriter(const metamodel::Model& model,
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
