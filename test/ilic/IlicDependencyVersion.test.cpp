#include "ilic/Compiler.h"
#include "iox/test/Test.h"

#include <string>

#ifndef IOX_EXPECTED_ILIC_VERSION
#error IOX_EXPECTED_ILIC_VERSION must be defined
#endif

IOX_TEST(ilic_dependency_version_matches_release_contract) {
    const std::string expected = IOX_EXPECTED_ILIC_VERSION;
    const std::string actual = ilic::version();
    if (expected != actual) {
        iox::test::fail(
            __FILE__,
            __LINE__,
            "expected ilic runtime version '" + expected + "', got '" + actual + "'"
        );
    }
}

#include "iox/test/TestMain.h"
