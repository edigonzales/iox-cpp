#pragma once

#include "iox/Reader.h"
#include "iox/Writer.h"

#include <string>
#include <string_view>
#include <memory>
#include <functional>
#include <vector>

namespace iox {

/// Tries to detect the format of byte content by inspecting its prefix.
using FormatSniffer = std::function<std::string(ByteView firstChunk)>;

/// Optional score-based sniffer. Scores are in [0, 100].
using ScoredFormatSniffer = std::function<int(ByteView firstChunk)>;

/// Factory for creating a Reader. Takes no arguments.
using ReaderCreator = std::function<std::unique_ptr<Reader>()>;

/// Factory for creating a Writer. Takes an OutputSink.
using WriterCreator = std::function<std::unique_ptr<Writer>(
    std::shared_ptr<OutputSink>)>;

/// Registration entry for a format.
struct FormatEntry final {
    std::string name;
    std::string description;
    std::vector<std::string> extensions;
    std::vector<std::string> mimeTypes;
    bool canRead = true;
    bool canWrite = true;
    FormatSniffer sniffer;
    ScoredFormatSniffer scoreSniffer;
    ReaderCreator readerFactory;
    WriterCreator writerFactory;
};

/// Explicit, testable format registry.
class FormatRegistry final {
public:
    FormatRegistry() = default;

    void addFormat(FormatEntry entry);
    bool removeFormat(std::string_view name);
    std::vector<std::string> formatNames() const;
    const FormatEntry* findByName(std::string_view name) const;

    std::unique_ptr<Reader> createReader(
        std::string_view formatName) const;

    std::unique_ptr<Reader> createReaderBySniffing(
        ByteView firstChunk,
        std::string_view extensionHint = "") const;

    std::unique_ptr<Writer> createWriter(
        std::string_view formatName,
        std::shared_ptr<OutputSink> output) const;

private:
    std::vector<FormatEntry> formats_;
};

} // namespace iox
