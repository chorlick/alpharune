/// The state feature vector has ONE ordering convention: everything is relative
/// to the perspective player, self first and opponent second (plans/01 item
/// 1.9). Before the 2026-10 checkpoint break the battlefield controller was
/// "1 = Player1, 2 = Player2" and the two per-side blocks of every battlefield
/// were "Player1 then Player2", next to blocks that were self-then-opponent.
///
/// Three kinds of test:
///   (a) per block, a hand-built position in which the two seats differ,
///       extracted from both seats, asserting whose data is where;
///   (b) seat-swap symmetry at every decision of real games: the vector of a
///       position seen from one seat equals the vector of the mirrored
///       position (every Player1 thing given to Player2 and back) seen from
///       the other seat, over the whole vector;
///   (c) hidden information stays masked per perspective at every decision of
///       the same games.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "ml/feature_extractor.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::feature_perspective {

namespace L = ::riftbound::ml::layout;
constexpr PlayerId P1 = PlayerId::Player1;
constexpr PlayerId P2 = PlayerId::Player2;

// ─── The seat swap ──────────────────────────────────────────────────────────

PlayerId other(PlayerId p) {
    return p == P1 ? P2 : p == P2 ? P1 : p;
}
void flip(PlayerId& p) { p = other(p); }
void flip(std::optional<PlayerId>& p) { if (p) *p = other(*p); }
std::optional<LocationId> flipped(const std::optional<LocationId>& loc) {
    if (!loc) return loc;
    if (const auto* base = std::get_if<BaseLocation>(&*loc))
        return LocationId{BaseLocation{other(base->player)}};
    return loc;
}

/// The position with every Player1 thing given to Player2 and back: the two
/// player states change places, and every field that names a player names the
/// other one. Object ids, battlefield slots and all numbers stay. (The
/// knowledge tracker's per-observer facts are not swapped: the feature
/// extractor does not read them.)
GameState mirrorSeats(const GameState& in) {
    GameState s = in;
    std::swap(s.players[0], s.players[1]);
    for (auto& ps : s.players) {
        flip(ps.id);
        for (auto& p : ps.additional_turns) flip(p);
    }
    for (auto& [id, obj] : s.objects) {
        flip(obj.owner);
        flip(obj.controller);
        if (obj.location.has_value()) obj.location = flipped(obj.location);
        obj.last_location = flipped(obj.last_location);
        std::swap(obj.damage_marked_by[0], obj.damage_marked_by[1]);
        flip(obj.original_controller_eot);
        flip(obj.control_revert_to);
        flip(obj.last_known.controller);
        flip(obj.last_known.killed_by);
        obj.last_known.location = flipped(obj.last_known.location);
        for (auto& r : obj.pending_death_replacements) flip(r.controller);
    }
    for (auto& bf : s.battlefields) {
        flip(bf.controller);
        flip(bf.contested_by);
        flip(bf.contributed_by);
        flip(bf.attacker);
        flip(bf.defender);
        flip(bf.conquered_by_player);
        flip(bf.conquered_from);
        std::swap(bf.ignore_tank[0], bf.ignore_tank[1]);
        std::swap(bf.empower_cost_reduction[0], bf.empower_cost_reduction[1]);
        std::swap(bf.multi_move_tax[0], bf.multi_move_tax[1]);
        std::swap(bf.opp_hidden_unrevealable[0], bf.opp_hidden_unrevealable[1]);
    }
    flip(s.turn.turn_player);
    flip(s.turn.starting_player);
    flip(s.turn.priority_holder);
    flip(s.turn.focus_holder);
    std::swap(s.turn.turn_decisions_p1, s.turn.turn_decisions_p2);
    auto flipSet = [](std::set<PlayerId>& set) {
        std::set<PlayerId> out;
        for (auto p : set) out.insert(other(p));
        set = std::move(out);
    };
    flipSet(s.turn.players_passed_priority);
    flipSet(s.turn.players_passed_focus);
    for (auto& item : s.chain.items) flip(item.controller);
    if (s.chain.resuming) flip(s.chain.resuming->controller);
    for (auto& d : s.delayed_abilities) {
        flip(d.controller);
        flip(d.watched_player);
    }
    flip(s.winner);
    if (s.cost_cursor) flip(s.cost_cursor->player);
    return s;
}

