// Include this AFTER all IOX_TEST definitions to generate main().
// IOX_TEST_NO_MAIN lets the coverage build combine the test translation units.

#ifndef IOX_TEST_NO_MAIN
int main() {
    int passed = 0;
    int failed = 0;
    for (int i = 0; i < _iox_g_testCount; ++i) {
        std::cout << "  [" << (i + 1) << "/" << _iox_g_testCount
                  << "] " << _iox_g_tests[i].name << "... ";
        try {
            _iox_g_tests[i].func();
            std::cout << "PASSED\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << e.what() << "\n";
            ++failed;
        } catch (...) {
            std::cout << "FAILED: unknown exception\n";
            ++failed;
        }
    }
    std::cout << "\n" << passed << " passed, "
              << failed << " failed\n";
    delete[] _iox_g_tests;
    return failed > 0 ? 1 : 0;
}
#endif
