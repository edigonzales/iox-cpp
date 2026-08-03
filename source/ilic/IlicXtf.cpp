#include "iox/ilic/IlicModelIndex.h"

#include "iox/Writer.h"
#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace iox {
namespace ilic {
namespace {

std::string nameText(const IomName& name) {
    if (name.hasInterlisName()) return name.interlisName();
    if (name.hasXmlName()) return name.xmlName().expanded();
    return "<unnamed>";
}

std::string attributeKey(const IomName& name) {
    if (name.hasInterlisName()) return name.interlisName();
    return name.hasXmlName() ? name.xmlName().expanded() : std::string{};
}

Diagnostic unknownDiagnostic(DiagnosticSeverity severity,
                             std::string message,
                             const SourceLocation& location) {
    return {severity, DiagnosticCode::UnknownInterlisName,
            std::move(message), location, {}};
}

std::string selectHeaderModel(const IlicModelIndex& index,
                              const TransferHeader& header) {
    std::string selected;
    std::string language;
    for (const auto& model : header.models) {
        const auto candidateLanguage = index.modelLanguage(model.name);
        if (!candidateLanguage) continue;
        if (selected.empty()) {
            selected = model.name;
            language = *candidateLanguage;
            continue;
        }
        if (*candidateLanguage != language) {
            throw IoxError(
                DiagnosticCode::ModelMismatch,
                "Transfer header selects ilic models in different languages");
        }
    }
    return selected;
}

void enrichHeaderModels(const IlicModelIndex& index,
                        TransferHeader& header) {
    for (auto& model : header.models) {
        const auto indexed = index.transferModel(model.name, header.version);
        if (!indexed) continue;
        model = *indexed;
    }
}

struct TransformOptions final {
    bool rejectUnknownTopics = false;
    bool rejectUnknownClasses = false;
    bool rejectUnknownProperties = false;
    bool enforceTransferOrder = false;
    bool writer = false;
};

class EventTransformer final {
public:
    EventTransformer(const IlicModelIndex& index,
                     std::vector<Diagnostic>& diagnostics,
                     TransformOptions options)
        : index_(index), diagnostics_(diagnostics), options_(options) {}