/// Name of the block a feature index falls in, for failure messages and the
/// per-block census.
const char* blockOf(int i) {
    if (i < L::kChainItems) return "kGlobals";
    if (i < L::kSelfPlayer) return "kChainItems";
    if (i < L::kOppPlayer) return "kSelfPlayer";
    if (i < L::kBattlefields) return "kOppPlayer";
    if (i < L::kChainTargets) {
        const int in_bf = (i - L::kBattlefields) % L::kBfDim;
        if (in_bf == 1) return "kBattlefields.controller";
        if (in_bf < L::kBfHeaderDim) return "kBattlefields.header";
        return "kBattlefields.sides";
    }
    if (i < L::kCostModTypes) return "kChainTargets";
    if (i < L::kScoredFlags) return "kCostModTypes";
    if (i < L::kUnitExt) return "kScoredFlags";
    if (i < L::kZones) return "kUnitExt";
    if (i < L::kPlayLockout) return "kZones";
    if (i < L::kUnitT2) return "kPlayLockout";
    if (i < L::kExtGlobals) return "kUnitT2";
    if (i < L::kBfContributedBy) return "kExtGlobals";
    if (i < L::kPadding) return "kBfContributedBy";
    return "kPadding";
}

// ─── (a) hand-built positions ───────────────────────────────────────────────

struct Board {
    GameState state;
    CardRegistry registry;
    CardDB db;

    Board() {
        registry.loadAll();
        db.buildFromClasses(registry);
        state.player(P1).id = P1;
        state.player(P2).id = P2;
        for (uint32_t i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(i);
            bf.contributed_by = i == 0 ? P1 : P2;
            state.battlefields.push_back(bf);
        }
    }

    GameObject& make(PlayerId owner, CardDefId def, ZoneType zone) {
        const GameObjectId id = state.createObject();
        auto& o = state.getObject(id);
        o.card_def_id = def;
        o.owner = owner;
        o.controller = owner;
        o.zone = zone;
        return o;
    }
    GameObject& unitAt(PlayerId controller, CardDefId def, LocationId loc, int might) {
        const bool base = std::holds_alternative<BaseLocation>(loc);
        auto& o = make(controller, def, base ? ZoneType::Base : ZoneType::BattlefieldZone);
        o.card_type = CardType::Unit;
        o.location = loc;
        o.base_might = might;
        o.current_might = might;
        return o;
    }
    GameObjectId inZone(PlayerId owner, CardDefId def, ZoneType zone,
                        std::vector<GameObjectId>& list) {
        auto& o = make(owner, def, zone);
        list.push_back(o.id);
        return o.id;
    }
    std::vector<float> from(PlayerId p) const {
        return ml::extractStateFeatures(state, p, db);
    }
};

constexpr BattlefieldLocation kBf0{0};
constexpr BattlefieldLocation kBf1{1};

TEST(FeaturePerspective, BattlefieldControllerIsSelfOrOpponent) {
    Board b;
    b.state.battlefields[0].controller = P1;
    b.state.battlefields[1].controller = P2;
    const auto f1 = b.from(P1), f2 = b.from(P2);
    // 1 = the perspective player controls it, 2 = the opponent does.
    EXPECT_EQ(f1[L::bf(0) + 1], 1.0f);
    EXPECT_EQ(f1[L::bf(1) + 1], 2.0f);
    EXPECT_EQ(f2[L::bf(0) + 1], 2.0f) << "Player2 sees Player1's battlefield as the opponent's";
    EXPECT_EQ(f2[L::bf(1) + 1], 1.0f) << "and its own as its own";
    // Nobody: 0 from both seats.
    b.state.battlefields[0].controller.reset();
    EXPECT_EQ(b.from(P1)[L::bf(0) + 1], 0.0f);
    EXPECT_EQ(b.from(P2)[L::bf(0) + 1], 0.0f);
}

