#include "iox/FormatRegistry.h"
#include <algorithm>

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
    if (!entry || !entry->readerFactory) return nullptr;
    return entry->readerFactory();
}

std::unique_ptr<Reader> FormatRegistry::createReaderBySniffing(
    ByteView firstChunk,
    std::string_view extensionHint) const
{
    // Try sniffers first
    const FormatEntry* sniffed = nullptr;
    for (const auto& f : formats_) {
        if (f.sniffer) {
            auto name = f.sniffer(firstChunk);
            if (!name.empty()) {
                sniffed = &f;
                break;
            }
        }
    }

    if (sniffed) {
        if (!extensionHint.empty()) {
            for (const auto& ext : sniffed->extensions) {
                if (ext == extensionHint) {
                    return sniffed->readerFactory();
                }
            }
        }
        return sniffed->readerFactory();
    }

    // Try extension match
    if (!extensionHint.empty()) {
        for (const auto& f : formats_) {
            for (const auto& ext : f.extensions) {
                if (ext == extensionHint) {
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
    if (!entry || !entry->writerFactory) return nullptr;
    return entry->writerFactory(std::move(output));
}

} // namespace iox
