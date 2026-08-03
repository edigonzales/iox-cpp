#include "iox/json/JsonEventWriter.h"

#include "iox/Diagnostic.h"

#include <yyjson.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace iox {
namespace json {
namespace {

using JsonDoc = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>;

void addString(yyjson_mut_doc* doc, yyjson_mut_val* object,
               const char* key, const std::string& value) {
    if (!yyjson_mut_obj_add_strcpy(doc, object, key, value.c_str())) {
        throw IoxError(DiagnosticCode::InternalError,
                       "Unable to allocate JSON string");
    }
}

void addOptionalString(yyjson_mut_doc* doc, yyjson_mut_val* object,
                       const char* key,
                       const std::optional<std::string>& value) {
    if (value) addString(doc, object, key, *value);
}

yyjson_mut_val* makeQName(yyjson_mut_doc* doc,
                          const XmlQualifiedName& name) {
    auto* object = yyjson_mut_obj(doc);
    addString(doc, object, "namespaceUri", name.namespaceUri);
    addString(doc, object, "localName", name.localName);
    addString(doc, object, "prefixHint", name.prefixHint);
    return object;
}

yyjson_mut_val* makeName(yyjson_mut_doc* doc, const IomName& name) {
    auto* object = yyjson_mut_obj(doc);
    addString(doc, object, "interlisName", name.interlisName());
    if (name.hasXmlName()) {
        yyjson_mut_obj_add_val(doc, object, "xml", makeQName(doc, name.xmlName()));
    } else {
        yyjson_mut_obj_add_null(doc, object, "xml");
    }
    return object;
}

yyjson_mut_val* makeLocation(yyjson_mut_doc* doc,
                             const SourceLocation& location) {
    auto* object = yyjson_mut_obj(doc);
    addString(doc, object, "sourceName", location.sourceName);
    yyjson_mut_obj_add_uint(doc, object, "byteOffset", location.byteOffset);
    yyjson_mut_obj_add_uint(doc, object, "line", location.line);
    yyjson_mut_obj_add_uint(doc, object, "column", location.column);
    return object;
}

const char* operationName(ObjectOperation operation) noexcept {
    switch (operation) {
    case ObjectOperation::Insert: return "insert";
    case ObjectOperation::Update: return "update";
    case ObjectOperation::Delete: return "delete";
    case ObjectOperation::None: return "none";
    }
    return "none";
}

const char* consistencyName(Consistency consistency) noexcept {
    switch (consistency) {
    case Consistency::Complete: return "complete";
    case Consistency::Incomplete: return "incomplete";
    case Consistency::Inconsistent: return "inconsistent";
    case Consistency::Adapted: return "adapted";
    case Consistency::Unspecified: return "unspecified";
    }
    return "unspecified";
}

const char* basketKindName(BasketKind kind) noexcept {
    switch (kind) {
    case BasketKind::Full: return "full";
    case BasketKind::Update: return "update";
    case BasketKind::Initial: return "initial";
    case BasketKind::Unspecified: return "unspecified";
    }
    return "unspecified";
}

yyjson_mut_val* makeExtension(yyjson_mut_doc* doc,
                              const ExtensionElement& extension) {
    auto* object = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, object, "name", makeQName(doc, extension.name));
    auto* attributes = yyjson_mut_obj_add_arr(doc, object, "attributes");
    for (const auto& attribute : extension.attributes) {
        auto* item = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_val(doc, item, "name", makeQName(doc, attribute.name));
        addString(doc, item, "value", attribute.value);
        yyjson_mut_arr_append(attributes, item);
    }
    addString(doc, object, "text", extension.text);
    auto* children = yyjson_mut_obj_add_arr(doc, object, "children");
    for (const auto& child : extension.children) {
        yyjson_mut_arr_append(children, makeExtension(doc, child));
    }
    return object;
}

yyjson_mut_val* makeExtensions(yyjson_mut_doc* doc,
                               const std::vector<ExtensionElement>& extensions) {
    auto* array = yyjson_mut_arr(doc);
    for (const auto& extension : extensions) {
        yyjson_mut_arr_append(array, makeExtension(doc, extension));
    }
    return array;
}

yyjson_mut_val* makeStringArray(yyjson_mut_doc* doc,
                                const std::vector<std::string>& values) {
    auto* array = yyjson_mut_arr(doc);
    for (const auto& value : values) {
        yyjson_mut_arr_append(array, yyjson_mut_strcpy(doc, value.c_str()));
    }
    return array;
}

yyjson_mut_val* makeIomObject(yyjson_mut_doc* doc, const IomObject& object);

yyjson_mut_val* makeIomValue(yyjson_mut_doc* doc, const IomValue& value) {
    auto* object = yyjson_mut_obj(doc);
    if (value.isPrimitive()) {
        addString(doc, object, "kind", "primitive");
        addString(doc, object, "value", value.primitive());
    } else {
        addString(doc, object, "kind", "object");
        yyjson_mut_obj_add_val(doc, object, "value",
                               makeIomObject(doc, value.object()));
    }
    return object;
}

yyjson_mut_val* makeIomObject(yyjson_mut_doc* doc, const IomObject& object) {
    auto* result = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, result, "tag", makeName(doc, object.tag()));
    addOptionalString(doc, result, "oid", object.oid());
    addString(doc, result, "operation", operationName(object.operation()));
    addString(doc, result, "consistency", consistencyName(object.consistency()));
    if (object.isReference()) {
        auto* reference = yyjson_mut_obj_add_obj(doc, result, "reference");
        addOptionalString(doc, reference, "targetOid", object.reference().targetOid);
        addOptionalString(doc, reference, "targetBasketId",
                          object.reference().targetBasketId);
        if (object.reference().orderPosition) {
            yyjson_mut_obj_add_uint(doc, reference, "orderPosition",
                                    *object.reference().orderPosition);
        }
    } else {
        yyjson_mut_obj_add_null(doc, result, "reference");
    }
    yyjson_mut_obj_add_val(doc, result, "location",
                           makeLocation(doc, object.sourceLocation()));
    auto* attributes = yyjson_mut_obj_add_arr(doc, result, "attributes");
    for (std::size_t attributeIndex = 0;
         attributeIndex < object.attributeCount(); ++attributeIndex) {
        const auto& name = object.attributeName(attributeIndex);
        auto* attribute = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_val(doc, attribute, "name", makeName(doc, name));
        auto* values = yyjson_mut_obj_add_arr(doc, attribute, "values");
        for (std::size_t valueIndex = 0;
             valueIndex < object.valueCount(name.interlisName()); ++valueIndex) {
            yyjson_mut_arr_append(values,
                makeIomValue(doc, object.value(name.interlisName(), valueIndex)));
        }
        yyjson_mut_arr_append(attributes, attribute);
    }
    return result;
}