TEST(FeaturePerspective, BattlefieldSideBlocksAreSelfThenOpponent) {
    Board b;
    // Player1: one 5-Might unit (card 10) at battlefield 0, damaged 2.
    // Player2: two units (cards 20 and 21, Might 3 and 1) there.
    b.unitAt(P1, 10, kBf0, 5).damage_marked = 2;
    b.unitAt(P2, 20, kBf0, 3);
    b.unitAt(P2, 21, kBf0, 1);
    for (PlayerId me : {P1, P2}) {
        const auto f = b.from(me);
        const int mine = L::bfSide(0, me == P1 ? 0 : 1);     // where Player1's side is
        const int theirs = L::bfSide(0, me == P1 ? 1 : 0);   // where Player2's side is
        EXPECT_EQ(f[mine + 0], 1.0f) << toString(me) << ": Player1's unit count";
        EXPECT_EQ(f[mine + 1], 5.0f);
        EXPECT_EQ(f[mine + 2], 2.0f);
        EXPECT_EQ(f[mine + L::kSideUnitIds], 10.0f);
        EXPECT_EQ(f[mine + L::kSideUnitMight], 5.0f);
        EXPECT_EQ(f[mine + L::kSideUnitDamage], 2.0f);
        EXPECT_EQ(f[theirs + 0], 2.0f) << toString(me) << ": Player2's unit count";
        EXPECT_EQ(f[theirs + 1], 4.0f);
        EXPECT_EQ(f[theirs + L::kSideUnitIds], 20.0f);
        EXPECT_EQ(f[theirs + L::kSideUnitIds + 1], 21.0f);
        EXPECT_EQ(f[theirs + L::kSideUnitMight], 3.0f);
    }
    // Side 0 is the perspective player's from either seat.
    EXPECT_EQ(b.from(P1)[L::bfSide(0, 0) + L::kSideUnitIds], 10.0f);
    EXPECT_EQ(b.from(P2)[L::bfSide(0, 0) + L::kSideUnitIds], 20.0f);
}

TEST(FeaturePerspective, PerUnitBlocksLineUpWithTheSideBlocks) {
    Board b;
    // Player1's unit is the attacker with +2 Might this turn and Assault 3;
    // Player2's is the defender with Shield 4 and a buff.
    auto& a = b.unitAt(P1, 10, kBf1, 6);
    a.combat_designation = CombatDesignation::Attacker;
    a.base_might = 4;
    a.temp_might_bonus = 2;
    a.assault_value = 3;
    auto& d = b.unitAt(P2, 20, kBf1, 2);
    d.combat_designation = CombatDesignation::Defender;
    d.shield_value = 4;
    d.buff_count = 1;
    for (PlayerId me : {P1, P2}) {
        const auto f = b.from(me);
        const int p1_side = me == P1 ? 0 : 1, p2_side = 1 - p1_side;
        // The same unit is at the same (battlefield, side, unit) slot of all
        // three blocks: the base side block, the extended block and Tier 2.
        EXPECT_EQ(f[L::bfSide(1, p1_side) + L::kSideUnitIds], 10.0f) << toString(me);
        EXPECT_EQ(f[L::unitExt(1, p1_side, 0) + 0], 1.0f) << toString(me) << ": attacker";
        EXPECT_EQ(f[L::unitExt(1, p1_side, 0) + 2], 2.0f) << "Might delta";
        EXPECT_EQ(f[L::unitExt(1, p1_side, 0) + 3], 2.0f) << "temporary Might";
        EXPECT_EQ(f[L::unitT2(1, p1_side, 0) + 0], 3.0f) << "assault";
        EXPECT_EQ(f[L::unitT2(1, p1_side, 0) + 1], 0.0f);

        EXPECT_EQ(f[L::bfSide(1, p2_side) + L::kSideUnitIds], 20.0f) << toString(me);
        EXPECT_EQ(f[L::unitExt(1, p2_side, 0) + 0], 2.0f) << toString(me) << ": defender";
        EXPECT_EQ(f[L::unitExt(1, p2_side, 0) + 2], 0.0f);
        EXPECT_EQ(f[L::unitT2(1, p2_side, 0) + 0], 0.0f);
        EXPECT_EQ(f[L::unitT2(1, p2_side, 0) + 1], 4.0f) << "shield";
        EXPECT_EQ(f[L::unitT2(1, p2_side, 0) + 3], 1.0f) << "buffs";
    }
}

