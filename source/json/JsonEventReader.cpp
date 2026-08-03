#include "iox/json/JsonEventReader.h"

#include "iox/Diagnostic.h"

#include <yyjson.h>

#include <algorithm>
#include <deque>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace iox {
namespace json {
namespace {

using JsonDoc = std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)>;

[[noreturn]] void malformed(std::string message) {
    throw IoxError(DiagnosticCode::JsonMalformed, std::move(message));
}

std::string_view jsonString(yyjson_val* value, const char* description) {
    if (!value || !yyjson_is_str(value)) {
        malformed(std::string(description) + " must be a string");
    }
    return {yyjson_get_str(value), yyjson_get_len(value)};
}

yyjson_val* requiredField(yyjson_val* object, const char* key) {
    if (!object || !yyjson_is_obj(object)) malformed("Expected JSON object");
    auto* value = yyjson_obj_get(object, key);
    if (!value) malformed(std::string("Missing JSON field: ") + key);
    return value;
}

yyjson_val* optionalField(yyjson_val* object, const char* key) {
    return object && yyjson_is_obj(object) ? yyjson_obj_get(object, key) : nullptr;
}

yyjson_val* requiredObject(yyjson_val* parent, const char* key) {
    auto* value = requiredField(parent, key);
    if (!yyjson_is_obj(value)) malformed(std::string(key) + " must be an object");
    return value;
}

yyjson_val* requiredArray(yyjson_val* parent, const char* key) {
    auto* value = requiredField(parent, key);
    if (!yyjson_is_arr(value)) malformed(std::string(key) + " must be an array");
    return value;
}

std::string requiredString(yyjson_val* parent, const char* key) {
    return std::string(jsonString(requiredField(parent, key), key));
}

std::optional<std::string> optionalString(yyjson_val* parent,
                                          const char* key) {
    auto* value = optionalField(parent, key);
    if (!value || yyjson_is_null(value)) return std::nullopt;
    return std::string(jsonString(value, key));
}

std::uint64_t requiredUint(yyjson_val* parent, const char* key) {
    auto* value = requiredField(parent, key);
    if (!yyjson_is_uint(value)) malformed(std::string(key) + " must be an unsigned integer");
    return yyjson_get_uint(value);
}

void requireOnlyKeys(yyjson_val* object,
                     std::initializer_list<std::string_view> allowed) {
    if (!yyjson_is_obj(object)) malformed("Expected JSON object");
    std::unordered_set<std::string_view> keys(allowed.begin(), allowed.end());
    size_t index = 0;
    size_t count = 0;
    yyjson_val* key = nullptr;
    yyjson_val* value = nullptr;
    yyjson_obj_foreach(const_cast<yyjson_val*>(object), index, count, key, value) {
        const std::string_view name(yyjson_get_str(key), yyjson_get_len(key));
        if (keys.find(name) == keys.end()) {
            malformed("Unknown JSON field: " + std::string(name));
        }
    }
}

void rejectDuplicateKeys(yyjson_val* value) {
    if (yyjson_is_obj(value)) {
        std::unordered_set<std::string> keys;
        size_t index = 0;
        size_t count = 0;
        yyjson_val* key = nullptr;
        yyjson_val* child = nullptr;
        yyjson_obj_foreach(const_cast<yyjson_val*>(value), index, count, key, child) {
            std::string name(yyjson_get_str(key), yyjson_get_len(key));
            if (!keys.insert(name).second) malformed("Duplicate JSON field: " + name);
            rejectDuplicateKeys(child);
        }
    } else if (yyjson_is_arr(value)) {
        size_t index = 0;
        size_t count = 0;
        yyjson_val* child = nullptr;
        yyjson_arr_foreach(const_cast<yyjson_val*>(value), index, count, child) {
            rejectDuplicateKeys(child);
        }
    }
}

XmlQualifiedName parseQName(yyjson_val* value) {
    requireOnlyKeys(value, {"namespaceUri", "localName", "prefixHint"});
    return {requiredString(value, "namespaceUri"),
            requiredString(value, "localName"),
            requiredString(value, "prefixHint")};
}

IomName parseName(yyjson_val* value) {
    requireOnlyKeys(value, {"interlisName", "xml"});
    auto interlisName = requiredString(value, "interlisName");
    auto* xml = requiredField(value, "xml");
    if (yyjson_is_null(xml)) return IomName(std::move(interlisName));
    if (!yyjson_is_obj(xml)) malformed("xml must be an object or null");
    return IomName(std::move(interlisName), parseQName(xml));
}

SourceLocation parseLocation(yyjson_val* value) {
    requireOnlyKeys(value, {"sourceName", "byteOffset", "line", "column"});
    SourceLocation location;
    location.sourceName = requiredString(value, "sourceName");
    location.byteOffset = requiredUint(value, "byteOffset");
    const auto line = requiredUint(value, "line");
    const auto column = requiredUint(value, "column");
    if (line > UINT32_MAX || column > UINT32_MAX) malformed("Location exceeds uint32 range");
    location.line = static_cast<std::uint32_t>(line);
    location.column = static_cast<std::uint32_t>(column);
    return location;
}

ObjectOperation parseOperation(std::string_view value) {
    if (value == "insert") return ObjectOperation::Insert;
    if (value == "update") return ObjectOperation::Update;
    if (value == "delete") return ObjectOperation::Delete;
    if (value == "none") return ObjectOperation::None;
    malformed("Unknown object operation");
}

Consistency parseConsistency(std::string_view value) {
    if (value == "complete") return Consistency::Complete;
    if (value == "incomplete") return Consistency::Incomplete;
    if (value == "inconsistent") return Consistency::Inconsistent;
    if (value == "adapted") return Consistency::Adapted;
    if (value == "unspecified") return Consistency::Unspecified;
    malformed("Unknown consistency value");
}

BasketKind parseBasketKind(std::string_view value) {
    if (value == "full") return BasketKind::Full;
    if (value == "update") return BasketKind::Update;
    if (value == "initial") return BasketKind::Initial;
    if (value == "unspecified") return BasketKind::Unspecified;
    malformed("Unknown basket kind");
}

ExtensionElement parseExtension(yyjson_val* value) {
    requireOnlyKeys(value, {"name", "attributes", "text", "children"});
    ExtensionElement result;
    result.name = parseQName(requiredObject(value, "name"));
    auto* attributes = requiredArray(value, "attributes");
    size_t index = 0;
    size_t count = 0;
    yyjson_val* attribute = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(attributes), index, count, attribute) {
        requireOnlyKeys(attribute, {"name", "value"});
        result.attributes.push_back({parseQName(requiredObject(attribute, "name")),
                                     requiredString(attribute, "value")});
    }
    result.text = requiredString(value, "text");
    auto* children = requiredArray(value, "children");
    index = 0;
    count = 0;
    yyjson_val* child = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(children), index, count, child) {
        result.children.push_back(parseExtension(child));
    }
    return result;
}

