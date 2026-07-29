#include "iox/json/JsonEventReader.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

namespace iox {
namespace json {

namespace {

enum class TokenType {
    Eof, LBrace, RBrace, LBracket, RBracket, Colon, Comma,
    String, Number, True, False, Null, Invalid
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string_view text;
};

class JsonLexer final {
public:
    explicit JsonLexer(std::string_view input) : input_(input) {}

    Token next() {
        skipWhitespace();
        if (pos_ >= input_.size()) return {TokenType::Eof};

        const char c = input_[pos_];
        switch (c) {
        case '{': ++pos_; return {TokenType::LBrace};
        case '}': ++pos_; return {TokenType::RBrace};
        case '[': ++pos_; return {TokenType::LBracket};
        case ']': ++pos_; return {TokenType::RBracket};
        case ':': ++pos_; return {TokenType::Colon};
        case ',': ++pos_; return {TokenType::Comma};
        case '"': return readString();
        case 't': return readKeyword("true", TokenType::True);
        case 'f': return readKeyword("false", TokenType::False);
        case 'n': return readKeyword("null", TokenType::Null);
        default:
            if (c == '-' || (c >= '0' && c <= '9')) return readNumber();
            ++pos_;
            return {TokenType::Invalid};
        }
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;

    void skipWhitespace() {
        while (pos_ < input_.size() &&
               (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                input_[pos_] == '\r' || input_[pos_] == '\n')) {
            ++pos_;
        }
    }

    Token readString() {
        ++pos_;
        const auto start = pos_;
        while (pos_ < input_.size()) {
            if (input_[pos_] == '"') {
                const auto end = pos_++;
                return {TokenType::String, input_.substr(start, end - start)};
            }
            if (input_[pos_] == '\\' && pos_ + 1 < input_.size()) {
                pos_ += 2;
            } else {
                ++pos_;
            }
        }
        return {TokenType::Invalid};
    }

    Token readNumber() {
        const auto start = pos_;
        if (input_[pos_] == '-') ++pos_;
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        }
        return {TokenType::Number, input_.substr(start, pos_ - start)};
    }

    Token readKeyword(const char* keyword, TokenType type) {
        const auto length = std::strlen(keyword);
        if (pos_ + length <= input_.size() && input_.substr(pos_, length) == keyword) {
            const auto result = Token{type, input_.substr(pos_, length)};
            pos_ += length;
            return result;
        }
        ++pos_;
        return {TokenType::Invalid};
    }
};

std::string unescapeJson(std::string_view raw) {
    std::string result;
    result.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '\\' || i + 1 >= raw.size()) {
            result.push_back(raw[i]);
            continue;
        }
        switch (raw[++i]) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
            if (i + 4 >= raw.size()) return {};
            const auto code = std::strtoul(std::string(raw.substr(i + 1, 4)).c_str(), nullptr, 16);
            if (code <= 0x7f) result.push_back(static_cast<char>(code));
            else if (code <= 0x7ff) {
                result.push_back(static_cast<char>(0xc0 | (code >> 6)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            } else {
                result.push_back(static_cast<char>(0xe0 | (code >> 12)));
                result.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
                result.push_back(static_cast<char>(0x80 | (code & 0x3f)));
            }
            i += 4;
            break;
        }
        default: return {};
        }
    }
    return result;
}

struct JsonValue final {
    enum class Type { Null, Bool, Number, String, Object, Array };
    Type type = Type::Null;
    bool boolean = false;
    std::string text;
    std::vector<std::pair<std::string, JsonValue>> object;
    std::vector<JsonValue> array;
};

class JsonDocumentParser final {
public:
    explicit JsonDocumentParser(std::string_view input) : lexer_(input) {}

    bool parse(JsonValue& value) {
        if (!parseValue(lexer_.next(), value)) return false;
        return lexer_.next().type == TokenType::Eof;
    }

private:
    JsonLexer lexer_;

    bool parseValue(Token token, JsonValue& value) {
        switch (token.type) {
        case TokenType::Null: value.type = JsonValue::Type::Null; return true;
        case TokenType::True: value.type = JsonValue::Type::Bool; value.boolean = true; return true;
        case TokenType::False: value.type = JsonValue::Type::Bool; value.boolean = false; return true;
        case TokenType::String:
            value.type = JsonValue::Type::String;
            value.text = unescapeJson(token.text);
            return true;
        case TokenType::Number:
            value.type = JsonValue::Type::Number;
            value.text = std::string(token.text);
            return true;
        case TokenType::LBrace: return parseObject(value);
        case TokenType::LBracket: return parseArray(value);
        default: return false;
        }
    }

