#include "iox/ByteView.h"
#include "iox/xtf/XtfReader.h"

#include <cstddef>
#include <cstdint>

#ifdef IOX_STANDALONE_FUZZ
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    iox::xtf::XtfReader reader;
    reader.feed(iox::ByteView(data, size));
    reader.finish();
    while (true) {
        const auto outcome = reader.next();
        if (outcome.status == iox::ReadOutcome::Status::End ||
            outcome.status == iox::ReadOutcome::Status::NeedInput) {
            break;
        }
    }
    (void)reader.takeDiagnostics();
    return 0;
}

#ifdef IOX_STANDALONE_FUZZ
int main(int argc, char** argv) {
    std::size_t runs = 1;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--runs" && i + 1 < argc) {
            runs = static_cast<std::size_t>(std::stoull(argv[++i]));
        } else {
            paths.push_back(arg);
        }
    }

    std::vector<std::uint8_t> seed;
    for (const auto& path : paths) {
        std::ifstream input(path, std::ios::binary);
        seed.assign(std::istreambuf_iterator<char>(input), {});
        LLVMFuzzerTestOneInput(seed.data(), seed.size());
        for (std::size_t run = 1; run < runs; ++run) {
            auto mutated = seed;
            if (!mutated.empty()) {
                mutated[run % mutated.size()] ^= static_cast<std::uint8_t>(run * 31U);
            }
            LLVMFuzzerTestOneInput(mutated.data(), mutated.size());
        }
    }
    if (paths.empty()) {
        for (std::size_t run = 0; run < runs; ++run) {
            std::vector<std::uint8_t> input(run % 17U, static_cast<std::uint8_t>(run));
            LLVMFuzzerTestOneInput(input.data(), input.size());
        }
    }
    std::cout << "standalone fuzz runs: " << (paths.empty() ? runs : runs * paths.size()) << "\n";
    return 0;
}
#endif
