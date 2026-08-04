#include "iox/IomObject.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) {
    iox::IomObject object(iox::IomName("Fuzz.Topic.Class"), "o1");
    const auto limit = std::min<std::size_t>(size, 512U);
    try {
        for (std::size_t index = 0; index < limit; ++index) {
            const std::string attribute =
                "a" + std::to_string(data[index] % 8U);
            const std::string value(1U,
                static_cast<char>(' ' + data[index] % 95U));
            switch (data[index] % 7U) {
            case 0: object.setPrimitive(iox::IomName(attribute), value); break;
            case 1: object.appendPrimitive(iox::IomName(attribute), value); break;
            case 2:
                object.setObject(iox::IomName(attribute),
                    iox::IomObject(iox::IomName("Fuzz.Struct")));
                break;
            case 3: object.eraseAttribute(attribute); break;
            case 4:
                if (object.valueCount(attribute) != 0U) {
                    object.eraseValue(attribute, 0U);
                }
                break;
            case 5: (void)object.deepCopy(); break;
            case 6: (void)object.semanticallyEquals(object.deepCopy()); break;
            }
        }
    } catch (const iox::IoxError&) {
        // Index and cycle errors are valid API outcomes for generated calls.
    }
    return 0;
}

#include "FuzzMain.h"