    bool parseObject(JsonValue& value) {
        value.type = JsonValue::Type::Object;
        auto token = lexer_.next();
        if (token.type == TokenType::RBrace) return true;
        while (token.type == TokenType::String) {
            const auto key = unescapeJson(token.text);
            if (lexer_.next().type != TokenType::Colon) return false;
            JsonValue child;
            if (!parseValue(lexer_.next(), child)) return false;
            value.object.emplace_back(key, std::move(child));
            token = lexer_.next();
            if (token.type == TokenType::RBrace) return true;
            if (token.type != TokenType::Comma) return false;
            token = lexer_.next();
        }
        return false;
    }

    bool parseArray(JsonValue& value) {
        value.type = JsonValue::Type::Array;
        auto token = lexer_.next();
        if (token.type == TokenType::RBracket) return true;
        while (true) {
            JsonValue child;
            if (!parseValue(token, child)) return false;
            value.array.push_back(std::move(child));
            token = lexer_.next();
            if (token.type == TokenType::RBracket) return true;
            if (token.type != TokenType::Comma) return false;
            token = lexer_.next();
        }
    }
};

const JsonValue* field(const JsonValue& object, std::string_view name) {
    if (object.type != JsonValue::Type::Object) return nullptr;
    for (const auto& entry : object.object) {
        if (entry.first == name) return &entry.second;
    }
    return nullptr;
}

std::string stringField(const JsonValue& object, std::string_view name,
                        std::string defaultValue = {}) {
    const auto* value = field(object, name);
    return value && value->type == JsonValue::Type::String ? value->text : std::move(defaultValue);
}

std::optional<std::int64_t> integerField(const JsonValue& object, std::string_view name) {
    const auto* value = field(object, name);
    if (!value) return std::nullopt;
    try {
        if (value->type == JsonValue::Type::Number || value->type == JsonValue::Type::String) {
            return std::stoll(value->text);
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<std::string> optionalStringField(const JsonValue& object, std::string_view name) {
    const auto* value = field(object, name);
    if (!value || value->type == JsonValue::Type::Null) return std::nullopt;
    if (value->type == JsonValue::Type::String) return value->text;
    return std::nullopt;
}

IomValue parsePrimitive(const JsonValue& value) {
    switch (value.type) {
    case JsonValue::Type::Null: return IomValue::null();
    case JsonValue::Type::String: return IomValue::text(value.text);
    case JsonValue::Type::Bool: return IomValue::boolean(value.boolean);
    case JsonValue::Type::Number:
        try {
            if (value.text.find_first_of(".eE") == std::string::npos) {
                return IomValue::integer(std::stoll(value.text));
            }
            return IomValue::decimal(std::stod(value.text));
        } catch (...) {
            return IomValue::text(value.text);
        }
    default: return IomValue::null();
    }
}

IomObject parseObjectValue(const JsonValue& value) {
    IomObject result(IomName(stringField(value, "tag")));
    if (const auto ref = optionalStringField(value, "ref")) result.setRef(*ref);
    if (const auto bid = optionalStringField(value, "bid")) result.setBid(*bid);
    if (const auto order = integerField(value, "orderPos")) result.setOrderPos(*order);

    const auto* attrs = field(value, "attrs");
    if (!attrs || attrs->type != JsonValue::Type::Array) return result;
    for (const auto& attrValue : attrs->array) {
        const auto name = stringField(attrValue, "name");
        if (name.empty()) continue;
        auto& attr = result.setAttribute(IomName(name));
        if (const auto ref = optionalStringField(attrValue, "ref")) attr.ref = *ref;
        if (const auto bid = optionalStringField(attrValue, "bid")) attr.bid = *bid;
        if (const auto order = integerField(attrValue, "orderPos")) attr.orderPos = *order;

        const auto addValue = [&attr](const JsonValue& item) {
            if (item.type == JsonValue::Type::Object) {
                attr.values.emplace_back(parseObjectValue(item));
            } else {
                attr.values.emplace_back(parsePrimitive(item));
            }
        };
        if (const auto* values = field(attrValue, "values");
            values && values->type == JsonValue::Type::Array) {
            for (const auto& item : values->array) addValue(item);
        } else if (const auto* single = field(attrValue, "value")) {
            addValue(*single);
        }
    }
    return result;
}

class JsonEventParser final {
public:
    bool feedLine(std::string_view line, IoxEvent& event,
                  std::vector<Diagnostic>& diagnostics) const {
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty()) return false;

        JsonValue root;
        JsonDocumentParser parser(line);
        if (!parser.parse(root)) {
            diagnostics.push_back({Diagnostic::Severity::Error, ErrorCode::JsonParseError,
                                   "Malformed JSON event line"});
            return false;
        }
        std::string type = stringField(root, "type");
        if (type.empty()) type = stringField(root, "event");
        if (type == "startTransfer" || type == "StartTransfer") {
            StartTransferEvent value;
            value.sender = stringField(root, "sender");
            value.comment = stringField(root, "comment");
            value.iliVersion = stringField(root, "iliVersion");
            value.software = stringField(root, "software");
            value.date = stringField(root, "date");
            if (const auto version = integerField(root, "version")) value.version = static_cast<int>(*version);
            event = std::move(value);
            return true;
        }
        if (type == "startBasket" || type == "StartBasket") {
            StartBasketEvent value;
            value.basketType = IomName(stringField(root, "basketType"));
            value.bid = stringField(root, "bid");
            value.consistency = stringField(root, "consistency");
            value.operation = stringField(root, "operation");
            if (const auto domain = integerField(root, "oidDomain")) value.oidDomain = static_cast<int>(*domain);
            value.startState = optionalStringField(root, "startState");
            value.endState = optionalStringField(root, "endState");
            value.kind = optionalStringField(root, "kind");
            if (const auto* domains = field(root, "domains"); domains && domains->type == JsonValue::Type::Array) {
                for (const auto& domain : domains->array) {
                    if (domain.type == JsonValue::Type::String) value.domains.push_back(domain.text);
                }
            }
            event = std::move(value);
            return true;
        }
        if (type == "object" || type == "Object") {
            ObjectEvent value;
            value.operation = stringField(root, "operation");
            value.objectId = stringField(root, "objectId");
            value.consistency = optionalStringField(root, "consistency");
            value.refBid = optionalStringField(root, "refBid");
            value.refOrderPos = optionalStringField(root, "refOrderPos");
            if (const auto* object = field(root, "object"); object && object->type == JsonValue::Type::Object) {
                value.object = parseObjectValue(*object);
            }
            event = std::move(value);
            return true;
        }
        if (type == "endBasket" || type == "EndBasket") {
            event = EndBasketEvent{stringField(root, "bid")};
            return true;
        }
        if (type == "endTransfer" || type == "EndTransfer") {
            event = EndTransferEvent{};
            return true;
        }

        diagnostics.push_back({Diagnostic::Severity::Error, ErrorCode::JsonParseError,
                               "Unknown event type: " + type});
        return false;
    }
};

} // namespace

struct JsonEventReader::Impl {
    std::string buffer;
    std::size_t lineStart = 0;
    bool finished = false;
    bool eof = false;
    std::vector<Diagnostic> diagnostics;
    JsonEventParser parser;
};

JsonEventReader::JsonEventReader() : impl_(std::make_unique<Impl>()) {}
JsonEventReader::~JsonEventReader() = default;

void JsonEventReader::feed(ByteView data) {
    if (impl_->finished) {
        impl_->diagnostics.push_back({Diagnostic::Severity::Error, ErrorCode::InvalidState,
                                      "Cannot feed JsonEventReader after finish()"});
        return;
    }
    impl_->buffer.append(data.data(), data.size());
}

void JsonEventReader::finish() {
    if (impl_->finished) {
        impl_->diagnostics.push_back({Diagnostic::Severity::Error, ErrorCode::InvalidState,
                                      "JsonEventReader finish() called more than once"});
        return;
    }
    impl_->finished = true;
}

bool JsonEventReader::isFinished() const noexcept { return impl_->eof; }

ReadOutcome JsonEventReader::next() {
    ReadOutcome outcome;
    if (impl_->eof) {
        outcome.status = ReadOutcome::Status::End;
        return outcome;
    }

    auto& buffer = impl_->buffer;
    auto position = impl_->lineStart;
    while (position < buffer.size()) {
        if (buffer[position] == '\n') {
            const auto line = std::string_view(buffer).substr(impl_->lineStart,
                                                              position - impl_->lineStart);
            impl_->lineStart = position + 1;
            IoxEvent event;
            if (impl_->parser.feedLine(line, event, impl_->diagnostics)) {
                outcome.status = ReadOutcome::Status::Event;
                outcome.event = std::move(event);
                outcome.diagnostics = std::move(impl_->diagnostics);
                impl_->diagnostics.clear();
                return outcome;
            }
            position = impl_->lineStart;
            continue;
        }
        ++position;
    }

    if (impl_->finished) {
        if (impl_->lineStart < buffer.size()) {
            const auto line = std::string_view(buffer).substr(impl_->lineStart);
            impl_->lineStart = buffer.size();
            IoxEvent event;
            if (impl_->parser.feedLine(line, event, impl_->diagnostics)) {
                outcome.status = ReadOutcome::Status::Event;
                outcome.event = std::move(event);
                outcome.diagnostics = std::move(impl_->diagnostics);
                impl_->diagnostics.clear();
                return outcome;
            }
        }
        impl_->eof = true;
        outcome.status = ReadOutcome::Status::End;
    } else {
        outcome.status = ReadOutcome::Status::NeedInput;
    }
    outcome.diagnostics = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return outcome;
}

std::vector<Diagnostic> JsonEventReader::takeDiagnostics() {
    auto result = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return result;
}

} // namespace json
} // namespace iox
