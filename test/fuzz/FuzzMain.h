#ifdef IOX_STANDALONE_FUZZ

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::size_t runs = 1;
    std::vector<std::string> paths;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--runs" && index + 1 < argc) {
            runs = static_cast<std::size_t>(std::stoull(argv[++index]));
        } else {
            paths.push_back(argument);
        }
    }

    std::size_t executed = 0;
    for (const auto& path : paths) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            std::cerr << "cannot open fuzz seed: " << path << '\n';
            return 2;
        }
        std::vector<std::uint8_t> seed(
            std::istreambuf_iterator<char>(input), {});
        for (std::size_t run = 0; run < runs; ++run) {
            auto mutated = seed;
            if (run != 0U && !mutated.empty()) {
                mutated[run % mutated.size()] ^=
                    static_cast<std::uint8_t>(run * 31U);
            }
            LLVMFuzzerTestOneInput(mutated.data(), mutated.size());
            ++executed;
        }
    }
    if (paths.empty()) {
        for (std::size_t run = 0; run < runs; ++run) {
            std::vector<std::uint8_t> input(
                run % 17U, static_cast<std::uint8_t>(run));
            LLVMFuzzerTestOneInput(input.data(), input.size());
            ++executed;
        }
    }
    std::cout << "standalone fuzz runs: " << executed << '\n';
    return 0;
}

#endif
