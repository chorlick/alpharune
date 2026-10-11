// Plan item 5.5: every kind of id is a type of its own.
//
// What a trait can show is asserted here at compile time. What it cannot (an
// int VARIABLE handed to a CardDefId: the literal constructor is consteval,
// and a trait does not evaluate) is in tests/compile_fail/strong_id_mix.cc,
// run by the `riftbound_strong_id_*` ctests.
#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "core/types.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"

#include <map>
#include <set>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace riftbound::test::strong_ids {

// ── The properties every strong id has ──────────────────────────────────────
template <class Id>
concept StrongIdShape =
    std::is_trivially_copyable_v<Id> &&
    std::is_standard_layout_v<Id> &&
    sizeof(Id) == sizeof(uint32_t) && alignof(Id) == alignof(uint32_t) &&
    // none is the default, and kInvalidId converts to it
    std::is_nothrow_default_constructible_v<Id> &&
    std::is_convertible_v<InvalidId, Id> &&
    // never to a number, a bool or anything else by itself
    !std::is_convertible_v<Id, uint32_t> && !std::is_convertible_v<Id, int> &&
    !std::is_convertible_v<Id, bool> && !std::is_convertible_v<Id, std::size_t> &&
    !std::is_constructible_v<uint32_t, Id> && !std::is_constructible_v<int, Id> &&
    !std::is_constructible_v<bool, Id> && !std::is_constructible_v<float, Id> &&
    // by name only
    requires(Id id, uint32_t n, int32_t s) {
        { id.raw() } -> std::same_as<uint32_t>;
        { Id::fromRaw(n) } -> std::same_as<Id>;
        { Id::fromRaw(s) } -> std::same_as<Id>;
        { id == id } -> std::same_as<bool>;
        { id < id } -> std::same_as<bool>;
        { id == kInvalidId } -> std::same_as<bool>;
        { kInvalidId != id } -> std::same_as<bool>;
        { std::hash<Id>{}(id) } -> std::same_as<std::size_t>;
        { toString(id) } -> std::same_as<std::string>;
    } &&
    // no arithmetic, no indexing with it, no comparison with a number
    !requires(Id id) { id + 1; } && !requires(Id id) { ++id; } &&
    !requires(Id id, std::vector<int> v) { v[id]; };

/// A kind that does NOT take a number at all (every kind but the card id).
template <class Id>
concept TakesNoNumber =
    !std::is_convertible_v<uint32_t, Id> && !std::is_convertible_v<int, Id> &&
    !std::is_constructible_v<Id, uint32_t> && !std::is_constructible_v<Id, int> &&
    !std::is_constructible_v<Id, std::size_t> && !std::is_assignable_v<Id&, int> &&
    !requires(Id id) { id == 0; } &&
    !requires(Id id, uint32_t n) { id == n; } &&
    !requires(Id id, uint32_t n) { id < n; };

/// Two kinds do not mix in any direction.
template <class A, class B>
concept DoNotMix =
    !std::is_same_v<A, B> &&
    !std::is_convertible_v<A, B> && !std::is_convertible_v<B, A> &&
    !std::is_constructible_v<A, B> && !std::is_constructible_v<B, A> &&
    !std::is_assignable_v<A&, B> && !std::is_assignable_v<B&, A> &&
    !requires(A a, B b) { a == b; } && !requires(A a, B b) { a < b; } &&
    !requires(std::vector<A> v, B b) { v.push_back(b); } &&
    !requires(std::unordered_map<A, int> m, B b) { m.find(b); };

static_assert(StrongIdShape<CardDefId>);

// ── Values ──────────────────────────────────────────────────────────────────

TEST(StrongIds, NoneIsTheDefaultAndIsUint32Max) {
    const CardDefId none;
    EXPECT_EQ(none, kInvalidId);
    EXPECT_FALSE(none.valid());
    EXPECT_EQ(none.raw(), 4294967295u);
    EXPECT_EQ(CardDefId{kInvalidId}.raw(), 4294967295u);
    // The number a plain integer slot holds for "none", as before.
    const uint32_t raw_none = kInvalidId;
    EXPECT_EQ(raw_none, 4294967295u);
    EXPECT_EQ(static_cast<int32_t>(kInvalidId), -1);
    // -1 read back from a signed slot is none.
    EXPECT_EQ(CardDefId::fromRaw(int32_t{-1}), kInvalidId);
}

TEST(StrongIds, ACardIdLiteralIsItsNumber) {
    const CardDefId fiora = 235;
    EXPECT_EQ(fiora.raw(), 235u);
    EXPECT_TRUE(fiora.valid());
    EXPECT_EQ(CardDefId::fromRaw(235), fiora);
    EXPECT_EQ(toString(fiora), "235");
    std::ostringstream os;
    os << fiora << ' ' << CardDefId{};
    EXPECT_EQ(os.str(), "235 4294967295");
}

