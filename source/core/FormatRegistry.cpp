#include "iox/FormatRegistry.h"
#include <algorithm>
#include <cctype>

namespace {

std::string lowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        result.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    return result;
}

bool extensionMatches(const std::string& left, std::string_view right) {
    return lowerAscii(left) == lowerAscii(right);
}

} // namespace

namespace iox {

void FormatRegistry::addFormat(FormatEntry entry) {
    auto it = std::find_if(formats_.begin(), formats_.end(),
        [&](const FormatEntry& e) { return e.name == entry.name; });
    if (it != formats_.end()) {
        *it = std::move(entry);
    } else {
        formats_.push_back(std::move(entry));
    }
}

bool FormatRegistry::removeFormat(std::string_view name) {
    auto it = std::find_if(formats_.begin(), formats_.end(),
        [name](const FormatEntry& e) { return e.name == name; });
    if (it != formats_.end()) {
        formats_.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> FormatRegistry::formatNames() const {
    std::vector<std::string> names;
    names.reserve(formats_.size());
    for (const auto& f : formats_) {
        names.push_back(f.name);
    }
    return names;
}

const FormatEntry* FormatRegistry::findByName(std::string_view name) const {
    auto it = std::find_if(formats_.begin(), formats_.end(),
        [name](const FormatEntry& e) { return e.name == name; });
    return it != formats_.end() ? &*it : nullptr;
}

std::unique_ptr<Reader> FormatRegistry::createReader(
    std::string_view formatName) const
{
    auto* entry = findByName(formatName);
    if (!entry || !entry->canRead || !entry->readerFactory) return nullptr;
    return entry->readerFactory();
}

std::unique_ptr<Reader> FormatRegistry::createReaderBySniffing(
    ByteView firstChunk,
    std::string_view extensionHint) const
{
    // Content scores always outrank extension hints. Registration order is the
    // deterministic tie-breaker for equal scores.
    const FormatEntry* sniffed = nullptr;
    int bestScore = 0;
    for (const auto& f : formats_) {
        if (!f.canRead || !f.readerFactory) continue;
        int score = 0;
        if (f.scoreSniffer) {
            score = std::max(0, std::min(100, f.scoreSniffer(firstChunk)));
        } else if (f.sniffer) {
            auto name = f.sniffer(firstChunk);
            score = name == f.name ? 100 : (name.empty() ? 0 : 50);
        }
        if (score > bestScore) {
            sniffed = &f;
            bestScore = score;
        }
    }

    if (sniffed && sniffed->canRead && sniffed->readerFactory) {
        return sniffed->readerFactory();
    }

    // Try extension match
    if (!extensionHint.empty()) {
        for (const auto& f : formats_) {
            for (const auto& ext : f.extensions) {
                if (f.canRead && f.readerFactory &&
                    extensionMatches(ext, extensionHint)) {
                    return f.readerFactory();
                }
            }
        }
    }

    return nullptr;
}

std::unique_ptr<Writer> FormatRegistry::createWriter(
    std::string_view formatName,
    std::shared_ptr<OutputSink> output) const
{
    auto* entry = findByName(formatName);
    if (!entry || !entry->canWrite || !entry->writerFactory) return nullptr;
    return entry->writerFactory(std::move(output));
}

} // namespace iox