yyjson_mut_val* makeHeader(yyjson_mut_doc* doc,
                           const TransferHeader& header) {
    auto* object = yyjson_mut_obj(doc);
    addString(doc, object, "version", xtfVersionName(header.version));
    addString(doc, object, "sender", header.sender);
    addOptionalString(doc, object, "comment", header.comment);
    auto* models = yyjson_mut_obj_add_arr(doc, object, "models");
    for (const auto& model : header.models) {
        auto* item = yyjson_mut_obj(doc);
        addString(doc, item, "name", model.name);
        addOptionalString(doc, item, "version", model.version);
        addOptionalString(doc, item, "uri", model.uri);
        yyjson_mut_obj_add_val(doc, item, "xmlNamespace",
                               makeQName(doc, model.xmlNamespace));
        yyjson_mut_arr_append(models, item);
    }
    auto* spaces = yyjson_mut_obj_add_arr(doc, object, "oidSpaces");
    for (const auto& space : header.oidSpaces) {
        auto* item = yyjson_mut_obj(doc);
        addString(doc, item, "name", space.name);
        addString(doc, item, "domain", space.domain);
        yyjson_mut_arr_append(spaces, item);
    }
    yyjson_mut_obj_add_val(doc, object, "extensions",
                           makeExtensions(doc, header.extensions));
    return object;
}

yyjson_mut_val* makeBasket(yyjson_mut_doc* doc,
                           const BasketMetadata& basket) {
    auto* object = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_val(doc, object, "topic", makeName(doc, basket.topic));
    addString(doc, object, "basketId", basket.basketId);
    addString(doc, object, "kind", basketKindName(basket.kind));
    addString(doc, object, "consistency", consistencyName(basket.consistency));
    addOptionalString(doc, object, "startState", basket.startState);
    addOptionalString(doc, object, "endState", basket.endState);
    yyjson_mut_obj_add_val(doc, object, "domains",
                           makeStringArray(doc, basket.domains));
    yyjson_mut_obj_add_val(doc, object, "topics",
                           makeStringArray(doc, basket.topics));
    yyjson_mut_obj_add_val(doc, object, "extensions",
                           makeExtensions(doc, basket.extensions));
    yyjson_mut_obj_add_val(doc, object, "location",
                           makeLocation(doc, basket.location));
    return object;
}