TEST(FeaturePerspective, PlayerBlocksGlobalsAndChainAreSelfThenOpponent) {
    Board b;
    b.state.player(P1).score = 5;
    b.state.player(P2).score = 2;
    b.state.turn.turn_player = P2;
    b.state.turn.starting_player = P1;
    ChainItem item;
    item.controller = P2;
    b.state.chain.items.push_back(item);
    const auto f1 = b.from(P1), f2 = b.from(P2);
    EXPECT_EQ(f1[L::kSelfPlayer], 5.0f);
    EXPECT_EQ(f1[L::kOppPlayer], 2.0f);
    EXPECT_EQ(f2[L::kSelfPlayer], 2.0f);
    EXPECT_EQ(f2[L::kOppPlayer], 5.0f);
    EXPECT_EQ(f1[L::kGlobalScoreDiff], 3.0f);
    EXPECT_EQ(f2[L::kGlobalScoreDiff], -3.0f);
    EXPECT_EQ(f1[L::kGlobalSelfIsTurnPlayer], 0.0f);
    EXPECT_EQ(f2[L::kGlobalSelfIsTurnPlayer], 1.0f);
    // Who goes first is the explicit feature that stands in for the seat.
    EXPECT_EQ(f1[L::kGlobalSelfStarts], 1.0f);
    EXPECT_EQ(f2[L::kGlobalSelfStarts], 0.0f);
    EXPECT_EQ(f1[L::kChainItems + 1], 0.0f) << "the chain item is the opponent's";
    EXPECT_EQ(f2[L::kChainItems + 1], 1.0f);
}

TEST(FeaturePerspective, PairedBlocksAreSelfThenOpponent) {
    Board b;
    // Cost-modifier types and sum, scored flags, lockout, additional turns,
    // ready base units: each differs between the seats.
    PlayerState::CostModifier mod;
    mod.next_spell_only = true;
    mod.energy_reduction = 2;
    b.state.player(P1).cost_modifiers.push_back(mod);
    b.state.player(P2).battlefields_scored_this_turn.insert(BattlefieldId::fromRaw(1));
    b.state.player(P2).cant_play_cards_this_turn = true;
    b.state.player(P1).additional_turns.push_back(P1);
    b.unitAt(P2, 20, BaseLocation{P2}, 2);
    const auto f1 = b.from(P1), f2 = b.from(P2);

    EXPECT_EQ(f1[L::kCostModTypes + 0], 1.0f);      // self next-spell
    EXPECT_EQ(f1[L::kCostModTypes + 2], 0.0f);      // opp next-spell
    EXPECT_EQ(f2[L::kCostModTypes + 0], 0.0f);
    EXPECT_EQ(f2[L::kCostModTypes + 2], 1.0f);

    EXPECT_EQ(f1[L::kScoredFlags + 2 * 1 + 0], 0.0f);   // battlefield 1, scored by self
    EXPECT_EQ(f1[L::kScoredFlags + 2 * 1 + 1], 1.0f);   // ... by the opponent
    EXPECT_EQ(f2[L::kScoredFlags + 2 * 1 + 0], 1.0f);
    EXPECT_EQ(f2[L::kScoredFlags + 2 * 1 + 1], 0.0f);

    EXPECT_EQ(f1[L::kPlayLockout + 0], 0.0f);
    EXPECT_EQ(f1[L::kPlayLockout + 1], 1.0f);
    EXPECT_EQ(f2[L::kPlayLockout + 0], 1.0f);
    EXPECT_EQ(f2[L::kPlayLockout + 1], 0.0f);

    EXPECT_EQ(f1[L::kExtGlobals + 0], 0.0f);   // ready base units: self
    EXPECT_EQ(f1[L::kExtGlobals + 1], 1.0f);   // opp
    EXPECT_EQ(f2[L::kExtGlobals + 0], 1.0f);
    EXPECT_EQ(f2[L::kExtGlobals + 1], 0.0f);
    EXPECT_EQ(f1[L::kExtGlobals + 2], 2.0f);   // cost-modifier sum: self
    EXPECT_EQ(f2[L::kExtGlobals + 3], 2.0f);   // opp
    EXPECT_EQ(f1[L::kExtGlobals + 4], 1.0f);   // additional turns: self
    EXPECT_EQ(f2[L::kExtGlobals + 5], 1.0f);   // opp
}

