#include "iox/IomValue.h"
#include <string>
#include <cstdio>

namespace iox {

std::string IomValue::toTransferString() const {
    return std::visit(
        [](const auto& val) -> std::string {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, NullTag>) {
                return "";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return val;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(val);
            } else if constexpr (std::is_same_v<T, double>) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.15g", val);
                return std::string(buf);
            } else if constexpr (std::is_same_v<T, bool>) {
                return val ? "true" : "false";
            }
        },
        data_);
}

} // namespace iox