std::string serializeEvent(const IoxEvent& event) {
    JsonDoc doc(yyjson_mut_doc_new(nullptr), &yyjson_mut_doc_free);
    if (!doc) throw IoxError(DiagnosticCode::InternalError,
                             "Unable to allocate JSON document");
    auto* root = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), root);
    addString(doc.get(), root, "schema", "iox-event/2");
    addString(doc.get(), root, "event", eventKindName(eventKind(event)));
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            yyjson_mut_obj_add_val(doc.get(), root, "header",
                                   makeHeader(doc.get(), value.header));
        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            yyjson_mut_obj_add_val(doc.get(), root, "basket",
                                   makeBasket(doc.get(), value.basket));
        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            value.object.deepCopy();
            yyjson_mut_obj_add_val(doc.get(), root, "object",
                                   makeIomObject(doc.get(), value.object));
        }
    }, event);
    size_t length = 0;
    char* encoded = yyjson_mut_write(doc.get(), YYJSON_WRITE_NOFLAG, &length);
    if (!encoded) throw IoxError(DiagnosticCode::InternalError,
                                 "Unable to serialize JSON event");
    std::string result(encoded, length);
    std::free(encoded);
    result.push_back('\n');
    return result;
}

void writeAll(OutputSink& output, const std::string& value) {
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto written = output.write(value.data() + offset,
                                          value.size() - offset);
        if (written == 0 || written > value.size() - offset) {
            throw IoxError(DiagnosticCode::IoError,
                           "Output sink did not accept JSON data");
        }
        offset += written;
    }
}

} // namespace

struct JsonEventWriter::Impl final {
    enum class State { BeforeTransfer, InTransfer, InBasket, AfterTransfer, Closed, Failed };

    std::shared_ptr<OutputSink> output;
    std::vector<Diagnostic> diagnostics;
    State state = State::BeforeTransfer;

    explicit Impl(std::shared_ptr<OutputSink> sink) : output(std::move(sink)) {
        if (!output) throw IoxError(DiagnosticCode::InvalidArgument,
                                    "JsonEventWriter requires an output sink");
    }

    void validate(EventKind kind) {
        const bool valid =
            (state == State::BeforeTransfer && kind == EventKind::StartTransfer) ||
            (state == State::InTransfer &&
                (kind == EventKind::StartBasket || kind == EventKind::EndTransfer)) ||
            (state == State::InBasket &&
                (kind == EventKind::Object || kind == EventKind::EndBasket));
        if (!valid) {
            state = State::Failed;
            throw IoxError(DiagnosticCode::InvalidEventOrder,
                           "Invalid event order in JSON writer");
        }
    }

    void advance(EventKind kind) noexcept {
        switch (kind) {
        case EventKind::StartTransfer: state = State::InTransfer; break;
        case EventKind::StartBasket: state = State::InBasket; break;
        case EventKind::EndBasket: state = State::InTransfer; break;
        case EventKind::EndTransfer: state = State::AfterTransfer; break;
        case EventKind::Object: break;
        }
    }
};

JsonEventWriter::JsonEventWriter(std::shared_ptr<OutputSink> output)
    : impl_(std::make_unique<Impl>(std::move(output))) {}

JsonEventWriter::~JsonEventWriter() = default;

void JsonEventWriter::write(const IoxEvent& event) {
    if (impl_->state == Impl::State::Closed || impl_->state == Impl::State::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "JSON writer is closed or failed");
    }
    const auto kind = eventKind(event);
    impl_->validate(kind);
    try {
        writeAll(*impl_->output, serializeEvent(event));
        impl_->advance(kind);
    } catch (...) {
        impl_->state = Impl::State::Failed;
        throw;
    }
}

void JsonEventWriter::flush() {
    if (impl_->state == Impl::State::Failed) {
        throw IoxError(DiagnosticCode::WriterStateError,
                       "JSON writer is in a failed state");
    }
    impl_->output->flush();
}

void JsonEventWriter::close() {
    if (impl_->state == Impl::State::Closed) return;
    if (impl_->state != Impl::State::AfterTransfer) {
        impl_->state = Impl::State::Failed;
        throw IoxError(DiagnosticCode::InvalidEventOrder,
                       "JSON stream ended before EndTransferEvent");
    }
    impl_->output->flush();
    impl_->output->close();
    impl_->state = Impl::State::Closed;
}

bool JsonEventWriter::isClosed() const noexcept {
    return impl_->state == Impl::State::Closed;
}

std::vector<Diagnostic> JsonEventWriter::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

} // namespace json
} // namespace iox
