// Plan item 9.KK1: what a GameObject holds is cheap to copy.
//
// name / tags / domains are Interned<T> (core/interned.h); card_counters and
// string_state are KeyedSlots<V> (core/keyed_slots.h). These tests pin the
// behaviour card code relies on (they read and write like the std::string,
// the vectors and the maps they replaced) and the properties the change was
// made for (a copy shares, an edit does not leak into another object).
#include <gtest/gtest.h>

#include "core/game_object.h"
#include "core/game_state.h"
#include "core/interned.h"
#include "core/keyed_slots.h"

#include <sstream>
#include <thread>
#include <type_traits>

namespace riftbound::test::object_copy_cost {

// A copy of these members is a pointer copy: nothing to allocate or free.
static_assert(std::is_trivially_copyable_v<Interned<std::string>>);
static_assert(std::is_trivially_copyable_v<Interned<std::vector<std::string>>>);
static_assert(std::is_trivially_copyable_v<Interned<std::vector<Domain>>>);
static_assert(sizeof(Interned<std::string>) == sizeof(void*));
static_assert(sizeof(KeyedSlots<int>) == sizeof(void*));
static_assert(sizeof(KeyedSlots<std::string>) == sizeof(void*));
// The object was 744 bytes with a std::string, three vectors' worth of
// headers and two hash tables in it. A new member that pushes it past this
// is a new cost on every state copy: decide it knowingly.
static_assert(sizeof(GameObject) <= 600);
// A T and its interned form do not convert into each other silently in both
// directions (a conditional expression of the two is the T).
static_assert(!std::is_convertible_v<std::string, Interned<std::string>>);
static_assert(std::is_convertible_v<Interned<std::string>, const std::string&>);

TEST(ObjectCopyCost, ANameReadsAsAString) {
    GameObject o;
    EXPECT_TRUE(o.name.empty());
    EXPECT_EQ(o.name, "");
    o.name = "Sand Soldier";
    EXPECT_EQ(o.name, "Sand Soldier");
    EXPECT_EQ(o.name, std::string("Sand Soldier"));
    EXPECT_TRUE(o.name == std::string_view("Sand Soldier"));
    EXPECT_NE(o.name, "Recruit");
    EXPECT_EQ(o.name.size(), 12u);
    EXPECT_EQ("a " + o.name + " (" + o.name + ")", "a Sand Soldier (Sand Soldier)");
    EXPECT_EQ(o.name + '!', "Sand Soldier!");
    EXPECT_EQ(o.name.find("Sol"), 5u);
    EXPECT_EQ(o.name.substr(0, 4), "Sand");
    const std::string& as_string = o.name;
    EXPECT_EQ(as_string, "Sand Soldier");
    std::string copy = o.name;
    copy += "s";
    EXPECT_EQ(o.name, "Sand Soldier") << "a std::string made from a name is its own";
    std::ostringstream os;
    os << o.name;
    EXPECT_EQ(os.str(), "Sand Soldier");
    // With a literal in a conditional expression the name is read as a string.
    const std::string shown = o.name.empty() ? "(token)" : o.name.get();
    EXPECT_EQ(shown, "Sand Soldier");
}

TEST(ObjectCopyCost, EqualValuesAreOneSharedValue) {
    GameObject a, b;
    const std::string from_def = "Fiora, Peerless";
    a.name = from_def;
    b.name = std::string("Fiora, ") + "Peerless";   // built elsewhere, equal
    EXPECT_EQ(a.name, b.name);
    EXPECT_EQ(&a.name.get(), &b.name.get()) << "one pooled string for both";
    a.tags = std::vector<std::string>{"Demacia", "Elite"};
    b.tags = {"Demacia", "Elite"};
    EXPECT_EQ(&a.tags.get(), &b.tags.get());
    GameObject c = a;
    EXPECT_EQ(&c.name.get(), &a.name.get()) << "a copy shares; it allocates nothing";
    EXPECT_EQ(&c.tags.get(), &a.tags.get());
    EXPECT_EQ(&GameObject{}.domains.get(), &GameObject{}.domains.get()) << "one empty value";
}

TEST(ObjectCopyCost, AnEditOfOneObjectDoesNotShowInAnother) {
    GameObject a;
    a.tags = {"Poro"};
    a.domains = std::vector<Domain>{Domain::Fury};
    GameObject b = a;                       // shares a's values
    b.tags.push_back("Mech");               // Experimental Hexplate
    b.domains.push_back(Domain::Mind);
    b.name = "Renamed";
    EXPECT_EQ(a.tags.get(), (std::vector<std::string>{"Poro"}));
    EXPECT_EQ(b.tags.get(), (std::vector<std::string>{"Poro", "Mech"}));
    EXPECT_EQ(a.domains.size(), 1u);
    EXPECT_EQ(b.domains.size(), 2u);
    EXPECT_TRUE(a.name.empty());
    b.tags.edit([](std::vector<std::string>& tags) {
        tags.erase(std::remove(tags.begin(), tags.end(), std::string("Mech")), tags.end());
    });
    EXPECT_EQ(b.tags, a.tags);
    EXPECT_EQ(&b.tags.get(), &a.tags.get()) << "back to the shared value";
    b.tags.clear();
    EXPECT_TRUE(b.tags.empty());
    EXPECT_FALSE(a.tags.empty());
    // The vector face cards use.
    EXPECT_EQ(a.tags.front(), "Poro");
    EXPECT_EQ(a.tags[0], "Poro");
    EXPECT_NE(std::find(a.tags.begin(), a.tags.end(), "Poro"), a.tags.end());
    int seen = 0;
    for (const auto& t : a.tags) seen += t == "Poro";
    EXPECT_EQ(seen, 1);
}

TEST(ObjectCopyCost, CountersReadAndWriteLikeTheMapTheyWere) {
    GameObject o;
    auto& c = o.card_counters;
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(c.find("x"), c.end());
    EXPECT_EQ(c.count("x"), 0u);
    EXPECT_EQ(c["x"], 0) << "operator[] adds a zero entry";
    EXPECT_EQ(c.count("x"), 1u);
    EXPECT_FALSE(c.empty());
    c["x"] = 7;
    c["__defend_attacker_id"]++;
    EXPECT_EQ(c.at("x"), 7);
    EXPECT_EQ(c.find("__defend_attacker_id")->second, 1);
    EXPECT_THROW(c.at("missing"), std::out_of_range);
    EXPECT_EQ(c.size(), 2u);
    // A name is a name however it is spelled: a literal, a constant, a string
    // built at run time, the same characters at another address.
    static constexpr const char* kName = "x";
    const std::string built = std::string("") + "x";
    char buffer[8] = "x";
    EXPECT_EQ(c[kName], 7);
    EXPECT_EQ(c[built], 7);
    EXPECT_EQ(c[buffer], 7);
    buffer[0] = 'y';                        // same address, another name
    EXPECT_EQ(c.count(buffer), 0u);
    EXPECT_EQ(c.count("y"), 0u);
    EXPECT_EQ(c.erase("x"), 1u);
    EXPECT_EQ(c.erase("x"), 0u);
    EXPECT_EQ(c.count("x"), 0u);
    c.erase(c.find("__defend_attacker_id"));
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(slotKeyName(slotKey("__defend_attacker_id")), "__defend_attacker_id");
    EXPECT_EQ(slotKey("x"), slotKey(built));
    EXPECT_NE(slotKey("x"), slotKey("y"));
}

TEST(ObjectCopyCost, AReferenceToACounterSurvivesLaterInsertions) {
    // Tornado Warrior 886 and Evelynn 703 hold `auto& flag = counters["a"]`,
    // add another counter and then write through `flag`. The map guaranteed
    // that; so do the slots, however many are added.
    KeyedSlots<int> c;
    int& first = c["first"];
    first = 1;
    for (int i = 0; i < 100; ++i) c["k" + std::to_string(i)] = i;
    first = 42;
    EXPECT_EQ(c["first"], 42);
    EXPECT_EQ(&c["first"], &first);
    for (int i = 0; i < 100; i += 2) c.erase("k" + std::to_string(i));
    c["again"] = 5;                         // reuses a freed entry
    EXPECT_EQ(&c["first"], &first);
    EXPECT_EQ(c["first"], 42);
    EXPECT_EQ(c["k99"], 99);
    EXPECT_EQ(c.size(), 52u);
}

TEST(ObjectCopyCost, ACopyOfCountersIsItsOwn) {
    GameObject a;
    a.card_counters["n"] = 3;
    a.string_state["named_tag"] = "Mech";
    GameObject b = a;
    b.card_counters["n"] = 9;
    b.card_counters["m"] = 1;
    b.string_state["named_tag"] = "Poro";
    EXPECT_EQ(a.card_counters["n"], 3);
    EXPECT_EQ(a.card_counters.count("m"), 0u);
    EXPECT_EQ(a.string_state.find("named_tag")->second, "Mech");
    EXPECT_EQ(b.string_state.at("named_tag"), "Poro");
    a = b;
    EXPECT_EQ(a.card_counters, b.card_counters);
    EXPECT_EQ(a.card_counters["m"], 1);
    b.card_counters.clear();
    EXPECT_EQ(a.card_counters["n"], 9);
    EXPECT_EQ(b.string_state.erase("named_tag"), 1u);
    EXPECT_TRUE(b.string_state.empty());
}

TEST(ObjectCopyCost, ThreadsInternTheSameNamesToTheSameValues) {
    // Games run on a thread pool and each interns the same card names and
    // counter names; all must agree, whoever came first.
    constexpr int kThreads = 6, kNames = 200;
    std::vector<std::vector<SlotKey>> keys(kThreads);
    std::vector<std::vector<const std::string*>> names(kThreads);
    {
        std::vector<std::thread> pool;
        for (int t = 0; t < kThreads; ++t) {
            pool.emplace_back([&, t] {
                for (int round = 0; round < 20; ++round) {
                    for (int i = 0; i < kNames; ++i) {
                        const std::string s = "__thread_test_" + std::to_string((i * 7 + t) % kNames);
                        const SlotKey key = slotKey(s);
                        Interned<std::string> name;
                        name = s;
                        if (round == 0) {
                            keys[t].push_back(key);
                            names[t].push_back(&name.get());
                        }
                        ASSERT_EQ(slotKeyName(key), s);
                        ASSERT_EQ(name, s);
                    }
                }
            });
        }
        for (auto& th : pool) th.join();
    }
    for (int i = 0; i < kNames; ++i) {
        const std::string s = "__thread_test_" + std::to_string(i);
        const SlotKey want = slotKey(s);
        Interned<std::string> name;
        name = s;
        for (int t = 0; t < kThreads; ++t) {
            bool found = false;
            for (std::size_t k = 0; k < keys[t].size(); ++k) {
                if (*names[t][k] == s) {
                    found = true;
                    EXPECT_EQ(keys[t][k], want);
                    EXPECT_EQ(names[t][k], &name.get());
                }
            }
            EXPECT_TRUE(found);
        }
    }
}

TEST(ObjectCopyCost, AStateCopySharesNamesAndKeepsCounters) {
    GameState state;
    const GameObjectId id = state.createObject();
    auto& o = state.getObject(id);
    o.name = "Brush";
    o.tags = {"Terrain"};
    o.card_counters["__turn"] = 4;
    const GameState copy = state;
    const auto& c = copy.getObject(id);
    EXPECT_EQ(&c.name.get(), &o.name.get());
    EXPECT_EQ(&c.tags.get(), &o.tags.get());
    EXPECT_EQ(c.card_counters.at("__turn"), 4);
    o.card_counters["__turn"] = 5;
    o.name = "Changed";
    EXPECT_EQ(c.card_counters.at("__turn"), 4);
    EXPECT_EQ(c.name, "Brush");
}

}  // namespace riftbound::test::object_copy_cost
