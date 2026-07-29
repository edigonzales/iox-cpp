#include "iox/json/JsonEventReader.h"
#include "iox/FormatRegistry.h"

#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace iox {
namespace json {

// ============================================================================
// Minimal JSON parser for NDJSON event streams.
//
// This is a hand-written, non-allocating (where possible) parser that
// handles the JSON subset needed for IoxEvent serialization. It does NOT
// support arbitrary JSON — only the structure produced by JsonEventWriter.
//
// Each line is one JSON object representing one IoxEvent.
// ============================================================================

namespace {

// ---- JSON token types ----
enum class TokenType {
    Eof, LBrace, RBrace, LBracket, RBracket,
    Colon, Comma, String, Number, True, False, Null
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string_view text;  // raw text for String/Number
};

// ---- Lexer ----
class JsonLexer {
public:
    explicit JsonLexer(std::string_view input) : input_(input), pos_(0) {}

    Token next() {
        skipWhitespace();
        if (pos_ >= input_.size()) return {TokenType::Eof};

        char c = input_[pos_];
        switch (c) {
        case '{': ++pos_; return {TokenType::LBrace, input_.substr(pos_ - 1, 1)};
        case '}': ++pos_; return {TokenType::RBrace, input_.substr(pos_ - 1, 1)};
        case '[': ++pos_; return {TokenType::LBracket, input_.substr(pos_ - 1, 1)};
        case ']': ++pos_; return {TokenType::RBracket, input_.substr(pos_ - 1, 1)};
        case ':': ++pos_; return {TokenType::Colon, input_.substr(pos_ - 1, 1)};
        case ',': ++pos_; return {TokenType::Comma, input_.substr(pos_ - 1, 1)};
        case '"': return readString();
        case 't': return readKeyword("true", TokenType::True);
        case 'f': return readKeyword("false", TokenType::False);
        case 'n': return readKeyword("null", TokenType::Null);
        default:
            if (c == '-' || (c >= '0' && c <= '9')) return readNumber();
            return {TokenType::Eof}; // error
        }
    }

    std::size_t pos() const { return pos_; }

private:
    std::string_view input_;
    std::size_t pos_;

    void skipWhitespace() {
        while (pos_ < input_.size() &&
               (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                input_[pos_] == '\r' || input_[pos_] == '\n')) {
            ++pos_;
        }
    }

    Token readString() {
        ++pos_; // skip opening quote
        auto start = pos_;
        while (pos_ < input_.size() && input_[pos_] != '"') {
            if (input_[pos_] == '\\') pos_ += 2; // skip escape
            else ++pos_;
        }
        auto end = pos_;
        if (pos_ < input_.size()) ++pos_; // skip closing quote
        return {TokenType::String, input_.substr(start, end - start)};
    }

    Token readNumber() {
        auto start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') ++pos_;
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
        auto len = std::strlen(keyword);
        if (pos_ + len <= input_.size() &&
            input_.substr(pos_, len) == keyword) {
            auto tok = Token{type, input_.substr(pos_, len)};
            pos_ += len;
            return tok;
        }
        return {TokenType::Eof};
    }
};

// ---- JSON unescaping ----
std::string unescapeJson(std::string_view raw) {
    std::string result;
    result.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            switch (raw[i + 1]) {
            case '"':  result += '"'; break;
            case '\\': result += '\\'; break;
            case '/':  result += '/'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'u': {
                // \uXXXX — minimal support for BMP
                if (i + 5 < raw.size()) {
                    auto hex = raw.substr(i + 2, 4);
                    char* end = nullptr;
                    unsigned long cp = std::strtoul(std::string(hex).c_str(), &end, 16);
                    if (cp <= 0x7F) {
                        result += static_cast<char>(cp);
                    } else if (cp <= 0x7FF) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    i += 5;
                }
                break;
            }
            default: result += raw[i + 1]; break;
            }
            ++i;
        } else {
            result += raw[i];
        }
    }
    return result;
}

// ============================================================================
// Parser state
// ============================================================================

class JsonEventParser {
public:
    explicit JsonEventParser() = default;