TEST(FeaturePerspective, CombatAndContestFieldsAreRelative) {
    Board b;
    auto& bf = b.state.battlefields[0];
    bf.combat_in_progress = true;
    bf.attacker = P2;
    bf.defender = P1;
    bf.is_contested = true;
    bf.contested_by = P2;
    b.state.turn.focus_holder = P2;
    const auto f1 = b.from(P1), f2 = b.from(P2);
    EXPECT_EQ(f1[L::bf(0) + 6], 0.0f) << "self is attacker";
    EXPECT_EQ(f2[L::bf(0) + 6], 1.0f);
    EXPECT_EQ(f1[L::bf(1) + 6], -1.0f) << "no combat at battlefield 1";
    EXPECT_EQ(f1[L::kExtGlobals + 6 + 0], 0.0f) << "contested by self";
    EXPECT_EQ(f2[L::kExtGlobals + 6 + 0], 1.0f);
    EXPECT_EQ(f1[L::kExtFocusHolder], 0.0f);
    EXPECT_EQ(f2[L::kExtFocusHolder], 1.0f);
}

TEST(FeaturePerspective, ZonesAreSelfDeckTrashBanishmentThenOpponentTrashBanishment) {
    Board b;
    b.inZone(P1, 30, ZoneType::MainDeck, b.state.player(P1).main_deck);
    b.inZone(P1, 31, ZoneType::Trash, b.state.player(P1).trash);
    b.inZone(P1, 32, ZoneType::Banishment, b.state.player(P1).banishment);
    b.inZone(P2, 40, ZoneType::MainDeck, b.state.player(P2).main_deck);
    b.inZone(P2, 41, ZoneType::Trash, b.state.player(P2).trash);
    b.inZone(P2, 42, ZoneType::Banishment, b.state.player(P2).banishment);
    const auto f1 = b.from(P1), f2 = b.from(P2);
    auto at = [](const std::vector<float>& f, int zone, int card) {
        return f[L::zone(zone) + card - 1];
    };
    EXPECT_EQ(at(f1, 0, 30), 1.0f);
    EXPECT_EQ(at(f1, 1, 31), 1.0f);
    EXPECT_EQ(at(f1, 2, 32), 1.0f);
    EXPECT_EQ(at(f1, 3, 41), 1.0f);
    EXPECT_EQ(at(f1, 4, 42), 1.0f);
    EXPECT_EQ(at(f2, 0, 40), 1.0f);
    EXPECT_EQ(at(f2, 1, 41), 1.0f);
    EXPECT_EQ(at(f2, 2, 42), 1.0f);
    EXPECT_EQ(at(f2, 3, 31), 1.0f);
    EXPECT_EQ(at(f2, 4, 32), 1.0f);
    // The opponent's deck is in neither vector.
    for (int z = 0; z < ml::kNumZoneVectors; ++z) {
        EXPECT_EQ(at(f1, z, 40), 0.0f) << "zone " << z;
        EXPECT_EQ(at(f2, z, 30), 0.0f) << "zone " << z;
    }
}