TEST(StrongIds, OrderAndHashAreTheNumbers) {
    // Option order and the iteration order of every container keyed by an id
    // depend on these being what they were for a plain uint32_t.
    for (uint32_t a : {0u, 1u, 7u, 955u, 1100u, 65536u, 4294967295u}) {
        EXPECT_EQ(std::hash<CardDefId>{}(CardDefId::fromRaw(a)), std::hash<uint32_t>{}(a));
        for (uint32_t b : {0u, 1u, 7u, 955u, 4294967295u}) {
            EXPECT_EQ(CardDefId::fromRaw(a) < CardDefId::fromRaw(b), a < b);
            EXPECT_EQ(CardDefId::fromRaw(a) == CardDefId::fromRaw(b), a == b);
        }
    }
    std::unordered_map<CardDefId, int> strong;
    std::unordered_map<uint32_t, int> plain;
    for (uint32_t n : {700u, 3u, 955u, 12u, 401u, 88u, 1u, 640u, 213u, 77u, 902u, 5u}) {
        strong[CardDefId::fromRaw(n)] = 1;
        plain[n] = 1;
    }
    std::vector<uint32_t> strong_order, plain_order;
    for (const auto& [id, v] : strong) strong_order.push_back(id.raw());
    for (const auto& [n, v] : plain) plain_order.push_back(n);
    EXPECT_EQ(strong_order, plain_order);

    std::set<CardDefId> ordered = {CardDefId{9}, CardDefId{2}, CardDefId{}, CardDefId{5}};
    std::vector<uint32_t> got;
    for (auto id : ordered) got.push_back(id.raw());
    EXPECT_EQ(got, (std::vector<uint32_t>{2, 5, 9, 4294967295u}));
}

// ── One "none" per kind: the places that used 0 for "no card" ───────────────

TEST(StrongIds, NoCardIsKInvalidIdEverywhere) {
    EXPECT_EQ(CardDef{}.id, kInvalidId);
    EXPECT_EQ(DeckSubmission{}.legend, kInvalidId);
    EXPECT_EQ(DeckSubmission{}.chosen_champion, kInvalidId);
    EXPECT_EQ(Intent{}.granted_ability_def, kInvalidId);
    EXPECT_EQ(Intent{}.chosen_card_def, kInvalidId);
    EXPECT_EQ(ChainItem{}.granted_ability_def, kInvalidId);
    EXPECT_EQ(ChainItem{}.card_def_id, kInvalidId);
    EXPECT_EQ(GameObject{}.card_def_id, kInvalidId);
    EXPECT_EQ(GameObject::GrantedAbilityRef{}.source_def_id, kInvalidId);
    EXPECT_EQ(GameObject::PendingDeathReplacement{}.card, kInvalidId);
}

TEST(StrongIds, ARevealOfNoCardIsNotRemembered) {
    // recordReveal used to filter kInvalidId only, while other fields spelled
    // "no card" as 0; with one none there is one filter.
    GameState state;
    state.recordReveal(CardRevealedEvent{GameObjectId{}, CardDefId{}, PlayerId::Player1,
                                         true});
    EXPECT_TRUE(state.player(PlayerId::Player1).observed_cards.empty());
    EXPECT_TRUE(state.player(PlayerId::Player2).observed_cards.empty());
    state.recordReveal(CardRevealedEvent{GameObjectId{}, CardDefId{235}, PlayerId::Player1,
                                         true});
    EXPECT_EQ(state.player(PlayerId::Player1).observed_cards.at(CardDefId{235}), 1);
    EXPECT_EQ(state.player(PlayerId::Player2).observed_cards.at(CardDefId{235}), 1);
}

TEST(StrongIds, AMissingLegendOrChampionIsReportedNotLookedUp) {
    CardRegistry registry;
    registerAllCards(registry);
    CardDB db;
    db.buildFromClasses(registry);
    DeckValidator validator(db);
    DeckSubmission deck;  // nothing named
    auto result = validator.validate(deck);  // (a lookup of "no card" would throw)
    EXPECT_FALSE(result.is_legal);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("egend"), std::string::npos) << result.errors[0];

    const CardDef* legend = db.findByName("Deceiver");
    ASSERT_NE(legend, nullptr);
    deck.legend = legend->id;
    result = validator.validate(deck);
    EXPECT_FALSE(result.is_legal);
    bool champion = false;
    for (const auto& e : result.errors)
        if (e.find("hampion") != std::string::npos) champion = true;
    EXPECT_TRUE(champion);
}

TEST(StrongIds, TheHistoryTokenOfAnOwnAbilityKeptItsNumber) {
    // Intent::granted_ability_def was 0 for "the source's own ability" and is
    // kInvalidId now; the history token still feeds 0 for it, so tokens are
    // what they were. A token that fed UINT32_MAX would differ from this one.
    GameState state;
    Intent own;
    own.type = IntentType::ActivateAbility;
    own.player = PlayerId::Player1;
    Intent granted = own;
    granted.granted_ability_def = 506;
    const int64_t own_token = openspiel::historyToken(own, state);
    EXPECT_NE(own_token, openspiel::historyToken(granted, state));
    Intent as_zero = own;
    as_zero.granted_ability_def = CardDefId::fromRaw(0);
    EXPECT_EQ(own_token, openspiel::historyToken(as_zero, state))
        << "none must be fed to the token as the 0 it used to be";
}

}  // namespace riftbound::test::strong_ids