    bool feedLine(std::string_view line, IoxEvent& outEvent,
                  std::vector<Diagnostic>& diags) {
        if (line.empty()) return false;

        // Trim trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty()) return false;

        JsonLexer lexer(line);
        auto tok = lexer.next();
        if (tok.type != TokenType::LBrace) {
            diags.push_back({Diagnostic::Severity::Error,
                ErrorCode::JsonParseError,
                "Expected '{' at start of JSON event line"});
            return false;
        }

        // Scan for "type" key and extract its value
        std::string eventType;
        bool found = false;

        while (!found) {
            tok = lexer.next();
            if (tok.type == TokenType::RBrace || tok.type == TokenType::Eof) {
                break;
            }
            if (tok.type != TokenType::String) break;

            std::string key = unescapeJson(tok.text);

            // Expect colon
            tok = lexer.next();
            if (tok.type != TokenType::Colon) break;

            if (key == "type") {
                tok = lexer.next();
                if (tok.type == TokenType::String) {
                    eventType = unescapeJson(tok.text);
                    found = true;
                }
                break;
            } else {
                // Skip this key-value pair: read the value token and skip
                tok = lexer.next();
                skipValue(lexer, tok);
            }

            // Expect comma or end of object
            tok = lexer.next();
            if (tok.type == TokenType::RBrace) break;
            if (tok.type != TokenType::Comma) break;
        }

        if (!found || eventType.empty()) {
            diags.push_back({Diagnostic::Severity::Error,
                ErrorCode::JsonParseError,
                "Missing or empty 'type' field in event JSON"});
            return false;
        }

        return createEvent(eventType, outEvent, diags);
    }

private:
    static void skipValue(JsonLexer& lexer, Token& tok) {
        if (tok.type == TokenType::Eof) tok = lexer.next();
        switch (tok.type) {
        case TokenType::String:
        case TokenType::Number:
        case TokenType::True:
        case TokenType::False:
        case TokenType::Null:
            break;
        case TokenType::LBrace:
            skipObject(lexer);
            break;
        case TokenType::LBracket:
            skipArray(lexer);
            break;
        default:
            break;
        }
    }

    static void skipObject(JsonLexer& lexer) {
        int depth = 1;
        while (depth > 0) {
            auto tok = lexer.next();
            if (tok.type == TokenType::Eof) return;
            if (tok.type == TokenType::LBrace) ++depth;
            if (tok.type == TokenType::RBrace) --depth;
        }
    }

    static void skipArray(JsonLexer& lexer) {
        int depth = 1;
        while (depth > 0) {
            auto tok = lexer.next();
            if (tok.type == TokenType::Eof) return;
            if (tok.type == TokenType::LBracket) ++depth;
            if (tok.type == TokenType::RBracket) --depth;
        }
    }

    static bool createEvent(const std::string& type,
                            IoxEvent& outEvent,
                            std::vector<Diagnostic>& diags)
    {
        // For Phase 1 minimal implementation: create empty events
        // of the correct type based on the "type" field.
        // Full deserialization with all fields is done in Phase 8+.
        if (type == "StartTransfer") {
            outEvent = StartTransferEvent{};
        } else if (type == "StartBasket") {
            outEvent = StartBasketEvent{};
        } else if (type == "Object") {
            outEvent = ObjectEvent{};
        } else if (type == "EndBasket") {
            outEvent = EndBasketEvent{};
        } else if (type == "EndTransfer") {
            outEvent = EndTransferEvent{};
        } else {
            diags.push_back({Diagnostic::Severity::Error,
                ErrorCode::JsonParseError,
                "Unknown event type: " + type});
            return false;
        }
        return true;
    }
};

} // anonymous namespace

// ============================================================================
// JsonEventReader::Impl
// ============================================================================

struct JsonEventReader::Impl {
    std::string buffer;
    std::size_t lineStart = 0;
    bool finished = false;
    std::vector<Diagnostic> diagnostics;
    JsonEventParser parser;
    bool eof = false;
};

JsonEventReader::JsonEventReader()
    : impl_(std::make_unique<Impl>()) {}

JsonEventReader::~JsonEventReader() = default;

void JsonEventReader::feed(ByteView data) {
    if (impl_->finished) return;
    impl_->buffer.append(data.data(), data.size());
}

void JsonEventReader::finish() {
    impl_->finished = true;
}

bool JsonEventReader::isFinished() const noexcept {
    return impl_->eof;
}

ReadOutcome JsonEventReader::next() {
    ReadOutcome outcome;

    if (impl_->eof) {
        outcome.status = ReadOutcome::Status::End;
        return outcome;
    }

    // Extract one line
    auto& buf = impl_->buffer;
    auto pos = impl_->lineStart;

    while (pos < buf.size()) {
        if (buf[pos] == '\n') {
            auto line = std::string_view(buf).substr(impl_->lineStart,
                                                     pos - impl_->lineStart);
            impl_->lineStart = pos + 1;

            IoxEvent event;
            if (impl_->parser.feedLine(line, event, impl_->diagnostics)) {
                outcome.status = ReadOutcome::Status::Event;
                outcome.event = std::move(event);
                outcome.diagnostics = std::move(impl_->diagnostics);
                impl_->diagnostics.clear();
                return outcome;
            }
            // Line was invalid — advance pos past the newline and continue scanning
            pos = impl_->lineStart;
            continue;
        }
        ++pos;
    }

    // No complete line found
    if (impl_->finished) {
        impl_->eof = true;
        outcome.status = ReadOutcome::Status::End;
    } else {
        outcome.status = ReadOutcome::Status::NeedInput;
    }
    return outcome;
}

std::vector<Diagnostic> JsonEventReader::takeDiagnostics() {
    auto diags = std::move(impl_->diagnostics);
    impl_->diagnostics.clear();
    return diags;
}

} // namespace json
} // namespace iox