TEST(FeaturePerspective, BattlefieldSlotsSayWhoBroughtThem) {
    Board b;   // slot 0 is Player1's battlefield, slot 1 Player2's
    BattlefieldState token;
    token.id = BattlefieldId::fromRaw(2);
    token.is_token = true;
    b.state.battlefields.push_back(token);
    const auto f1 = b.from(P1), f2 = b.from(P2);
    EXPECT_EQ(f1[L::kBfContributedBy + 0], 1.0f);
    EXPECT_EQ(f1[L::kBfContributedBy + 1], 2.0f);
    EXPECT_EQ(f2[L::kBfContributedBy + 0], 2.0f);
    EXPECT_EQ(f2[L::kBfContributedBy + 1], 1.0f);
    EXPECT_EQ(f1[L::kBfContributedBy + 2], 0.0f) << "a token battlefield is nobody's";
    EXPECT_EQ(f1[L::kBfContributedBy + 3], 0.0f) << "no battlefield in the slot";
    EXPECT_EQ(L::kPadding, L::kBfContributedBy + 4);
    EXPECT_EQ(f1.size(), static_cast<size_t>(ml::kStateFeatureDim));
}

// plans/09 item 9.UU1: the vector's length is a multiple of 16 floats (64
// bytes); what rounds it up is a named block at the end that is always zero.
TEST(FeaturePerspective, TheVectorEndsInZeroPaddingToAMultipleOfSixteenFloats) {
    EXPECT_EQ(ml::kStateFeatureDim % 16, 0);
    EXPECT_EQ(ml::kStateFeatureDim, 10944);
    EXPECT_EQ(L::kPaddingDim, 12);
    EXPECT_EQ(ml::kStateFeatureDim, L::kPadding + L::kPaddingDim);
    EXPECT_LT(L::kPaddingDim, 16);
    Board b;
    // Fill everything the extractor reads near the end, then look at the tail.
    b.state.battlefields[0].controller = P1;
    b.state.turn.focus_holder = P2;
    b.inZone(P2, 2048, ZoneType::Banishment, b.state.player(P2).banishment);
    for (PlayerId p : {P1, P2}) {
        const auto f = b.from(p);
        ASSERT_EQ(f.size(), static_cast<size_t>(ml::kStateFeatureDim));
        for (int i = L::kPadding; i < L::kEnd; ++i) EXPECT_EQ(f[i], 0.0f) << "padding float " << i;
        // The highest card id has a slot: the last float of the last zone block.
        EXPECT_EQ(f[L::kPlayLockout - 1], p == P1 ? 1.0f : 0.0f);
    }
    // The id ranges the constant is sized for.
    EXPECT_EQ(ml::kCardVocabSize, 2048);
    EXPECT_EQ(ml::kLastRegisteredCardId, 955);
    EXPECT_EQ(ml::kRadianceLastCardId - ml::kRadianceFirstCardId + 1, 210);
    EXPECT_EQ(ml::kLegacyLastCardId - ml::kLegacyFirstCardId + 1, 400);
    EXPECT_EQ(ml::kCardVocabSize - ml::kSpareFirstCardId + 1, 483);
}

// plans/09 item 9.UU1: a test-only card must not take an id a set will
// register. Every `CardDefId kName = N;` of the test sources is a registered
// card (N <= 955), sits in the spare range, or is past the id space altogether.
TEST(FeaturePerspective, NoTestCardIdSitsInARangeReservedForASet) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    const std::filesystem::path tests = std::filesystem::path(root ? root : "..") / "tests";
    const std::regex id_constant(R"(CardDefId\s+k\w+\s*=\s*(\d+)\s*;)");
    int constants = 0, spare = 0, outside = 0;
    std::string offenders;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tests)) {
        const auto ext = entry.path().extension();
        if (ext != ".cpp" && ext != ".h") continue;
        std::ifstream in(entry.path());
        std::string line;
        while (std::getline(in, line)) {
            std::smatch m;
            if (!std::regex_search(line, m, id_constant)) continue;
            const int n = std::stoi(m[1]);
            ++constants;
            if (n >= ml::kSpareFirstCardId && n <= ml::kCardVocabSize) ++spare;
            else if (n > ml::kCardVocabSize) ++outside;   // no slot anywhere: collides with nothing
            else if (n > ml::kLastRegisteredCardId)
                offenders += " " + entry.path().filename().string() + ":" + std::to_string(n);
        }
    }
    EXPECT_GT(constants, 500);
    EXPECT_GT(spare, 50) << "the test-only cards";
    EXPECT_LE(outside, 3) << "ids past the id space (test_review_fix_integrity.cpp has three on purpose)";
    EXPECT_TRUE(offenders.empty()) << "ids in a set's reserved range:" << offenders;
}

