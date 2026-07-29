#pragma once

#include "iox/FormatRegistry.h"

namespace iox {

/// Thread-safe registry containing the official built-in formats.
FormatRegistry& defaultFormatRegistry();

/// Convenience facade selecting a reader by explicit format or source name.
class ReaderFactory final {
public:
    static std::unique_ptr<Reader> create(
        std::string_view sourceName, ByteView prefix = {});
    static std::unique_ptr<Reader> createByName(std::string_view formatName);
};

/// Convenience facade selecting a writer by explicit format name.
class WriterFactory final {
public:
    static std::unique_ptr<Writer> create(
        std::string_view formatName, std::shared_ptr<OutputSink> output);
};

} // namespace iox
