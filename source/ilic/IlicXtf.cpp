#include "iox/ilic/IlicModelIndex.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/Writer.h"

#include <string>
#include <utility>

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

    Impl(const metamodel::Model& model, IlicXtfReaderOptions opts)
        : index(model), options(std::move(opts)) {
        genericReader = std::make_unique<xtf::XtfReader>(options.xtf);
    }
};

IlicXtfReader::IlicXtfReader(const metamodel::Model& model, IlicXtfReaderOptions options)
    : impl_(std::make_unique<Impl>(model, std::move(options))) {}

IlicXtfReader::~IlicXtfReader() = default;

ReadOutcome IlicXtfReader::next() {
    auto outcome = impl_->genericReader->next();

    if (outcome.event) {
        std::visit([this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, ObjectEvent>) {
                auto* cls = impl_->index.findClass(e.object.tag().interlisName());
                if (!cls && impl_->options.rejectUnknownClasses) {
                    impl_->accumulated.push_back({
                        DiagnosticSeverity::Error,
                        DiagnosticCode::UnknownInterlisName,
                        "Unknown class: " + e.object.tag().interlisName(),
                        {}, {}});
                }
                if (cls && impl_->options.rejectUnknownProperties) {
                    for (std::size_t i = 0; i < e.object.attributeCount(); ++i) {
                        const auto& name = e.object.attributeName(i);
                        auto* prop = impl_->index.findProperty(
                            *cls, name.interlisName());
                        if (!prop) {
                            impl_->accumulated.push_back({
                                DiagnosticSeverity::Error,
                                DiagnosticCode::UnknownInterlisName,
                                "Unknown property '" + name.interlisName() +
                                "' on class " + e.object.tag().interlisName(),
                                {}, {}});
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
                if (impl_->options.rejectUnknownTopics) {
                    auto* topic = impl_->index.findTopic(
                        e.basket.topic.interlisName());
                    if (!topic) {
                        impl_->accumulated.push_back({
                            DiagnosticSeverity::Error,
                            DiagnosticCode::UnknownInterlisName,
                            "Unknown topic: " +
                                e.basket.topic.interlisName(), {}, {}});
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

    Impl(const metamodel::Model& model, std::shared_ptr<OutputSink> output,
         IlicXtfWriterOptions opts)
        : index(model), options(std::move(opts)) {
        genericWriter = std::make_unique<xtf::XtfWriter>(
            std::move(output), options.xtf);
    }
};

IlicXtfWriter::IlicXtfWriter(const metamodel::Model& model,
                             std::shared_ptr<OutputSink> output,
                             IlicXtfWriterOptions options)
    : impl_(std::make_unique<Impl>(model, std::move(output), std::move(options))) {}

IlicXtfWriter::~IlicXtfWriter() = default;

void IlicXtfWriter::write(const IoxEvent& event) {
    bool rejectEvent = false;
    std::visit([this, &rejectEvent](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, ObjectEvent>) {
            auto* cls = impl_->index.findClass(e.object.tag().interlisName());
            if (!cls && impl_->options.rejectUnknownClasses) {
                rejectEvent = true;
                impl_->accumulated.push_back({
                    DiagnosticSeverity::Error,
                    DiagnosticCode::UnknownInterlisName,
                    "Cannot write unknown class: " +
                        e.object.tag().interlisName(), {}, {}});
                return;
            }
            if (cls && impl_->options.rejectUnknownProperties) {
                for (std::size_t i = 0; i < e.object.attributeCount(); ++i) {
                    const auto& name = e.object.attributeName(i);
                    auto* prop = impl_->index.findProperty(
                        *cls, name.interlisName());
                    if (!prop) {
                        rejectEvent = true;
                        impl_->accumulated.push_back({
                            DiagnosticSeverity::Error,
                            DiagnosticCode::UnknownInterlisName,
                            "Unknown property '" + name.interlisName() +
                            "' on class " + e.object.tag().interlisName(),
                            {}, {}});
                    }
                }
            }
        }
    }, event);

    if (rejectEvent) return;

    if (const auto* objectEvent = std::get_if<ObjectEvent>(&event);
        objectEvent != nullptr && impl_->options.enforceTransferOrder) {
        const auto* klass = impl_->index.findClass(objectEvent->object.tag().interlisName());
        if (klass != nullptr) {
            IomObject ordered(objectEvent->object.tag(),
                              objectEvent->object.oid());
            ordered.setOperation(objectEvent->object.operation());
            ordered.setConsistency(objectEvent->object.consistency());
            ordered.setReference(objectEvent->object.reference());
            ordered.setSourceLocation(objectEvent->object.sourceLocation());

            const auto copyAttribute = [&objectEvent, &ordered](
                const IomName& name) {
                const auto count = objectEvent->object.valueCount(
                    name.interlisName());
                for (std::size_t index = 0; index < count; ++index) {
                    const auto& value = objectEvent->object.value(
                        name.interlisName(), index);
                    if (value.isPrimitive()) {
                        ordered.appendPrimitive(name, value.primitive());
                    } else {
                        ordered.appendObject(name, value.object());
                    }
                }
            };

            for (const auto* property : impl_->index.transferProperties(*klass)) {
                if (objectEvent->object.hasAttribute(property->Name)) {
                    for (std::size_t index = 0;
                         index < objectEvent->object.attributeCount(); ++index) {
                        const auto& name = objectEvent->object.attributeName(index);
                        if (name.interlisName() == property->Name) {
                            copyAttribute(name);
                            break;
                        }
                    }
                }
            }
            // Preserve unknown attributes in their original relative order.
            for (std::size_t i = 0; i < objectEvent->object.attributeCount(); ++i) {
                const auto& name = objectEvent->object.attributeName(i);
                if (impl_->index.findProperty(*klass,
                                              name.interlisName()) == nullptr) {
                    copyAttribute(name);
                }
            }

            ObjectEvent reordered = *objectEvent;
            reordered.object = std::move(ordered);
            impl_->genericWriter->write(reordered);
            return;
        }
    }

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