// ─── (b), (c): every decision of real games ─────────────────────────────────

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

class FeaturePerspectiveGames : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        fiora_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_));
        yi_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_));
    }
    static void TearDownTestSuite() {
        delete yi_; delete fiora_; delete db_; delete registry_;
    }

    /// Calls `at(state)` at every decision of `games` random games of each of
    /// the four pairings of the two decks (both mirror matches included).
    template <typename F>
    static long walk(int games, F&& at) {
        long decisions = 0;
        const DeckSubmission* decks[2] = {fiora_, yi_};
        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {
                for (int g = 1; g <= games; ++g) {
                    const uint64_t seed = 1000u * (2 * a + b) + g;
                    EventBus bus;
                    GameEngine engine(*db_, bus, *registry_);
                    engine.beginGame(*decks[a], *decks[b], seed);
                    std::mt19937_64 rng(seed * 7919);
                    while (!engine.isStepDone()) {
                        const auto& step = engine.currentStep();
                        at(engine.state());
                        if (::testing::Test::HasFatalFailure()) return decisions;
                        ++decisions;
                        std::uniform_int_distribution<size_t> pick(0, step.legal.size() - 1);
                        engine.applyChoice(static_cast<int>(pick(rng)));
                    }
                }
            }
        }
        return decisions;
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static DeckSubmission* fiora_;
    static DeckSubmission* yi_;
};

CardRegistry* FeaturePerspectiveGames::registry_ = nullptr;
CardDB* FeaturePerspectiveGames::db_ = nullptr;
DeckSubmission* FeaturePerspectiveGames::fiora_ = nullptr;
DeckSubmission* FeaturePerspectiveGames::yi_ = nullptr;

TEST_F(FeaturePerspectiveGames, SeatSwapGivesTheSameVectorAtEveryDecision) {
    std::map<std::string, long> differing;   // block -> positions that differed
    long compared = 0, states_differing = 0;
    long with_controller = 0, with_units_both_sides = 0;
    std::string first;
    const long decisions = walk(6, [&](const GameState& state) {
        const GameState mirror = mirrorSeats(state);
        bool any = false;
        for (PlayerId me : {P1, P2}) {
            const auto here = ml::extractStateFeatures(state, me, *db_);
            const auto there = ml::extractStateFeatures(mirror, other(me), *db_);
            ASSERT_EQ(here.size(), there.size());
            ++compared;
            for (size_t i = 0; i < here.size(); ++i) {
                if (here[i] == there[i]) continue;
                any = true;
                ++differing[blockOf(static_cast<int>(i))];
                if (first.empty()) {
                    first = "feature " + std::to_string(i) + " (" +
                            blockOf(static_cast<int>(i)) + "): " +
                            std::to_string(here[i]) + " from " + toString(me) +
                            ", " + std::to_string(there[i]) +
                            " in the mirrored position from the other seat";
                }
            }
        }
        if (any) ++states_differing;
        // The walk must reach the positions the old layout got wrong.
        bool controlled = false, both = false;
        for (const auto& bf : state.battlefields) {
            if (bf.controller.has_value()) controlled = true;
            if (bf.hasUnitsFrom(P1, state.objects) && bf.hasUnitsFrom(P2, state.objects))
                both = true;
        }
        if (controlled) ++with_controller;
        if (both) ++with_units_both_sides;
    });
    std::cout << "[ " << decisions << " decisions of 24 games, " << compared
              << " vector pairs compared; " << with_controller
              << " positions with a controlled battlefield, " << with_units_both_sides
              << " with units of both players at one battlefield ]\n";
    for (const auto& [block, n] : differing)
        std::cout << "[   differing features in " << block << ": " << n << " ]\n";
    EXPECT_GT(decisions, 3000);
    EXPECT_GT(with_controller, 500);
    EXPECT_GT(with_units_both_sides, 50);
    EXPECT_EQ(states_differing, 0) << "first difference: " << first;
    EXPECT_TRUE(differing.empty());
}

