#include <smart/shared_ptr.hpp>

#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "test.hpp"

using smart::makeShared;
using smart::SharedPtr;

namespace {

struct Counted {
    static inline int alive = 0;
    int value;
    explicit Counted(int v = 0) : value(v) { ++alive; }
    virtual ~Counted() { --alive; }
};

struct Derived : Counted {
    static inline int derivedAlive = 0;
    explicit Derived(int v) : Counted(v) { ++derivedAlive; }
    ~Derived() override { --derivedAlive; }
};

}  // namespace

TEST(shared_ptr_default_and_null) {
    SharedPtr<int> a;
    SharedPtr<int> b(nullptr);
    CHECK(!a);
    CHECK(a.get() == nullptr);
    CHECK_EQ(a.use_count(), 0);
    CHECK(a == nullptr);
    CHECK(a == b);
}

TEST(shared_ptr_owns_and_destroys_once) {
    Counted::alive = 0;
    {
        SharedPtr<Counted> p(new Counted(5));
        CHECK_EQ(Counted::alive, 1);
        CHECK_EQ(p.use_count(), 1);
        CHECK_EQ(p->value, 5);
        CHECK_EQ((*p).value, 5);
        {
            SharedPtr<Counted> q = p;
            CHECK_EQ(p.use_count(), 2);
            CHECK(p == q);
        }
        CHECK_EQ(p.use_count(), 1);
        CHECK_EQ(Counted::alive, 1);
    }
    CHECK_EQ(Counted::alive, 0);
}

TEST(shared_ptr_move) {
    Counted::alive = 0;
    SharedPtr<Counted> a = makeShared<Counted>(1);
    SharedPtr<Counted> b = std::move(a);
    CHECK(!a);
    CHECK_EQ(b.use_count(), 1);
    SharedPtr<Counted> c;
    c = std::move(b);
    CHECK(!b);
    CHECK_EQ(c.use_count(), 1);
    CHECK_EQ(Counted::alive, 1);
    c = nullptr;
    CHECK_EQ(Counted::alive, 0);
}

TEST(shared_ptr_copy_assignment_and_self_assignment) {
    Counted::alive = 0;
    SharedPtr<Counted> a = makeShared<Counted>(1);
    SharedPtr<Counted> b = makeShared<Counted>(2);
    CHECK_EQ(Counted::alive, 2);
    b = a;
    CHECK_EQ(Counted::alive, 1);
    CHECK_EQ(a.use_count(), 2);
    auto& alias = a;
    a = alias;
    CHECK_EQ(a.use_count(), 2);
    a = std::move(alias);
    CHECK(a);
    CHECK_EQ(a->value, 1);
}

TEST(shared_ptr_reset_and_swap) {
    Counted::alive = 0;
    SharedPtr<Counted> a = makeShared<Counted>(1);
    SharedPtr<Counted> b = makeShared<Counted>(2);
    a.swap(b);
    CHECK_EQ(a->value, 2);
    CHECK_EQ(b->value, 1);
    swap(a, b);
    CHECK_EQ(a->value, 1);
    a.reset(new Counted(3));
    CHECK_EQ(a->value, 3);
    CHECK_EQ(Counted::alive, 2);
    a.reset();
    CHECK(!a);
    CHECK_EQ(Counted::alive, 1);
}

TEST(shared_ptr_derived_to_base) {
    Counted::alive = 0;
    Derived::derivedAlive = 0;
    {
        SharedPtr<Derived> d(new Derived(7));
        SharedPtr<Counted> base = d;
        CHECK_EQ(base.use_count(), 2);
        CHECK(base == d);
        SharedPtr<const Counted> constBase = base;
        CHECK_EQ(constBase->value, 7);
        d.reset();
        CHECK_EQ(Derived::derivedAlive, 1);
    }
    CHECK_EQ(Derived::derivedAlive, 0);
    CHECK_EQ(Counted::alive, 0);
}

TEST(shared_ptr_comparisons_and_hash) {
    SharedPtr<int> a = makeShared<int>(1);
    SharedPtr<int> b = makeShared<int>(2);
    SharedPtr<int> a2 = a;
    CHECK(a == a2);
    CHECK(a != b);
    CHECK((a < b) != (b < a));
    CHECK((a <= a2) && (a >= a2));
    CHECK(a != nullptr);
    CHECK(nullptr != a);
    std::unordered_set<SharedPtr<int>> set{a, a2, b};
    CHECK_EQ(set.size(), 2u);
}

TEST(shared_ptr_make_shared_forwards_args) {
    auto s = makeShared<std::string>(3, 'x');
    CHECK_EQ(*s, "xxx");
    s->push_back('y');
    CHECK_EQ(s->size(), 4u);
}

TEST(shared_ptr_multithreaded_copies) {
    Counted::alive = 0;
    {
        SharedPtr<Counted> shared = makeShared<Counted>(42);
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([shared] {
                for (int i = 0; i < 100000; ++i) {
                    SharedPtr<Counted> local = shared;
                    SharedPtr<Counted> moved = std::move(local);
                    if (moved->value != 42) std::abort();
                }
            });
        }
        for (auto& th : threads) th.join();
        CHECK_EQ(shared.use_count(), 1);
        CHECK_EQ(Counted::alive, 1);
    }
    CHECK_EQ(Counted::alive, 0);
}
