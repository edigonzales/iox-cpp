#include "iox/Version.h"
#include "iox/test/Test.h"

IOX_TEST(test_abi_version) {
    IOX_CHECK(iox::abiVersion() > 0);
}

IOX_TEST(test_version_string) {
    const char* v = iox::version();
    IOX_CHECK(v != nullptr);
    IOX_CHECK(v[0] != '\0');
}

#include "iox/test/TestMain.h"
