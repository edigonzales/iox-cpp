#pragma once

#include <iostream>
#include <string>
#include <cstdlib>

namespace iox {
namespace test {

inline void fail(const char* file, int line, std::string message) {
    std::cerr << file << ":" << line << ": FAIL: " << message << "\n";
    std::abort();
}

} // namespace test
} // namespace iox

#define IOX_CHECK(expr)                                                \
    do {                                                               \
        if (!(expr)) {                                                 \
            iox::test::fail(__FILE__, __LINE__, "CHECK(" #expr ")");   \
        }                                                              \
    } while (false)

#define IOX_CHECK_EQ(expected, actual)                                 \
    do {                                                               \
        auto _e = (expected);                                          \
        auto _a = (actual);                                            \
        if (!(_e == _a)) {                                             \
            iox::test::fail(__FILE__, __LINE__,                        \
                "CHECK_EQ(" #expected ", " #actual ")");               \
        }                                                              \
    } while (false)

// ---- Test registration infrastructure ----

using _IoxTestFunc = void(*)();

struct _IoxTestEntry {
    const char* name;
    _IoxTestFunc func;
};

inline _IoxTestEntry* _iox_g_tests = nullptr;
inline int _iox_g_testCount = 0;
inline int _iox_g_testCapacity = 0;

inline void _iox_registerTest(const char* name, _IoxTestFunc func) {
    if (_iox_g_testCount == _iox_g_testCapacity) {
        int newCap = _iox_g_testCapacity == 0 ? 64 : _iox_g_testCapacity * 2;
        auto* newTests = new _IoxTestEntry[newCap];
        for (int i = 0; i < _iox_g_testCount; ++i) newTests[i] = _iox_g_tests[i];
        delete[] _iox_g_tests;
        _iox_g_tests = newTests;
        _iox_g_testCapacity = newCap;
    }
    _iox_g_tests[_iox_g_testCount].name = name;
    _iox_g_tests[_iox_g_testCount].func = func;
    ++_iox_g_testCount;
}

#define IOX_TEST(name)                                                 \
    static void _iox_test_##name();                                    \
    static struct _iox_reg_##name {                                    \
        _iox_reg_##name() { _iox_registerTest(#name, _iox_test_##name); } \
    } _iox_reg_inst_##name;                                            \
    static void _iox_test_##name()
