#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

inline void reportFailure(const char* expr, const char* file, int line) {
    ++failures();
    std::cerr << "  FAILED: " << expr << " (" << file << ":" << line << ")\n";
}

}  // namespace testing

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                                   \
    static void name();                                                              \
    static ::testing::Registrar TEST_CONCAT(registrar_, name)(#name, &name);         \
    static void name()

#define CHECK(expr)                                                                  \
    do {                                                                             \
        if (!(expr)) ::testing::reportFailure(#expr, __FILE__, __LINE__);            \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
