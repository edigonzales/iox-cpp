/// Example: Read XTF, write back (roundtrip)
///
/// Usage: ./cpp-roundtrip <input.xtf> <output.xtf>

#include "iox/xtf/XtfReader.h"
#include "iox/xtf/XtfWriter.h"
#include "iox/Events.h"
#include "iox/Writer.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.xtf> <output.xtf>\n";
        return 1;
    }

    // Read input
    std::ifstream inFile(argv[1], std::ios::binary | std::ios::ate);
    if (!inFile) { std::cerr << "Cannot open: " << argv[1] << "\n"; return 1; }
    auto size = inFile.tellg();
    inFile.seekg(0);
    std::string data(static_cast<std::size_t>(size), '\0');
    inFile.read(&data[0], size);

    // Parse
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

    std::cout << "Read " << events.size() << " events\n";
    std::cout << "XTF version: " << iox::xtf::toString(reader.detectedVersion()) << "\n";

    // Determine version
    bool is24 = false;
    if (!events.empty()) {
        if (auto* st = std::get_if<iox::StartTransferEvent>(&events[0])) {
            is24 = st->version && *st->version == 24;
        }
    }

    // Write output
    auto sink = std::make_shared<iox::StringOutputSink>();
    iox::xtf::XtfWriterOptions opts;
    opts.version = is24 ? iox::xtf::XtfVersion::Xtf24 : iox::xtf::XtfVersion::Xtf23;
    opts.pretty = true;
    iox::xtf::XtfWriter writer(sink, opts);

    for (const auto& e : events) writer.write(e);
    writer.close();

    // Write to file
    std::ofstream outFile(argv[2], std::ios::binary);
    if (!outFile) { std::cerr << "Cannot write: " << argv[2] << "\n"; return 1; }
    auto& outStr = sink->str();
    outFile.write(outStr.data(), static_cast<std::streamsize>(outStr.size()));
    outFile.close();

    std::cout << "Wrote " << outStr.size() << " bytes to " << argv[2] << "\n";

    // Diagnostics
    for (auto& d : reader.takeDiagnostics()) {
        std::cerr << "DIAG: " << d.message << "\n";
    }

    return 0;
}
