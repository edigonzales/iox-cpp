#include "iox/json/JsonEventWriter.h"

#include <sstream>
#include <string>

namespace iox {
namespace json {

// ============================================================================
// Minimal JSON serializer for IoxEvent.
//
// Produces newline-delimited JSON (NDJSON). Each event is one line.
// ============================================================================

namespace {

class JsonWriter {
public:
    explicit JsonWriter(std::shared_ptr<OutputSink> sink)
        : sink_(std::move(sink)) {}

    void writeString(const std::string& s) {
        writeRaw("\"");
        for (char c : s) {
            switch (c) {
            case '"':  writeRaw("\\\""); break;
            case '\\': writeRaw("\\\\"); break;
            case '\b': writeRaw("\\b"); break;
            case '\f': writeRaw("\\f"); break;
            case '\n': writeRaw("\\n"); break;
            case '\r': writeRaw("\\r"); break;
            case '\t': writeRaw("\\t"); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(c));
                    writeRaw(buf);
                } else {
                    writeRaw(std::string(1, c));
                }
            }
        }
        writeRaw("\"");
    }

    void writeKey(const std::string& key) {
        writeString(key);
        writeRaw(":");
    }

    void writeKeyValue(const std::string& key, const std::string& value) {
        writeKey(key);
        writeString(value);
    }

    void writeKeyValue(const std::string& key, bool value) {
        writeKey(key);
        writeRaw(value ? "true" : "false");
    }

    void writeKeyValue(const std::string& key, std::int64_t value) {
        writeKey(key);
        writeRaw(std::to_string(value));
    }

    void writeKeyValueOpt(const std::string& key,
                           const std::optional<std::string>& value) {
        if (value) {
            writeRaw(",");
            writeKeyValue(key, *value);
        }
    }

    void writeKeyValueOpt(const std::string& key,
                           const std::optional<int>& value) {
        if (value) {
            writeRaw(",");
            writeKey(key);
            writeRaw(std::to_string(*value));
        }
    }

    void writeRaw(const std::string& s) {
        sink_->write(s.data(), s.size());
    }

    void writeLine(const std::string& s) {
        sink_->write(s.data(), s.size());
        sink_->write("\n", 1);
    }

private:
    std::shared_ptr<OutputSink> sink_;
};

void writeIomObject(JsonWriter& jw, const IomObject& obj);

void writeIomValue(JsonWriter& jw, const IomValue& val) {
    switch (val.kind()) {
    case IomValue::Kind::Null:
        jw.writeRaw("null");
        break;
    case IomValue::Kind::Text:
        jw.writeString(val.asText());
        break;
    case IomValue::Kind::Integer:
        jw.writeRaw(std::to_string(val.asInteger()));
        break;
    case IomValue::Kind::Decimal: {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.15g", val.asDecimal());
        jw.writeRaw(buf);
        break;
    }
    case IomValue::Kind::Boolean:
        jw.writeRaw(val.asBoolean() ? "true" : "false");
        break;
    }
}

void writeIomObject(JsonWriter& jw, const IomObject& obj) {
    jw.writeRaw("{");

    // tag
    jw.writeKeyValue("tag", obj.tag().iliName());

    // ref/bid/orderPos on the object itself
    if (obj.ref()) {
        jw.writeRaw(",");
        jw.writeKeyValue("ref", *obj.ref());
    }
    if (obj.bid()) {
        jw.writeRaw(",");
        jw.writeKeyValue("bid", *obj.bid());
    }
    if (obj.orderPos()) {
        jw.writeRaw(",");
        jw.writeKeyValue("orderPos", std::to_string(*obj.orderPos()));
    }

    // attributes
    if (obj.attributeCount() > 0) {
        jw.writeRaw(",\"attrs\":[");
        for (std::size_t i = 0; i < obj.attributeCount(); ++i) {
            if (i > 0) jw.writeRaw(",");
            const auto& attr = obj.attributeAt(i);
            jw.writeRaw("{");
            jw.writeKeyValue("name", attr.name.iliName());

            if (attr.values.size() == 1) {
                jw.writeRaw(",");
                jw.writeKey("value");
                const auto& val = attr.values[0];
                if (auto* prim = std::get_if<IomValue>(&val)) {
                    writeIomValue(jw, *prim);
                } else if (auto* sub = std::get_if<IomObject>(&val)) {
                    writeIomObject(jw, *sub);
                }
            } else if (attr.values.size() > 1) {
                jw.writeRaw(",\"values\":[");
                for (std::size_t j = 0; j < attr.values.size(); ++j) {
                    if (j > 0) jw.writeRaw(",");
                    const auto& val = attr.values[j];
                    if (auto* prim = std::get_if<IomValue>(&val)) {
                        writeIomValue(jw, *prim);
                    } else if (auto* sub = std::get_if<IomObject>(&val)) {
                        writeIomObject(jw, *sub);
                    }
                }
                jw.writeRaw("]");
            }

            jw.writeRaw("}");
        }
        jw.writeRaw("]");
    }

    jw.writeRaw("}");
}

} // anonymous namespace

