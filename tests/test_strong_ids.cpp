// Plan item 5.5: every kind of id is a type of its own.
//
// What a trait can show is asserted here at compile time. What it cannot (an
// int VARIABLE handed to a CardDefId: the literal constructor is consteval,
// and a trait does not evaluate) is in tests/compile_fail/strong_id_mix.cc,
// run by the `riftbound_strong_id_*` ctests.
#include <gtest/gtest.h>

#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "core/types.h"
#include "engine/effect_executor.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

// kInvalidId is "none" for an id and nothing else: not a number by itself.
static_assert(!std::is_convertible_v<InvalidId, uint32_t>);
static_assert(!std::is_convertible_v<InvalidId, int>);
static_assert(!std::is_constructible_v<int, InvalidId>);
template <class N>
concept ComparesWithNone = requires(N n) { n == kInvalidId; };
static_assert(!ComparesWithNone<int> && !ComparesWithNone<uint32_t>);
static_assert(ComparesWithNone<GameObjectId> && ComparesWithNone<CardDefId>);
static_assert(StrongIdShape<GameObjectId>);
static_assert(TakesNoNumber<GameObjectId>);
static_assert(DoNotMix<GameObjectId, CardDefId>);
static_assert(DoNotMix<GameObjectId, BattlefieldId>);
static_assert(DoNotMix<GameObjectId, ChainItemId>);
static_assert(DoNotMix<GameObjectId, AbilityId>);
// The mix-up CLAUDE.md warns about: Card::cardDefId() where ctx.source belongs.
static_assert(!std::is_invocable_v<const GameObject& (GameState::*)(GameObjectId) const,
                                   const GameState&, CardDefId>);
static_assert(std::is_invocable_v<const GameObject& (GameState::*)(GameObjectId) const,
                                  const GameState&, GameObjectId>);
static_assert(StrongIdShape<CardDefId>);
static_assert(StrongIdShape<BattlefieldId>);
static_assert(TakesNoNumber<BattlefieldId>);
static_assert(DoNotMix<CardDefId, BattlefieldId>);
static_assert(StrongIdShape<ChainItemId>);
static_assert(TakesNoNumber<ChainItemId>);
static_assert(StrongIdShape<AbilityId>);
static_assert(TakesNoNumber<AbilityId>);
static_assert(DoNotMix<CardDefId, ChainItemId>);
static_assert(DoNotMix<CardDefId, AbilityId>);
static_assert(DoNotMix<BattlefieldId, ChainItemId>);
// The setup choice is a position in a battlefield list, not a slot (9.RR2).
static_assert(StrongIdShape<BattlefieldPoolIndex>);
static_assert(TakesNoNumber<BattlefieldPoolIndex>);
static_assert(DoNotMix<BattlefieldId, BattlefieldPoolIndex>);
static_assert(DoNotMix<GameObjectId, BattlefieldPoolIndex>);
static_assert(DoNotMix<CardDefId, BattlefieldPoolIndex>);
static_assert(!std::is_convertible_v<BattlefieldPoolIndex, BattlefieldLocation>);
static_assert(std::is_same_v<decltype(Intent::battlefield_pool_index), BattlefieldPoolIndex>);
static_assert(std::is_same_v<decltype(Intent::chosen_battlefield), BattlefieldId>);
static_assert(DoNotMix<BattlefieldId, AbilityId>);
static_assert(DoNotMix<ChainItemId, AbilityId>);
// A location is not an id, and takes a slot number only as a literal spelled
// with its type (`BattlefieldLocation{0}`), never by itself.
static_assert(!std::is_convertible_v<int, BattlefieldLocation>);
static_assert(!std::is_convertible_v<int, LocationId>);
static_assert(std::is_convertible_v<BattlefieldId, BattlefieldLocation>);
static_assert(!std::is_constructible_v<BattlefieldLocation, CardDefId>);

// ── Values ──────────────────────────────────────────────────────────────────

