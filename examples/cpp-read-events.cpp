/// Example: Read XTF and print events
///
/// Build: Part of the iox-cpp examples (cmake -DIOX_BUILD_EXAMPLES=ON)
/// Usage: ./cpp-read-events <input.xtf>

#include "iox/xtf/XtfReader.h"
#include "iox/Events.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.xtf>\n";
        return 1;
    }

    // Read file
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open: " << argv[1] << "\n";
        return 1;
    }
    auto size = file.tellg();
    file.seekg(0);
    std::string data(static_cast<std::size_t>(size), '\0');
    file.read(&data[0], size);

    // Create reader
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(data));
    reader.finish();

    std::cout << "XTF version: "
              << (reader.detectedVersion()
                      ? iox::xtf::toString(*reader.detectedVersion())
                      : "unknown")
              << "\n\n";

    // Read events
    int count = 0;
    while (true) {
        auto outcome = reader.next();
        if (outcome.progress == iox::ReaderProgress::End) break;
        if (outcome.progress == iox::ReaderProgress::NeedInput) break;
        if (outcome.event) {
            ++count;
            std::cout << "[" << count << "] "
                      << iox::eventKindName(iox::eventKind(*outcome.event))
                      << "\n";
        }
    }

    // Diagnostics
    for (auto& d : reader.takeDiagnostics()) {
        std::cerr << "DIAG: " << d.message << "\n";
    }

    std::cout << "\nTotal: " << count << " events\n";
    return 0;
}
