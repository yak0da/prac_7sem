CXX      ?= clang++
BUILD    ?= build
OPT      ?= -O2 -g
SANFLAGS ?=

CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic $(OPT) $(SANFLAGS) -Iinclude -Ithird_party -pthread -MMD -MP
LDFLAGS  := -pthread $(SANFLAGS)
LDLIBS   := -lcurl

LIB_SRC  := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
TEST_SRC := $(wildcard tests/*.cpp)

LIB_OBJ  := $(LIB_SRC:%.cpp=$(BUILD)/%.o)
TEST_OBJ := $(TEST_SRC:%.cpp=$(BUILD)/%.o)
MAIN_OBJ := $(BUILD)/src/main.o

.PHONY: all test run asan tsan clean

all: $(BUILD)/mafia $(BUILD)/mafia_tests

$(BUILD)/mafia: $(MAIN_OBJ) $(LIB_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/mafia_tests: $(TEST_OBJ) $(LIB_OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(BUILD)/mafia_tests $(BUILD)/mafia
	./$(BUILD)/mafia_tests

run: $(BUILD)/mafia
	./$(BUILD)/mafia --players 10 --open-announcements

asan:
	$(MAKE) BUILD=build-asan OPT="-O1 -g -fno-omit-frame-pointer" \
		SANFLAGS="-fsanitize=address,undefined" test

tsan:
	$(MAKE) BUILD=build-tsan OPT="-O1 -g" SANFLAGS="-fsanitize=thread" all
	TSAN_OPTIONS=halt_on_error=1 ./build-tsan/mafia_tests
	for s in 1 2 3 4 5 6 7 8 9 10; do \
		TSAN_OPTIONS=halt_on_error=1 ./build-tsan/mafia --players 12 --seed $$s --engine thread \
			--no-file-log > /dev/null || exit 1; \
	done
	@echo "tsan: OK"

clean:
	rm -rf build build-asan build-tsan

-include $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(MAIN_OBJ:.o=.d)