TEST(StrongIds, NoneIsTheDefaultAndIsUint32Max) {
    const CardDefId none;
    EXPECT_EQ(none, kInvalidId);
    EXPECT_FALSE(none.valid());
    EXPECT_EQ(none.raw(), 4294967295u);
    EXPECT_EQ(CardDefId{kInvalidId}.raw(), 4294967295u);
    // The number a plain integer slot holds for "none", as before.
    const uint32_t raw_none = kInvalidId.raw();
    EXPECT_EQ(raw_none, 4294967295u);
    EXPECT_EQ(static_cast<int32_t>(kInvalidId.raw()), -1);
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

TEST(StrongIds, ABattlefieldIdIsItsSlot) {
    GameState state;
    for (uint32_t slot = 0; slot < 3; ++slot) {
        BattlefieldState bf;
        bf.id = BattlefieldId::fromRaw(slot);
        state.battlefields.push_back(bf);
    }
    // The list is indexed by position and by id alike.
    EXPECT_EQ(&state.battlefields[1], &state.battlefields[BattlefieldId::fromRaw(1)]);
    EXPECT_EQ(state.battlefields[BattlefieldId::fromRaw(2)].id.raw(), 2u);
    EXPECT_EQ(BattlefieldLocation{2}.id, BattlefieldId::fromRaw(2));
    EXPECT_EQ(BattlefieldLocation{}.id, kInvalidId);
    EXPECT_EQ(BattlefieldLocation{kInvalidId}.id, kInvalidId);
    EXPECT_EQ(GameObject{}.hidden_at, kInvalidId);
    EXPECT_EQ(Intent{}.chosen_battlefield, kInvalidId);
    EXPECT_EQ(std::hash<BattlefieldId>{}(BattlefieldId::fromRaw(3)), std::hash<uint32_t>{}(3u));
}

TEST(StrongIds, ChainItemIdsCountUpFromOne) {
    ChainState chain;
    EXPECT_EQ(chain.allocateId().raw(), 1u);
    EXPECT_EQ(chain.allocateId().raw(), 2u);
    EXPECT_EQ(chain.next_id.raw(), 3u);
    EXPECT_EQ(ChainItem{}.id, kInvalidId);
    EXPECT_EQ(Intent{}.ability, kInvalidId);
    // A death batch that was never begun takes every chain item as its own.
    EXPECT_EQ(DeathBatchMark{}.first_item.raw(), 0u);
}

TEST(StrongIds, ObjectIdsCountUpFromOneAndKeyTheTableByNumber) {
    GameState state;
    const GameObjectId a = state.createObject();
    const GameObjectId b = state.createObject();
    EXPECT_EQ(a.raw(), 1u);
    EXPECT_EQ(b.raw(), 2u);
    EXPECT_EQ(state.getObject(b).id, b);
    EXPECT_EQ(GameObject{}.id, kInvalidId);
    EXPECT_EQ(Intent{}.card, kInvalidId);
    EXPECT_FALSE(state.objectExists(kInvalidId));
    EXPECT_FALSE(state.objectExists(GameObjectId{}));
    // The slots cards keep ids in hold the number, -1 for none.
    const std::vector<int32_t> slots = {static_cast<int32_t>(b.raw()),
                                        static_cast<int32_t>(kInvalidId.raw()), 7};
    const auto ids = objectIdsFromRaw(slots);
    ASSERT_EQ(ids.size(), 3u);
    EXPECT_EQ(ids[0], b);
    EXPECT_EQ(ids[1], kInvalidId);
    EXPECT_EQ(ids[2].raw(), 7u);
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

TEST(StrongIds, HistoryTokensAndSlotsArePinned) {
    // The numbers of five intents on a default GameState, each alone in its
    // legal list. Until the 2026-10 checkpoint break (stage 5) these were the
    // numbers of a build of fcfa93b, from before ids were types. They changed
    // twice in that stage: Intent gained `battlefield_pool_index` (plans/09
    // item 9.RR2), which every token fingerprints, and the action vocabulary
    // was replaced (plans/01 item 1.12: new slot ranges), and with the card id
    // space, which grew to 1150 (plans/09 item 9.SS2) and then to 2048
    // (9.UU1): the card-keyed ranges are wider. A trainer replay or a saved game holds these numbers: a
    // change here is a break that docs/checkpoint-breaking-2026-10.md lists.
    GameState state;
    Intent own;
    own.type = IntentType::ActivateAbility;
    own.player = PlayerId::Player1;
    own.ability_source = GameObjectId::fromRaw(9);
    EXPECT_EQ(openspiel::historyToken(own, state), 7960844909697663093LL);
    EXPECT_EQ(openspiel::actionSlot(own, state), 98421);

    Intent granted = own;
    granted.granted_ability_def = 506;
    EXPECT_EQ(openspiel::historyToken(granted, state), 4496770096613665396LL);

    Intent named;  // "name a card": a MakeChoice that carries a card id
    named.type = IntentType::MakeChoice;
    named.player = PlayerId::Player2;
    named.chosen_card_def = 235;
    EXPECT_EQ(openspiel::historyToken(named, state), 4030418850666365288LL);
    EXPECT_EQ(openspiel::actionSlot(named, state), 115048);

    Intent bf;
    bf.type = IntentType::ChooseBattlefield;
    bf.player = PlayerId::Player1;
    bf.battlefield_pool_index = BattlefieldPoolIndex::fromRaw(1);
    EXPECT_EQ(openspiel::historyToken(bf, state), 9050218172478652462LL);
    EXPECT_EQ(openspiel::actionSlot(bf, state), 46);

    EXPECT_EQ(openspiel::historyToken(Intent::endTurn(PlayerId::Player1), state),
              2124341512652193793LL);
}

// Plan item 9.RR1. The compiler cannot see an integer slot written as one id
// kind and read as another, or read with the wrong "none". Every place that
// makes an object id out of a slot (`GameObjectId::fromRaw`) was paired by
// hand with the write that fills the slot (the table is in
// plans/reports/strong-ids-followup-1.md): same kind, same encoding (plain,
// id + 1 with 0 for none, or kInvalidId.raw()), a "none" test that matches.
// This is the audited list. A new reader, or one more in a listed file, fails
// here until it has been paired too and the count is updated.
TEST(StrongIds, EveryObjectIdReadFromAnIntegerSlotIsOnTheAuditedList) {
    const std::map<std::string, int> audited = {
        {"src/cards/card.cpp", 7},
        {"src/cards/card_helpers.h", 10},
        {"src/cards/gear/0412_the_zero_drive.cpp", 1},
        {"src/cards/gear/0698_scryer_s_bloom.cpp", 1},
        {"src/cards/gear/0854_bottled_constellation.cpp", 1},
        {"src/cards/gear/0924_shady_spectacles.cpp", 1},
        {"src/cards/legends/0553_void_burrower.cpp", 3},
        {"src/cards/legends/0756_deceiver.cpp", 1},
        {"src/cards/spells/0062_reinforce.cpp", 3},
        {"src/cards/spells/0115_promising_future.cpp", 2},
        {"src/cards/spells/0153_overt_operation.cpp", 2},
        {"src/cards/spells/0183_stacked_deck.cpp", 4},
        {"src/cards/spells/0209_cull_the_weak.cpp", 1},
        {"src/cards/spells/0247_icathian_rain.cpp", 1},
        {"src/cards/spells/0253_fox_fire.cpp", 2},
        {"src/cards/spells/0255_dragon_s_rage.cpp", 1},
        {"src/cards/spells/0309_flash.cpp", 1},
        {"src/cards/spells/0402_bellows_breath.cpp", 1},
        {"src/cards/spells/0475_guards.cpp", 2},
        {"src/cards/spells/0513_arise.cpp", 2},
        {"src/cards/spells/0516_hostile_takeover.cpp", 1},
        {"src/cards/spells/0616_tricksy_tentacles.cpp", 2},
        {"src/cards/spells/0665_disposal_order.cpp", 1},
        {"src/cards/spells/0693_abandon.cpp", 1},
        {"src/cards/spells/0701_bone_skewer.cpp", 1},
        {"src/cards/spells/0704_heedless_resurrection.cpp", 2},
        {"src/cards/spells/0730_undying_loyalty.cpp", 1},
        {"src/cards/spells/0757_mirror_image.cpp", 2},
        {"src/cards/spells/0876_wild_claw.cpp", 3},
        {"src/cards/spells/0894_decree_of_discord.cpp", 2},
        {"src/cards/spells/0943_lightning_rush.cpp", 4},   // +1: the look's replacement filter (D71)
        {"src/cards/units/0109_dr_mundo_expert.cpp", 1},
        {"src/cards/units/0141_kinkou_monk.cpp", 2},
        {"src/cards/units/0347_rell_magnetic.cpp", 1},
        {"src/cards/units/0449_overzealous_fan.cpp", 1},
        {"src/cards/units/0491_rek_sai_swarm_queen.cpp", 2},
        {"src/cards/units/0643_keeper_of_masks.cpp", 1},
        {"src/core/game_state.h", 2},
        {"src/io/play_server.cpp", 3},
    };
    const char* root_env = std::getenv("RIFTBOUND_ROOT");
    const std::filesystem::path src = std::filesystem::path(root_env ? root_env : "..") / "src";
    ASSERT_TRUE(std::filesystem::is_directory(src)) << src;
    const std::string needle = "GameObjectId::fromRaw(";
    std::map<std::string, int> found;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(src)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension();
        if (ext != ".cpp" && ext != ".h") continue;
        const std::string rel =
            "src/" + std::filesystem::relative(entry.path(), src).generic_string();
        if (rel == "src/core/types.h") continue;   // the definition and its comment
        std::ifstream in(entry.path());
        std::string line;
        int n = 0;
        while (std::getline(in, line))
            for (auto at = line.find(needle); at != std::string::npos;
                 at = line.find(needle, at + needle.size()))
                ++n;
        if (n > 0) found[rel] = n;
    }
    for (const auto& [file, n] : found) {
        const auto it = audited.find(file);
        EXPECT_TRUE(it != audited.end())
            << file << " reads an object id from an integer slot and is not on the audited list:"
            << " pair the reader with the slot's writer (kind, encoding, none) and add it";
        if (it != audited.end()) {
            EXPECT_EQ(n, it->second) << file << ": the number of slot readers changed;"
                                     << " pair the new one with its writer and update the count";
        }
    }
    for (const auto& [file, n] : audited)
        EXPECT_TRUE(found.count(file)) << file << " no longer reads a slot: take it off the list";
}

}  // namespace riftbound::test::strong_ids
