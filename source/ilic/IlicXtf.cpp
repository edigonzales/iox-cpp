#include "iox/ilic/IlicModelIndex.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/xtf/XtfReaderOptions.h"
#include "iox/Writer.h"

#include <string>

namespace iox {
namespace ilic {

// ============================================================================
// IlicXtfReader
// ============================================================================

struct IlicXtfReader::Impl {
    IlicModelIndex index;
    IlicXtfReaderOptions options;
    std::unique_ptr<xtf::XtfReader> genericReader;
    std::vector<Diagnostic> accumulated;

    Impl(const ModelDef& model, IlicXtfReaderOptions opts)
        : index(model), options(std::move(opts)) {
        xtf::XtfReaderOptions ropts;
        ropts.strict = options.strict;
        genericReader = std::make_unique<xtf::XtfReader>(ropts);
    }
};

IlicXtfReader::IlicXtfReader(const ModelDef& model, IlicXtfReaderOptions options)
    : impl_(std::make_unique<Impl>(model, std::move(options))) {}

IlicXtfReader::~IlicXtfReader() = default;

ReadOutcome IlicXtfReader::next() {
    auto outcome = impl_->genericReader->next();

    if (outcome.event) {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, ObjectEvent>) {
                auto* cls = impl_->index.findClass(e.object.tag().iliName());
                if (!cls && impl_->options.rejectUnknownClasses) {
                    impl_->accumulated.push_back({
                        Diagnostic::Severity::Error,
                        "ilic.unknown_class",
                        "Unknown class: " + e.object.tag().iliName()});
                }
                if (cls && impl_->options.rejectUnknownProperties) {
                    for (std::size_t i = 0; i < e.object.attributeCount(); ++i) {
                        const auto& attr = e.object.attributeAt(i);
                        auto* prop = cls->findProperty(attr.name.iliName());
                        if (!prop) {
                            impl_->accumulated.push_back({
                                Diagnostic::Severity::Error,
                                "ilic.unknown_property",
                                "Unknown property '" + attr.name.iliName() +
                                "' on class " + e.object.tag().iliName()});
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
                if (impl_->options.rejectUnknownTopics) {
                    auto* topic = impl_->index.findTopic(e.basketType.iliName());
                    if (!topic) {
                        impl_->accumulated.push_back({
                            Diagnostic::Severity::Error,
                            "ilic.unknown_topic",
                            "Unknown topic: " + e.basketType.iliName()});
                    }
                }
            }
        }, *outcome.event);
    }

    return outcome;
}

void IlicXtfReader::feed(ByteView data) { impl_->genericReader->feed(data); }
void IlicXtfReader::finish() { impl_->genericReader->finish(); }
bool IlicXtfReader::isFinished() const noexcept { return impl_->genericReader->isFinished(); }

std::vector<Diagnostic> IlicXtfReader::takeDiagnostics() {
    auto d = impl_->genericReader->takeDiagnostics();
    d.insert(d.end(), impl_->accumulated.begin(), impl_->accumulated.end());
    impl_->accumulated.clear();
    return d;
}

// ============================================================================
// IlicXtfWriter
// ============================================================================

struct IlicXtfWriter::Impl {
    IlicModelIndex index;
    IlicXtfWriterOptions options;
    std::unique_ptr<xtf::XtfWriter> genericWriter;
    std::vector<Diagnostic> accumulated;

    Impl(const ModelDef& model, std::shared_ptr<OutputSink> output,
         IlicXtfWriterOptions opts)
        : index(model), options(std::move(opts)) {
        xtf::XtfWriterOptions wopts;
        wopts.strict = options.strict;
        genericWriter = std::make_unique<xtf::XtfWriter>(std::move(output), wopts);
    }
};

IlicXtfWriter::IlicXtfWriter(const ModelDef& model,
                             std::shared_ptr<OutputSink> output,
                             IlicXtfWriterOptions options)
    : impl_(std::make_unique<Impl>(model, std::move(output), std::move(options))) {}

IlicXtfWriter::~IlicXtfWriter() = default;

void IlicXtfWriter::write(const IoxEvent& event) {
    std::visit([this](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, ObjectEvent>) {
            auto* cls = impl_->index.findClass(e.object.tag().iliName());
            if (!cls && impl_->options.rejectUnknownClasses) {
                impl_->accumulated.push_back({
                    Diagnostic::Severity::Error,
                    "ilic.unknown_class",
                    "Cannot write unknown class: " + e.object.tag().iliName()});
                return;
            }
            if (cls && impl_->options.rejectUnknownProperties) {
                for (std::size_t i = 0; i < e.object.attributeCount(); ++i) {
                    const auto& attr = e.object.attributeAt(i);
                    auto* prop = cls->findProperty(attr.name.iliName());
                    if (!prop) {
                        impl_->accumulated.push_back({
                            Diagnostic::Severity::Error,
                            "ilic.unknown_property",
                            "Unknown property '" + attr.name.iliName() +
                            "' on class " + e.object.tag().iliName()});
                    }
                }
            }
        }
    }, event);

    impl_->genericWriter->write(event);
}

void IlicXtfWriter::flush() { impl_->genericWriter->flush(); }
void IlicXtfWriter::close() { impl_->genericWriter->close(); }
bool IlicXtfWriter::isClosed() const noexcept { return impl_->genericWriter->isClosed(); }

std::vector<Diagnostic> IlicXtfWriter::takeDiagnostics() {
    auto d = impl_->genericWriter->takeDiagnostics();
    d.insert(d.end(), impl_->accumulated.begin(), impl_->accumulated.end());
    impl_->accumulated.clear();
    return d;
}

} // namespace ilic
} // namespace iox