TEST_F(FeaturePerspectiveGames, MirroringTwiceIsTheSamePositionAndTheSeatsDoDiffer) {
    // The mirror used above is an involution, and it is not the identity on
    // the features: the same seat sees a different vector in the mirror.
    long same_seat_differs = 0;
    const long decisions = walk(1, [&](const GameState& state) {
        const GameState mirror = mirrorSeats(state);
        const GameState back = mirrorSeats(mirror);
        for (PlayerId me : {P1, P2}) {
            const auto f = ml::extractStateFeatures(state, me, *db_);
            ASSERT_EQ(f, ml::extractStateFeatures(back, me, *db_));
            if (f != ml::extractStateFeatures(mirror, me, *db_)) ++same_seat_differs;
        }
    });
    EXPECT_GT(same_seat_differs, decisions) << "more than half of the (state, seat) pairs";
}

TEST_F(FeaturePerspectiveGames, HiddenInformationStaysMaskedAtEveryDecision) {
    // What a player cannot know: which cards the opponent holds, the order of
    // either Main Deck, which cards make up the opponent's Main Deck, and the
    // opponent's facedown cards. Re-deal all of it; the player's vector must
    // not move. (The same re-deal seen by the opponent does move it.)
    long redeals = 0, opp_noticed = 0, facedown_checked = 0;
    const long decisions = walk(3, [&](const GameState& state) {
        for (PlayerId me : {P1, P2}) {
            const PlayerId opp = other(me);
            GameState redealt = state;
            auto& ops = redealt.player(opp);
            std::mt19937_64 rng(state.decision_index * 31 + (me == P1 ? 1 : 2));
            // Deal the opponent's hand, deck and facedown cards afresh: the
            // same objects, their identities permuted among them.
            // (Except a card the player has been shown, by the knowledge
            // tracker's record: a unit returned to its owner's hand with
            // its ability still on the chain is one.)
            const knowledge::GameKnowledgeView knows(state, me);
            auto unseen = [&](GameObjectId id) { return !knows.about(id).identity_known; };
            std::vector<GameObjectId> hidden;
            for (auto id : ops.hand) if (unseen(id)) hidden.push_back(id);
            for (auto id : ops.main_deck) if (unseen(id)) hidden.push_back(id);
            for (const auto& bf : redealt.battlefields) {
                for (auto fid : bf.facedown) {
                    if (redealt.getObject(fid).controller == opp && unseen(fid)) {
                        hidden.push_back(fid);
                        ++facedown_checked;
                    }
                }
            }
            std::vector<CardDefId> defs;
            for (auto id : hidden) defs.push_back(redealt.getObject(id).card_def_id);
            std::shuffle(defs.begin(), defs.end(), rng);
            for (size_t i = 0; i < hidden.size(); ++i)
                redealt.getObject(hidden[i]).card_def_id = defs[i];
            std::shuffle(ops.main_deck.begin(), ops.main_deck.end(), rng);
            std::shuffle(redealt.player(me).main_deck.begin(),
                         redealt.player(me).main_deck.end(), rng);
            ++redeals;
            ASSERT_EQ(ml::extractStateFeatures(state, me, *db_),
                      ml::extractStateFeatures(redealt, me, *db_))
                << toString(me) << " saw a re-deal of what it cannot know, decision "
                << state.decision_index;
            if (ml::extractStateFeatures(state, opp, *db_) !=
                ml::extractStateFeatures(redealt, opp, *db_))
                ++opp_noticed;
        }
    });
    std::cout << "[ " << redeals << " re-deals over " << decisions << " decisions ("
              << facedown_checked << " facedown cards among them); the opponent's own vector "
              << "changed in " << opp_noticed << " ]\n";
    EXPECT_GT(opp_noticed, redeals / 2) << "the re-deal is real: its owner sees it";
}

} // namespace riftbound::test::feature_perspective
