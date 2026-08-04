#include "ilic/Compiler.h"
#include "iox/test/Test.h"

#include <string>

#ifndef IOX_EXPECTED_ILIC_VERSION
#error IOX_EXPECTED_ILIC_VERSION must be defined
#endif

IOX_TEST(ilic_dependency_version_matches_release_contract) {
    IOX_CHECK_EQ(
        std::string(IOX_EXPECTED_ILIC_VERSION),
        std::string(ilic::version())
    );
}

#include "iox/test/TestMain.h"
