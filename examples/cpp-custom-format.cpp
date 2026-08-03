/// Example: register a small custom event format explicitly.
///
/// This format reuses the canonical NDJSON event stream but adds a simple
/// textual magic line. A real format can replace the two adapter classes
/// without changing the registry or event consumers.

#include "iox/FormatRegistry.h"
#include "iox/json/JsonEventReader.h"
#include "iox/json/JsonEventWriter.h"

#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr const char* MAGIC = "IOX-CUSTOM/1\n";

class CustomReader final : public iox::Reader {
public:
    iox::ReadOutcome next() override { return reader_.next(); }

    void feed(iox::ByteView data) override {
        if (!started_) {
            pending_.append(reinterpret_cast<const char*>(data.data()),
                            data.size());
            const auto magicSize = std::string(MAGIC).size();
            if (pending_.size() < magicSize) return;
            if (pending_.compare(0, magicSize, MAGIC) != 0) {
                diagnostics_.push_back({iox::DiagnosticSeverity::Fatal,
                    iox::DiagnosticCode::FormatUnknown,
                    "Missing IOX-CUSTOM/1 header", {}, {}});
                failed_ = true;
                return;
            }
            started_ = true;
            const auto rest = pending_.substr(magicSize);
            pending_.clear();
            if (!rest.empty()) reader_.feed(iox::ByteView(rest));
            return;
        }
        reader_.feed(data);
    }

    void finish() override {
        if (!started_ && !failed_) {
            feed(iox::ByteView{});
        }
        if (!failed_) reader_.finish();
    }

    bool isFinished() const noexcept override {
        return failed_ || reader_.isFinished();
    }

    std::vector<iox::Diagnostic> takeDiagnostics() override {
        auto result = std::move(diagnostics_);
        auto nested = reader_.takeDiagnostics();
        result.insert(result.end(), nested.begin(), nested.end());
        return result;
    }

private:
    iox::json::JsonEventReader reader_;
    std::string pending_;
    std::vector<iox::Diagnostic> diagnostics_;
    bool started_ = false;
    bool failed_ = false;
};

class CustomWriter final : public iox::Writer {
public:
    explicit CustomWriter(std::shared_ptr<iox::OutputSink> output)
        : output_(std::move(output)), writer_(output_) {
        output_->write(MAGIC, std::string(MAGIC).size());
    }

    void write(const iox::IoxEvent& event) override { writer_.write(event); }
    void flush() override { writer_.flush(); }
    void close() override { writer_.close(); output_->close(); }
    bool isClosed() const noexcept override { return writer_.isClosed(); }
    std::vector<iox::Diagnostic> takeDiagnostics() override {
        return writer_.takeDiagnostics();
    }

private:
    std::shared_ptr<iox::OutputSink> output_;
    iox::json::JsonEventWriter writer_;
};

void registerCustomFormat(iox::FormatRegistry& registry) {
    iox::FormatEntry entry;
    entry.name = "custom-events";
    entry.description = "Magic-header NDJSON event stream";
    entry.extensions = {".custom"};
    entry.scoreSniffer = [](iox::ByteView prefix) {
        if (prefix.empty()) return 0;
        const std::string_view view(
            reinterpret_cast<const char*>(prefix.data()), prefix.size());
        return view.find(MAGIC) == 0 ? 100 : 0;
    };
    entry.readerFactory = [] { return std::make_unique<CustomReader>(); };
    entry.writerFactory = [](std::shared_ptr<iox::OutputSink> output) {
        return std::make_unique<CustomWriter>(std::move(output));
    };
    registry.addFormat(std::move(entry));
}

} // namespace

int main() {
    iox::FormatRegistry registry;
    registerCustomFormat(registry);

    auto sink = std::make_shared<iox::StringOutputSink>();
    auto writer = registry.createWriter("custom-events", sink);
    writer->write(iox::StartTransferEvent{});
    writer->write(iox::EndTransferEvent{});
    writer->close();

    auto reader = registry.createReaderBySniffing(
        iox::ByteView(sink->str()), ".wrong-extension");
    reader->feed(iox::ByteView(sink->str()));
    reader->finish();
    int eventCount = 0;
    while (reader->next().progress != iox::ReaderProgress::End) {
        ++eventCount;
    }
    std::cout << "custom format roundtrip events=" << eventCount << '\n';
    return eventCount == 2 ? 0 : 1;
}
