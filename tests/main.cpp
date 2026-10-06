#include <cstring>

#include "test.hpp"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failedTests = 0;
    for (const auto& test : testing::registry()) {
        if (filter && test.name.find(filter) == std::string::npos) continue;
        int before = testing::failures();
        std::cout << "[ RUN  ] " << test.name << std::endl;
        test.body();
        ++run;
        if (testing::failures() != before) {
            ++failedTests;
            std::cout << "[ FAIL ] " << test.name << std::endl;
        }
    }
    std::cout << "\n" << run - failedTests << "/" << run << " tests passed";
    if (failedTests) std::cout << ", " << testing::failures() << " checks failed";
    std::cout << std::endl;
    return failedTests == 0 ? 0 : 1;
}