    IoxEvent transform(IoxEvent event, XtfVersion configuredVersion) {
        if (auto* start = std::get_if<StartTransferEvent>(&event)) {
            version_ = options_.writer ? configuredVersion
                                       : start->header.version;
            selectedModel_ = selectHeaderModel(index_, start->header);
            enrichHeaderModels(index_, start->header);
            start->header.version = version_;
            return event;
        }
        if (auto* basket = std::get_if<StartBasketEvent>(&event)) {
            const auto resolved = index_.resolveTopic(
                basket->basket.topic, selectedModel_, version_);
            if (resolved) {
                basket->basket.topic = *resolved;
            } else {
                reportOrThrow("Unknown topic: " +
                                  nameText(basket->basket.topic),
                              basket->basket.location,
                              options_.writer ||
                                  options_.rejectUnknownTopics);
            }
            return event;
        }
        if (auto* object = std::get_if<ObjectEvent>(&event)) {
            object->object = transformObject(object->object, true);
        }
        return event;
    }

private:
    IomObject transformObject(const IomObject& source, bool topLevel) {
        const auto sourceClass = source.tag();
        const auto resolvedClass = index_.resolveClass(
            sourceClass, selectedModel_, version_);
        if (!resolvedClass) {
            // Canonical geometry and reference helper objects (for example
            // COORD and OID) are IOM implementation values, not model
            // classes. Keep such nested values opaque to the ilic layer.
            if (!topLevel) return source;
            reportOrThrow("Unknown class: " + nameText(sourceClass),
                          source.sourceLocation(),
                          options_.rejectUnknownClasses);
            return source;
        }
        if (topLevel && !index_.isTopLevelTransferable(sourceClass)) {
            reportOrThrow("Class is not a top-level transferable: " +
                              nameText(sourceClass),
                          source.sourceLocation(), options_.writer);
        }

        IomObject result(*resolvedClass, source.oid());
        result.setOperation(source.operation());
        result.setConsistency(source.consistency());
        result.setReference(source.reference());
        result.setSourceLocation(source.sourceLocation());

        struct Attribute final {
            IomName sourceName;
            IomName targetName;
            std::vector<IomValue> values;
            bool known = false;
        };
        std::vector<Attribute> attributes;
        attributes.reserve(source.attributeCount());
        for (std::size_t attributeIndex = 0;
             attributeIndex < source.attributeCount(); ++attributeIndex) {
            const auto& sourceName = source.attributeName(attributeIndex);
            Attribute attribute{sourceName, sourceName, {}, false};
            const auto resolvedProperty = index_.resolveProperty(
                sourceClass, sourceName, selectedModel_, version_);
            if (!resolvedProperty) {
                reportOrThrow(
                    "Unknown property '" + nameText(sourceName) +
                        "' on class " + nameText(sourceClass),
                    source.sourceLocation(),
                    options_.rejectUnknownProperties);
            } else {
                attribute.known = true;
                attribute.targetName = *resolvedProperty;
                if (index_.isTransientProperty(sourceClass, sourceName)) {
                    reportOrThrow(
                        "Transient property cannot appear in a transfer: " +
                            nameText(sourceName),
                        source.sourceLocation(), options_.writer);
                    if (options_.writer) continue;
                }
            }

            const auto key = attributeKey(sourceName);
            const auto valueCount = source.valueCount(key);
            attribute.values.reserve(valueCount);
            for (std::size_t valueIndex = 0; valueIndex < valueCount;
                 ++valueIndex) {
                const auto& value = source.value(key, valueIndex);
                if (value.isObject()) {
                    attribute.values.push_back(IomValue::object(
                        transformObject(value.object(), false)));
                    continue;
                }
                std::string lexical = value.primitive();
                if (attribute.known) {
                    const auto translated = index_.translateEnumeration(
                        sourceClass, sourceName, lexical, selectedModel_);
                    if (translated) {
                        lexical = *translated;
                    } else {
                        reportOrThrow(
                            "Unknown enumeration value '" + lexical +
                                "' for property " + nameText(sourceName),
                            source.sourceLocation(), options_.writer);
                    }
                }
                attribute.values.push_back(
                    IomValue::primitive(std::move(lexical)));
            }
            attributes.push_back(std::move(attribute));
        }

        if (options_.enforceTransferOrder) {
            const auto order = index_.transferProperties(
                sourceClass, selectedModel_, version_);
            std::stable_sort(
                attributes.begin(), attributes.end(),
                [&](const Attribute& left, const Attribute& right) {
                    const auto position = [&](const Attribute& attribute) {
                        const auto found = std::find_if(
                            order.begin(), order.end(), [&](const IomName& item) {
                                return item == attribute.targetName;
                            });
                        return found == order.end()
                                   ? order.size()
                                   : static_cast<std::size_t>(
                                         std::distance(order.begin(), found));
                    };
                    return position(left) < position(right);
                });
        }

        for (auto& attribute : attributes) {
            for (auto& value : attribute.values) {
                if (value.isPrimitive()) {
                    result.appendPrimitive(attribute.targetName,
                                           value.primitive());
                } else {
                    result.appendObject(attribute.targetName,
                                        value.object());
                }
            }
        }
        return result;
    }

    void reportOrThrow(std::string message,
                       const SourceLocation& location, bool reject) {
        diagnostics_.push_back(unknownDiagnostic(
            reject ? DiagnosticSeverity::Error
                   : DiagnosticSeverity::Warning,
            message, location));
        if (options_.writer && reject) {
            throw IoxError(DiagnosticCode::UnknownInterlisName,
                           std::move(message), location);
        }
    }

    const IlicModelIndex& index_;
    std::vector<Diagnostic>& diagnostics_;
    TransformOptions options_;
    std::string selectedModel_;
    XtfVersion version_ = XtfVersion::V23;
};

} // namespace

struct IlicXtfReader::Impl final {
    IlicModelIndex index;
    IlicXtfReaderOptions options;
    std::unique_ptr<xtf::XtfReader> genericReader;
    std::vector<Diagnostic> diagnostics;
    EventTransformer transformer;

