/// iox-dump — Diagnose- und Beispieltool für INTERLIS XTF Dateien
///
/// Usage:
///   iox-dump input.xtf                  # Liest XTF und gibt Events als Text aus
///   iox-dump --events input.xtf         # Gibt Events als NDJSON aus
///   iox-dump --roundtrip input.xtf out.xtf  # Liest und schreibt neu

#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/json/JsonEventWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstring>

namespace {

enum class Mode { Print, Events, Roundtrip };

void printUsage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog << " input.xtf                  # Print events as text\n";
    std::cerr << "  " << prog << " --events input.xtf         # Output events as NDJSON\n";
    std::cerr << "  " << prog << " --roundtrip input.xtf out.xtf  # Read and rewrite\n";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << path << "\n";
        std::exit(1);
    }
    auto size = file.tellg();
    file.seekg(0);
    std::string data(static_cast<std::size_t>(size), '\0');
    file.read(&data[0], size);
    return data;
}

void writeFile(const std::string& path, const std::string& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot write file: " << path << "\n";
        std::exit(1);
    }
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void printEvents(const std::vector<iox::IoxEvent>& events) {
    for (const auto& event : events) {
        std::cout << iox::eventTypeName(event);
        std::visit([](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, iox::StartTransferEvent>) {
                std::cout << " sender=" << e.sender
                          << " version=" << (e.version ? std::to_string(*e.version) : "?");
            } else if constexpr (std::is_same_v<T, iox::StartBasketEvent>) {
                std::cout << " bid=" << e.bid
                          << " type=" << e.basketType.iliName()
                          << " consistency=" << e.consistency;
            } else if constexpr (std::is_same_v<T, iox::ObjectEvent>) {
                std::cout << " tid=" << e.objectId
                          << " class=" << e.object.tag().iliName()
                          << " attrs=" << e.object.attributeCount();
            } else if constexpr (std::is_same_v<T, iox::EndBasketEvent>) {
                std::cout << " bid=" << e.bid;
            }
        }, event);
        std::cout << "\n";
    }
}

void printEventsJson(const std::vector<iox::IoxEvent>& events) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::json::JsonEventWriter writer(sink);
    for (const auto& e : events) writer.write(e);
    writer.close();
    std::cout << sink->str();
}

std::vector<iox::IoxEvent> readXtf(const std::string& data) {
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(data.data(), data.size()));
    reader.finish();

    std::vector<iox::IoxEvent> events;
    while (true) {
        auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End) break;
        if (outcome.status == iox::ReadOutcome::Status::NeedInput) break;
        if (outcome.event) events.push_back(std::move(*outcome.event));
    }

    // Print any diagnostics
    auto diags = reader.takeDiagnostics();
    for (const auto& d : diags) {
        std::cerr << "[" << (d.severity == iox::Diagnostic::Severity::Fatal ? "FATAL" :
                              d.severity == iox::Diagnostic::Severity::Error ? "ERROR" : "WARN")
                  << "] " << d.message << "\n";
    }
    return events;
}

std::string writeXtf(const std::vector<iox::IoxEvent>& events, bool isXtf24) {
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = isXtf24 ? iox::xtf::XtfVersion::Xtf24 : iox::xtf::XtfVersion::Xtf23;
    opts.pretty = true;
    iox::xtf::XtfWriter writer(sink, opts);
    for (const auto& e : events) writer.write(e);
    writer.close();
    return sink->str();
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    Mode mode = Mode::Print;
    std::string inputPath;
    std::string outputPath;

    int argIdx = 1;
    while (argIdx < argc) {
        std::string arg(argv[argIdx]);
        if (arg == "--events") {
            mode = Mode::Events;
        } else if (arg == "--roundtrip") {
            mode = Mode::Roundtrip;
            if (argIdx + 2 >= argc) {
                std::cerr << "Error: --roundtrip requires input and output files\n";
                return 1;
            }
            inputPath = argv[++argIdx];
            outputPath = argv[++argIdx];
            break;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            inputPath = arg;
        }
        ++argIdx;
    }

    if (inputPath.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    // Read input
    auto data = readFile(inputPath);
    if (data.empty()) {
        std::cerr << "Error: Empty input file\n";
        return 1;
    }

    // Parse XTF
    auto events = readXtf(data);

    if (events.empty()) {
        std::cerr << "No events parsed from input.\n";
        return 1;
    }

    std::cerr << "Parsed " << events.size() << " events from " << inputPath << "\n";

    switch (mode) {
    case Mode::Print:
        printEvents(events);
        break;
    case Mode::Events:
        printEventsJson(events);
        break;
    case Mode::Roundtrip: {
        // Detect version from first event
        bool is24 = false;
        if (auto* st = std::get_if<iox::StartTransferEvent>(&events[0])) {
            is24 = st->version && *st->version == 24;
        }
        auto output = writeXtf(events, is24);
        writeFile(outputPath, output);
        std::cerr << "Wrote " << output.size() << " bytes to " << outputPath << "\n";
        break;
    }
    }

    return 0;
}
