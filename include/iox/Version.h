#pragma once

#include <cstdint>

namespace iox {

/// Return the ABI version (a positive integer).
/// Incremented when the C ABI surface changes incompatibly.
constexpr std::uint32_t abiVersion() noexcept { return 2; }

/// Return the iox-cpp library version string.
const char* version() noexcept;

} // namespace iox
