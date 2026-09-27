#pragma once

// Minimal test framework — no external dependencies
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace crux::test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int g_pass = 0;
inline int g_fail = 0;
inline std::string g_current_test;

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> fn) {
        test_registry().push_back({name, std::move(fn)});
    }
};

inline void check_impl(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "  FAIL: " << file << ":" << line
                  << " in [" << g_current_test << "]\n"
                  << "        " << expr << "\n";
    }
}

#define TEST(name) \
    static void test_##name(); \
    static crux::test::TestRegistrar reg_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(expr) crux::test::check_impl((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) crux::test::check_impl((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NE(a, b) crux::test::check_impl((a) != (b), #a " != " #b, __FILE__, __LINE__)

inline int run_all_tests() {
    std::cout << "\n\033[36m\033[1m═══ CRUX Test Suite ═══\033[0m\n\n";

    for (auto& tc : test_registry()) {
        g_current_test = tc.name;
        std::cout << "  \033[90m▸\033[0m " << tc.name << " ... " << std::flush;
        try {
            tc.func();
            std::cout << "\033[32m✓\033[0m\n";
        } catch (const std::exception& e) {
            ++g_fail;
            std::cout << "\033[31m✗ EXCEPTION: " << e.what() << "\033[0m\n";
        } catch (...) {
            ++g_fail;
            std::cout << "\033[31m✗ UNKNOWN EXCEPTION\033[0m\n";
        }
    }

    std::cout << "\n  \033[1mResults:\033[0m "
              << "\033[32m" << g_pass << " passed\033[0m, "
              << "\033[31m" << g_fail << " failed\033[0m\n\n";

    return g_fail > 0 ? 1 : 0;
}

} // namespace crux::test