// ============================================================================
// JsonEventWriter::Impl
// ============================================================================

struct JsonEventWriter::Impl {
    std::shared_ptr<OutputSink> sink;
    bool closed = false;
    std::vector<Diagnostic> diagnostics;
};

JsonEventWriter::JsonEventWriter(std::shared_ptr<OutputSink> output)
    : impl_(std::make_unique<Impl>()) {
    impl_->sink = std::move(output);
}

JsonEventWriter::~JsonEventWriter() {
    if (!impl_->closed) {
        close();
    }
}

void JsonEventWriter::write(const IoxEvent& event) {
    if (impl_->closed) {
        impl_->diagnostics.push_back({Diagnostic::Severity::Error,
            ErrorCode::InvalidState,
            "Cannot write to closed JsonEventWriter"});
        return;
    }

    JsonWriter jw(impl_->sink);

    std::visit([&](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, StartTransferEvent>) {
            jw.writeRaw("{\"type\":\"StartTransfer\"");
            jw.writeKeyValueOpt("sender", e.sender);
            jw.writeKeyValueOpt("comment", e.comment);
            jw.writeKeyValueOpt("iliVersion", e.iliVersion);
            jw.writeKeyValueOpt("software", e.software);
            jw.writeKeyValueOpt("date", e.date);
            if (e.version) {
                jw.writeRaw(",");
                jw.writeKey("version");
                jw.writeRaw(std::to_string(*e.version));
            }
            jw.writeLine("}");

        } else if constexpr (std::is_same_v<T, StartBasketEvent>) {
            jw.writeRaw("{\"type\":\"StartBasket\"");
            jw.writeRaw(",");
            jw.writeKeyValue("basketType", e.basketType.iliName());
            jw.writeRaw(",");
            jw.writeKeyValue("bid", e.bid);
            jw.writeKeyValueOpt("consistency", e.consistency);
            jw.writeKeyValueOpt("operation", e.operation);
            if (e.oidDomain) {
                jw.writeRaw(",");
                jw.writeKey("oidDomain");
                jw.writeRaw(std::to_string(*e.oidDomain));
            }
            jw.writeKeyValueOpt("startState", e.startState);
            jw.writeKeyValueOpt("endState", e.endState);
            jw.writeKeyValueOpt("kind", e.kind);
            if (!e.domains.empty()) {
                jw.writeRaw(",\"domains\":[");
                for (std::size_t i = 0; i < e.domains.size(); ++i) {
                    if (i > 0) jw.writeRaw(",");
                    jw.writeString(e.domains[i]);
                }
                jw.writeRaw("]");
            }
            jw.writeLine("}");

        } else if constexpr (std::is_same_v<T, ObjectEvent>) {
            jw.writeRaw("{\"type\":\"Object\"");
            jw.writeRaw(",");
            jw.writeKeyValue("operation", e.operation);
            jw.writeRaw(",");
            jw.writeKeyValue("objectId", e.objectId);
            jw.writeKeyValueOpt("consistency", e.consistency);
            jw.writeKeyValueOpt("refBid", e.refBid);
            jw.writeKeyValueOpt("refOrderPos", e.refOrderPos);
            jw.writeRaw(",\"object\":");
            writeIomObject(jw, e.object);
            jw.writeLine("}");

        } else if constexpr (std::is_same_v<T, EndBasketEvent>) {
            jw.writeRaw("{\"type\":\"EndBasket\"");
            jw.writeRaw(",");
            jw.writeKeyValue("bid", e.bid);
            jw.writeLine("}");

        } else if constexpr (std::is_same_v<T, EndTransferEvent>) {
            jw.writeLine("{\"type\":\"EndTransfer\"}");
        }
    }, event);
}

void JsonEventWriter::flush() {
    if (impl_->sink) impl_->sink->flush();
}

void JsonEventWriter::close() {
    if (!impl_->closed) {
        impl_->closed = true;
        if (impl_->sink) impl_->sink->close();
    }
}

bool JsonEventWriter::isClosed() const noexcept {
    return impl_->closed;
}

std::vector<Diagnostic> JsonEventWriter::takeDiagnostics() {
    auto diags = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return diags;
}

} // namespace json
} // namespace iox