    Impl(const metamodel::MetaModelStore& models,
         IlicXtfReaderOptions value)
        : index(models), options(std::move(value)),
          genericReader(std::make_unique<xtf::XtfReader>(options.xtf)),
          transformer(index, diagnostics,
                      {options.rejectUnknownTopics,
                       options.rejectUnknownClasses,
                       options.rejectUnknownProperties, false, false}) {}
};

IlicXtfReader::IlicXtfReader(const metamodel::MetaModelStore& models,
                             IlicXtfReaderOptions options)
    : impl_(std::make_unique<Impl>(models, std::move(options))) {}

IlicXtfReader::~IlicXtfReader() = default;

ReadOutcome IlicXtfReader::next() {
    auto outcome = impl_->genericReader->next();
    if (outcome.event) {
        outcome.event = impl_->transformer.transform(
            std::move(*outcome.event), XtfVersion::V23);
    }
    return outcome;
}

void IlicXtfReader::feed(ByteView data) {
    impl_->genericReader->feed(data);
}

void IlicXtfReader::finish() {
    impl_->genericReader->finish();
}

bool IlicXtfReader::isFinished() const noexcept {
    return impl_->genericReader->isFinished();
}

std::vector<Diagnostic> IlicXtfReader::takeDiagnostics() {
    auto result = impl_->genericReader->takeDiagnostics();
    result.insert(result.end(), impl_->diagnostics.begin(),
                  impl_->diagnostics.end());
    impl_->diagnostics.clear();
    return result;
}

struct IlicXtfWriter::Impl final {
    IlicModelIndex index;
    IlicXtfWriterOptions options;
    std::unique_ptr<xtf::XtfWriter> genericWriter;
    std::vector<Diagnostic> diagnostics;
    EventTransformer transformer;
    bool failed = false;

    Impl(const metamodel::MetaModelStore& models,
         std::shared_ptr<OutputSink> output, IlicXtfWriterOptions value)
        : index(models), options(std::move(value)),
          genericWriter(std::make_unique<xtf::XtfWriter>(
              std::move(output), options.xtf)),
          transformer(index, diagnostics,
                      {true, options.rejectUnknownClasses,
                       options.rejectUnknownProperties,
                       options.enforceTransferOrder, true}) {}

    void ensureUsable() const {
        if (failed) {
            throw IoxError(DiagnosticCode::WriterStateError,
                           "ilic XTF writer is in a failed state");
        }
    }
};

IlicXtfWriter::IlicXtfWriter(const metamodel::MetaModelStore& models,
                             std::shared_ptr<OutputSink> output,
                             IlicXtfWriterOptions options)
    : impl_(std::make_unique<Impl>(models, std::move(output),
                                   std::move(options))) {}

IlicXtfWriter::~IlicXtfWriter() = default;

void IlicXtfWriter::write(const IoxEvent& event) {
    impl_->ensureUsable();
    try {
        auto transformed = impl_->transformer.transform(
            event, impl_->options.xtf.version);
        impl_->genericWriter->write(transformed);
    } catch (...) {
        impl_->failed = true;
        throw;
    }
}

void IlicXtfWriter::flush() {
    impl_->ensureUsable();
    try {
        impl_->genericWriter->flush();
    } catch (...) {
        impl_->failed = true;
        throw;
    }
}

void IlicXtfWriter::close() {
    impl_->ensureUsable();
    try {
        impl_->genericWriter->close();
    } catch (...) {
        impl_->failed = true;
        throw;
    }
}

bool IlicXtfWriter::isClosed() const noexcept {
    return !impl_->failed && impl_->genericWriter->isClosed();
}

std::vector<Diagnostic> IlicXtfWriter::takeDiagnostics() {
    auto result = impl_->genericWriter->takeDiagnostics();
    result.insert(result.end(), impl_->diagnostics.begin(),
                  impl_->diagnostics.end());
    impl_->diagnostics.clear();
    return result;
}

} // namespace ilic
} // namespace iox