std::vector<ExtensionElement> parseExtensions(yyjson_val* array) {
    if (!yyjson_is_arr(array)) malformed("extensions must be an array");
    std::vector<ExtensionElement> result;
    size_t index = 0;
    size_t count = 0;
    yyjson_val* value = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(array), index, count, value) {
        result.push_back(parseExtension(value));
    }
    return result;
}

std::vector<std::string> parseStringArray(yyjson_val* array,
                                          const char* description) {
    if (!yyjson_is_arr(array)) malformed(std::string(description) + " must be an array");
    std::vector<std::string> result;
    size_t index = 0;
    size_t count = 0;
    yyjson_val* value = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(array), index, count, value) {
        result.emplace_back(jsonString(value, description));
    }
    return result;
}

IomObject parseIomObject(yyjson_val* value);

IomValue parseIomValue(yyjson_val* value) {
    requireOnlyKeys(value, {"kind", "value"});
    const auto kind = requiredString(value, "kind");
    auto* payload = requiredField(value, "value");
    if (kind == "primitive") {
        return IomValue::primitive(std::string(jsonString(payload, "value")));
    }
    if (kind == "object") {
        if (!yyjson_is_obj(payload)) malformed("Object value must contain an object");
        return IomValue::object(parseIomObject(payload));
    }
    malformed("Unknown IOM value kind");
}

IomObject parseIomObject(yyjson_val* value) {
    requireOnlyKeys(value, {"tag", "oid", "operation", "consistency",
                            "reference", "location", "attributes"});
    IomObject result(parseName(requiredObject(value, "tag")),
                     optionalString(value, "oid"));
    result.setOperation(parseOperation(requiredString(value, "operation")));
    result.setConsistency(parseConsistency(requiredString(value, "consistency")));
    auto* reference = requiredField(value, "reference");
    if (!yyjson_is_null(reference)) {
        requireOnlyKeys(reference, {"targetOid", "targetBasketId", "orderPosition"});
        ReferenceInfo info;
        info.targetOid = optionalString(reference, "targetOid");
        info.targetBasketId = optionalString(reference, "targetBasketId");
        if (auto* position = optionalField(reference, "orderPosition")) {
            if (!yyjson_is_uint(position)) malformed("orderPosition must be unsigned");
            info.orderPosition = yyjson_get_uint(position);
        }
        result.setReference(std::move(info));
    }
    result.setSourceLocation(parseLocation(requiredObject(value, "location")));
    auto* attributes = requiredArray(value, "attributes");
    size_t attributeIndex = 0;
    size_t attributeCount = 0;
    yyjson_val* attribute = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(attributes), attributeIndex,
                       attributeCount, attribute) {
        requireOnlyKeys(attribute, {"name", "values"});
        auto name = parseName(requiredObject(attribute, "name"));
        auto* values = requiredArray(attribute, "values");
        size_t valueIndex = 0;
        size_t valueCount = 0;
        yyjson_val* item = nullptr;
        yyjson_arr_foreach(const_cast<yyjson_val*>(values), valueIndex,
                           valueCount, item) {
            result.insertValue(name, result.valueCount(name.interlisName()),
                               parseIomValue(item));
        }
        if (valueCount == 0) malformed("IOM attributes must contain at least one value");
    }
    return result;
}

TransferHeader parseHeader(yyjson_val* value) {
    requireOnlyKeys(value, {"version", "sender", "comment", "models",
                            "oidSpaces", "extensions"});
    TransferHeader result;
    const auto version = requiredString(value, "version");
    if (version == "2.3") result.version = XtfVersion::V23;
    else if (version == "2.4") result.version = XtfVersion::V24;
    else malformed("Unsupported XTF version in JSON event");
    result.sender = requiredString(value, "sender");
    result.comment = optionalString(value, "comment");
    auto* models = requiredArray(value, "models");
    size_t index = 0;
    size_t count = 0;
    yyjson_val* model = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(models), index, count, model) {
        requireOnlyKeys(model, {"name", "version", "uri", "xmlNamespace"});
        result.models.push_back({requiredString(model, "name"),
                                 optionalString(model, "version"),
                                 optionalString(model, "uri"),
                                 parseQName(requiredObject(model, "xmlNamespace"))});
    }
    auto* spaces = requiredArray(value, "oidSpaces");
    index = 0;
    count = 0;
    yyjson_val* space = nullptr;
    yyjson_arr_foreach(const_cast<yyjson_val*>(spaces), index, count, space) {
        requireOnlyKeys(space, {"name", "domain"});
        result.oidSpaces.push_back({requiredString(space, "name"),
                                    requiredString(space, "domain")});
    }
    result.extensions = parseExtensions(requiredArray(value, "extensions"));
    return result;
}

BasketMetadata parseBasket(yyjson_val* value) {
    requireOnlyKeys(value, {"topic", "basketId", "kind", "consistency",
                            "startState", "endState", "domains", "topics",
                            "extensions", "location"});
    BasketMetadata result;
    result.topic = parseName(requiredObject(value, "topic"));
    result.basketId = requiredString(value, "basketId");
    result.kind = parseBasketKind(requiredString(value, "kind"));
    result.consistency = parseConsistency(requiredString(value, "consistency"));
    result.startState = optionalString(value, "startState");
    result.endState = optionalString(value, "endState");
    result.domains = parseStringArray(requiredArray(value, "domains"), "domains");
    result.topics = parseStringArray(requiredArray(value, "topics"), "topics");
    result.extensions = parseExtensions(requiredArray(value, "extensions"));
    result.location = parseLocation(requiredObject(value, "location"));
    return result;
}

IoxEvent parseEvent(const std::string& line) {
    yyjson_read_err error{};
    JsonDoc doc(yyjson_read_opts(const_cast<char*>(line.data()), line.size(),
                                 YYJSON_READ_NOFLAG, nullptr, &error),
                &yyjson_doc_free);
    if (!doc) {
        malformed("Malformed JSON at byte " + std::to_string(error.pos) +
                  ": " + (error.msg ? std::string(error.msg) : "unknown error"));
    }
    auto* root = yyjson_doc_get_root(doc.get());
    if (!yyjson_is_obj(root)) malformed("JSON event root must be an object");
    rejectDuplicateKeys(root);
    if (requiredString(root, "schema") != "iox-event/2") {
        malformed("Unsupported event JSON schema");
    }
    const auto event = requiredString(root, "event");
    if (event == "startTransfer") {
        requireOnlyKeys(root, {"schema", "event", "header"});
        return StartTransferEvent{parseHeader(requiredObject(root, "header"))};
    }
    if (event == "startBasket") {
        requireOnlyKeys(root, {"schema", "event", "basket"});
        return StartBasketEvent{parseBasket(requiredObject(root, "basket"))};
    }
    if (event == "object") {
        requireOnlyKeys(root, {"schema", "event", "object"});
        return ObjectEvent{parseIomObject(requiredObject(root, "object"))};
    }
    if (event == "endBasket") {
        requireOnlyKeys(root, {"schema", "event"});
        return EndBasketEvent{};
    }
    if (event == "endTransfer") {
        requireOnlyKeys(root, {"schema", "event"});
        return EndTransferEvent{};
    }
    malformed("Unknown event kind");
}

} // namespace

struct JsonEventReader::Impl final {
    enum class State { BeforeTransfer, InTransfer, InBasket, AfterTransfer, Failed };

    JsonReaderOptions options;
    std::string buffered;
    std::deque<IoxEvent> events;
    std::vector<Diagnostic> diagnostics;
    State state = State::BeforeTransfer;
    bool inputFinished = false;
    std::uint64_t consumedBytes = 0;
    std::uint32_t line = 1;

    explicit Impl(JsonReaderOptions readerOptions)
        : options(std::move(readerOptions)) {
        if (options.maxLineBytes == 0) {
            throw IoxError(DiagnosticCode::InvalidArgument,
                           "JSON maxLineBytes must be greater than zero");
        }
    }

    [[noreturn]] void fail(DiagnosticCode code, std::string message,
                           std::uint64_t byteOffset = 0) {
        state = State::Failed;
        throw IoxError(code, std::move(message),
                       {options.sourceName, consumedBytes + byteOffset, line, 1});
    }

    void validate(const IoxEvent& event) {
        const auto kind = eventKind(event);
        const bool valid =
            (state == State::BeforeTransfer && kind == EventKind::StartTransfer) ||
            (state == State::InTransfer &&
                (kind == EventKind::StartBasket || kind == EventKind::EndTransfer)) ||
            (state == State::InBasket &&
                (kind == EventKind::Object || kind == EventKind::EndBasket));
        if (!valid) fail(DiagnosticCode::InvalidEventOrder,
                         "Invalid event order in JSON stream");
        switch (kind) {
        case EventKind::StartTransfer: state = State::InTransfer; break;
        case EventKind::StartBasket: state = State::InBasket; break;
        case EventKind::EndBasket: state = State::InTransfer; break;
        case EventKind::EndTransfer: state = State::AfterTransfer; break;
        case EventKind::Object: break;
        }
    }

    void parseLine(std::string lineValue) {
        if (!lineValue.empty() && lineValue.back() == '\r') lineValue.pop_back();
        if (lineValue.empty()) fail(DiagnosticCode::JsonMalformed,
                                    "Blank lines are not valid event JSON");
        try {
            auto event = parseEvent(lineValue);
            validate(event);
            events.push_back(std::move(event));
        } catch (const IoxError& error) {
            if (state == State::Failed) throw;
            fail(error.code(), error.what());
        }
        consumedBytes += lineValue.size() + 1;
        ++line;
    }

    void consumeCompleteLines() {
        while (true) {
            const auto newline = buffered.find('\n');
            if (newline == std::string::npos) break;
            auto lineValue = buffered.substr(0, newline);
            buffered.erase(0, newline + 1);
            parseLine(std::move(lineValue));
        }
        if (buffered.size() > options.maxLineBytes) {
            fail(DiagnosticCode::JsonMalformed,
                 "JSON event exceeds maxLineBytes");
        }
    }
};

JsonEventReader::JsonEventReader(JsonReaderOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

JsonEventReader::~JsonEventReader() = default;

ReadOutcome JsonEventReader::next() {
    if (!impl_->events.empty()) {
        auto event = std::move(impl_->events.front());
        impl_->events.pop_front();
        return {ReaderProgress::Event, std::move(event)};
    }
    if (impl_->inputFinished) return {ReaderProgress::End, std::nullopt};
    return {ReaderProgress::NeedInput, std::nullopt};
}

void JsonEventReader::feed(ByteView data) {
    if (impl_->inputFinished || impl_->state == Impl::State::Failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "Cannot feed a finished or failed JSON reader");
    }
    if (!data.empty()) {
        impl_->buffered.append(reinterpret_cast<const char*>(data.data()), data.size());
        impl_->consumeCompleteLines();
    }
}

void JsonEventReader::finish() {
    if (impl_->inputFinished || impl_->state == Impl::State::Failed) {
        throw IoxError(DiagnosticCode::InvalidState,
                       "JSON reader can only be finished once");
    }
    if (!impl_->buffered.empty()) {
        auto lineValue = std::move(impl_->buffered);
        impl_->buffered.clear();
        impl_->parseLine(std::move(lineValue));
    }
    if (impl_->state != Impl::State::AfterTransfer) {
        impl_->fail(DiagnosticCode::InvalidEventOrder,
                    "JSON stream ended before EndTransferEvent");
    }
    impl_->inputFinished = true;
}

bool JsonEventReader::isFinished() const noexcept {
    return impl_->inputFinished && impl_->events.empty();
}

std::vector<Diagnostic> JsonEventReader::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

} // namespace json
} // namespace iox
