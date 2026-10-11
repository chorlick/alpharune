/// @file test_review_fix_sweep_flow.cpp
/// Regression tests for the 2026-10 engine review, stage 3 sweep, flow stream:
/// action generation, game flow, combat, finalization and the target / play
/// location virtuals on Card (plans/STATUS.md carry-over rows, plans/05 item
/// 5.4, and the BLOCKED card items that waited on those engine pieces).
/// Every test drives a real GameEngine: intents go through executeIntent / the
/// generators, triggers through TriggerManager and the chain. Mechanisms are
/// exercised with small test-only cards registered in a reserved id range;
/// each unblocked printed card gets a test of its own.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <gtest/gtest.h>

#include <boost/signals2/connection.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_sweep_flow {  // file-unique: test sources are unity-built
namespace {

// ─── Test-only cards (ids in the reserved 956..1150 range) ──────────────────

constexpr CardDefId kTVanilla = 1060;          // unit, free, 2 [M]
constexpr CardDefId kTConquerWatcher = 1061;   // unit: "When I conquer" counter
constexpr CardDefId kTRecycleWatcher = 1062;   // unit: "When you recycle a rune" counter
constexpr CardDefId kTEquipGear = 1063;        // Equipment, Equip [A]
constexpr CardDefId kTRoleSpell = 1064;        // spell: unit A gets 1 damage, unit B gets a buff
constexpr CardDefId kTReactionAbility = 1065;  // unit with a [Reaction] ability
constexpr CardDefId kTHiddenSpell = 1066;      // [Hidden] spell, no targets
constexpr CardDefId kTHiddenChampion = 1067;   // [Hidden] champion unit
constexpr CardDefId kTFiveCost = 1068;         // spell costing [5]
constexpr CardDefId kTTwoExact = 1069;         // spell: exactly two units
constexpr CardDefId kTFinalizePrompts = 1070;  // trigger asking kPrompts prompts at finalization
constexpr CardDefId kTResolvePrompts = 1071;   // spell asking kPrompts prompts as it resolves
constexpr CardDefId kTEndlessPrompts = 1072;   // trigger that never stops asking
constexpr CardDefId kTDeathSummoner = 1073;    // unit: "When I die, play a 1 [M] token here"
constexpr CardDefId kTOpenBfUnit = 1074;       // unit: "You may play me to an open battlefield."
constexpr CardDefId kTPoke = 1075;             // spell: choose a unit (no effect)
constexpr CardDefId kTHiddenTwoHere = 1076;    // [Hidden] spell: exactly two units
constexpr CardDefId kTAnthem = 1077;           // unit: other friendly units have +1 [M]
constexpr CardDefId kTBigVanilla = 1078;       // unit, free, 4 [M]

constexpr CardDefId kDefy = 45;
constexpr CardDefId kRebuttal = 939;
constexpr CardDefId kRockfallPath = 530;
constexpr CardDefId kVilemawsLair = 290;
constexpr CardDefId kSneakyDeckhand = 176;
constexpr CardDefId kDeadbloomPredator = 161;
constexpr CardDefId kBlitzcrankImpassive = 67;
constexpr CardDefId kDeathgrip = 484;
constexpr CardDefId kSmokeAndMirrors = 645;
constexpr CardDefId kVexApathetic = 712;
constexpr CardDefId kMageseekerInvestigator = 725;
constexpr CardDefId kArachnoidHorror = 679;
constexpr CardDefId kAspirantsClimb = 271;
constexpr CardDefId kLeonaZealot = 79;
constexpr CardDefId kChemBaroness = 556;
constexpr CardDefId kFindYourCenter = 47;
constexpr CardDefId kCorruptedDragon = 878;
constexpr CardDefId kPoppyParagon = 678;
constexpr CardDefId kWujuMaster = 784;
constexpr CardDefId kProdigalExplorer = 514;
constexpr CardDefId kFioraVictorious = 232;
constexpr CardDefId kYoneBlademaster = 544;
constexpr CardDefId kCrowdFavorite = 664;
constexpr CardDefId kAlphaWildclaw = 619;

constexpr int kPrompts = 40;          // more than the old finalize cap of 16
constexpr int kResolvePrompts = 100;  // more than the old resolve-loop cap of 64

CardDef makeDef(CardDefId id, const char* name, CardType type) {
    CardDef d;
    d.id = id;
    d.name = name;
    d.card_type = type;
    return d;
}

#define TEST_CARD_DEF(expr)                                   \
    const CardDef& def() const override {                     \
        static const CardDef d = [] { return (expr); }();     \
        return d;                                             \
    }

class TVanilla : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTVanilla, "Test Vanilla", CardType::Unit);
                       d.might = 2; return d; }())
};

class TConquerWatcher : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTConquerWatcher, "Test Conquer Watcher", CardType::Unit);
                       d.might = 2; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["fired"]++;
    }
};

class TRecycleWatcher : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTRecycleWatcher, "Test Recycle Watcher", CardType::Unit);
                       d.might = 2; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenYouRecycleARune; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["fired"]++;
    }
};

class TEquipGear : public GearCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTEquipGear, "Test Equip Blade", CardType::Gear);
                       d.tags = {"Equipment"}; d.might_bonus = 1; return d; }())
    bool hasEquipAbility() const override { return true; }
    // Corrected (sweep-costs): an Equip cost is declared (Card::equipCost) and
    // charged by the engine, not paid inside onEquip.
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .power = 1, .power_any_domain = true};
    }
};

// "Deal 1 to a friendly unit. Give another friendly unit +3 [M] this turn."
// The two targets are roles: the first is damaged, the second is pumped.
class TRoleSpell : public SpellCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTRoleSpell, "Test Role Spell", CardType::Spell);
                       d.keywords.set(Keyword::Reaction); return d; }())
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const GameObjectId hurt = chosenTarget(ctx, targets, 0);
        const GameObjectId pump = chosenTarget(ctx, targets, 1);
        if (hurt != kInvalidId) ctx.executor.dealDamage(hurt, 1, ctx.source);
        if (pump != kInvalidId) ctx.executor.giveTemporaryMight(pump, 3);
    }
};

class TReactionAbility : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTReactionAbility, "Test Reaction Ability", CardType::Unit);
                       d.might = 2; return d; }())
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.is_reaction = true;   // "[Reaction][>] ..." — not also flagged is_action
        return {ab};
    }
    void onActivate(CardContext& ctx, int, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["resolved"]++;
    }
};

class THiddenSpell : public SpellCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTHiddenSpell, "Test Hidden Spell", CardType::Spell);
                       d.keywords.set(Keyword::Hidden); return d; }())
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.player(ctx.controller).xp += 1;
    }
};

class THiddenChampion : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTHiddenChampion, "Test Hidden Champion", CardType::Unit);
                       d.might = 3; d.super_type = SuperType::Champion;
                       d.keywords.set(Keyword::Hidden); return d; }())
};

class TFiveCost : public SpellCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTFiveCost, "Test Five Cost", CardType::Spell);
                       d.energy_cost = 5; return d; }())
};

class TTwoExact : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTTwoExact, "Test Two Exact", CardType::Spell))
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
};

// Asks `limit` yes/no prompts one after another through the executor's choice
// slot, counting the answers in resume_data[kSlot]. Returns true when done.
constexpr size_t kPromptSlot = 8;   // past the helpers' slots (Card::kChoiceStateSlot)
bool askPrompts(CardContext& ctx, int limit) {
    auto& ri = *ctx.state.chain.resuming;
    while (ri.resume_data.size() <= kPromptSlot + 1) ri.resume_data.push_back(0);
    if (ri.resume_data[kPromptSlot + 1] == 1) {          // a prompt is out: read it
        takenValue(ctx, 0);
        ri.resume_data[kPromptSlot + 1] = 0;
        ri.resume_data[kPromptSlot]++;
    }
    if (limit >= 0 && ri.resume_data[kPromptSlot] >= limit) return true;
    ctx.executor.requestChoice(ctx.controller,
        {valChoice(ctx.controller, 0, "no"), valChoice(ctx.controller, 1, "yes")},
        "test prompt");
    ri.resume_data[kPromptSlot + 1] = 1;
    return false;
}

class TFinalizePrompts : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTFinalizePrompts, "Test Finalize Prompts", CardType::Unit);
                       d.might = 2; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        if (!askPrompts(ctx, kPrompts)) return;
        if (finalizingChoices(ctx)) return;
        ctx.state.getObject(ctx.source).card_counters["answers"] =
            ctx.state.chain.resuming->resume_data[kPromptSlot];
    }
};

class TResolvePrompts : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTResolvePrompts, "Test Resolve Prompts", CardType::Spell))
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        if (!askPrompts(ctx, kResolvePrompts)) return;
        ctx.state.player(ctx.controller).xp = ctx.state.chain.resuming->resume_data[kPromptSlot];
    }
};

class TEndlessPrompts : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTEndlessPrompts, "Test Endless Prompts", CardType::Unit);
                       d.might = 2; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        askPrompts(ctx, -1);   // never satisfied
    }
};

class TDeathSummoner : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTDeathSummoner, "Test Death Summoner", CardType::Unit);
                       d.might = 1; d.keywords.set(Keyword::Deathknell); return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.last_location.has_value()) return;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Test Recruit", 1, {},
                                 KeywordSet{}, *self.last_location);
    }
};

class TOpenBfUnit : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTOpenBfUnit, "Test Open-Field Unit", CardType::Unit);
                       d.might = 2; return d; }())
    unsigned extraPlayLocations() const override { return kPlayToOpenBattlefield; }
};

class TPoke : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTPoke, "Test Poke", CardType::Spell))
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1};
    }
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, o] : state.objects)
            if ((o.isUnit() || o.isGear()) && o.location.has_value()) out.push_back(id);
        return out;
    }
};

class THiddenTwoHere : public SpellCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTHiddenTwoHere, "Test Hidden Two", CardType::Spell);
                       d.keywords.set(Keyword::Hidden); return d; }())
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
};

// "Other friendly units have +1 [M]."
class TAnthem : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTAnthem, "Test Anthem", CardType::Unit);
                       d.might = 1; return d; }())
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller ||
                !self.location.has_value()) continue;
            for (auto& [uid, u] : state.objects) {
                if (uid == sid || !u.isUnit() || u.controller != controller ||
                    !u.location.has_value()) continue;
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.might_bonus = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
};

class TBigVanilla : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTBigVanilla, "Test Big Vanilla", CardType::Unit);
                       d.might = 4; return d; }())
};

// ─── Agents ─────────────────────────────────────────────────────────────────

/// Passes priority / focus when it can; answers object choices with `want`
/// (or the first offered) and value choices with `value`. Counts prompts.
class DrivingAgent : public AgentInterface {
public:
    GameObjectId want = kInvalidId;
    int value = 0;
    int value_prompts = 0;
    int object_prompts = 0;
    int passes = 0;
    int passes_at_last_value_prompt = -1;
    std::vector<std::vector<Intent>> seen;   // every non-choice decision's options
    /// Returns an intent to take instead of passing (once), if it wants one.
    std::function<const Intent*(const std::vector<Intent>&)> respond;

    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool object_choice = false, value_choice = false;
        for (const auto& i : legal) {
            if (i.type != IntentType::MakeChoice) continue;
            if (!i.chosen_objects.empty()) object_choice = true;
            else if (i.chosen_value.has_value()) value_choice = true;
        }
        if (object_choice) {
            ++object_prompts;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            return legal.front();
        }
        if (value_choice) {
            ++value_prompts;
            passes_at_last_value_prompt = passes;
            for (const auto& i : legal)
                if (i.chosen_value.value_or(-1) == value) return i;
            return legal.front();
        }
        seen.push_back(legal);
        if (respond) {
            if (const Intent* r = respond(legal)) {
                Intent out = *r;
                respond = nullptr;
                return out;
            }
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) {
                ++passes;
                return i;
            }
        for (const auto& i : legal)
            if (i.type == IntentType::EndTurn) return i;
        return legal.front();
    }
};

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

// ─── Fixture ────────────────────────────────────────────────────────────────

class SweepFlowTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    DrivingAgent a1_, a2_;

    GameState& s() { return engine_->mutableState(); }

    void SetUp() override {
        CardTestFixture::SetUp();
        card_registry.registerCard(kTVanilla, std::make_unique<TVanilla>());
        card_registry.registerCard(kTConquerWatcher, std::make_unique<TConquerWatcher>());
        card_registry.registerCard(kTRecycleWatcher, std::make_unique<TRecycleWatcher>());
        card_registry.registerCard(kTEquipGear, std::make_unique<TEquipGear>());
        card_registry.registerCard(kTRoleSpell, std::make_unique<TRoleSpell>());
        card_registry.registerCard(kTReactionAbility, std::make_unique<TReactionAbility>());
        card_registry.registerCard(kTHiddenSpell, std::make_unique<THiddenSpell>());
        card_registry.registerCard(kTHiddenChampion, std::make_unique<THiddenChampion>());
        card_registry.registerCard(kTFiveCost, std::make_unique<TFiveCost>());
        card_registry.registerCard(kTTwoExact, std::make_unique<TTwoExact>());
        card_registry.registerCard(kTFinalizePrompts, std::make_unique<TFinalizePrompts>());
        card_registry.registerCard(kTResolvePrompts, std::make_unique<TResolvePrompts>());
        card_registry.registerCard(kTEndlessPrompts, std::make_unique<TEndlessPrompts>());
        card_registry.registerCard(kTDeathSummoner, std::make_unique<TDeathSummoner>());
        card_registry.registerCard(kTOpenBfUnit, std::make_unique<TOpenBfUnit>());
        registerMoreCards();
        card_db.buildFromClasses(card_registry);
    }
    void registerMoreCards();   // defined with the cards further down

    /// An engine in P1's main phase, Neutral Open, with `n_bfs` battlefields
    /// and a blank legend each.
    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.turn_number = 3;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        for (PlayerId p : {P1, P2}) {
            auto id = st.createObject();
            auto& lo = st.getObject(id);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = id;
        }
    }

    /// A card object built the way GameEngine::instantiateCard builds one.
    GameObjectId makeCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.card_def_id = def_id;
        c.owner = c.controller = p;
        c.name = def.name;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.tags = def.tags;
        c.domains = def.domains;
        c.base_might = c.current_might = def.might;
        c.keywords = def.keywords;
        c.assault_value = def.assault_value;
        c.shield_value = def.shield_value;
        c.deflect_value = def.deflect_value;
        c.might_bonus = def.might_bonus;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId inChampionZone(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::ChampionZone;
        s().player(p).champion_zone = id;
        return id;
    }
    /// On the board: at battlefield `at_bf`, or in `p`'s base when negative.
    GameObjectId onBoard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = makeCard(p, def_id);
        auto& c = s().getObject(id);
        if (at_bf >= 0) {
            c.zone = ZoneType::BattlefieldZone;
            c.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            c.zone = ZoneType::Base;
            c.location = BaseLocation{p};
        }
        return id;
    }
    /// Facedown at battlefield `bf`, hidden on an earlier turn.
    GameObjectId facedown(PlayerId p, CardDefId def_id, int bf) {
        auto id = makeCard(p, def_id);
        auto& c = s().getObject(id);
        c.zone = ZoneType::FacedownZone;
        c.is_hidden = true;
        c.hidden_at = BattlefieldId::fromRaw(bf);
        c.hidden_on_turn = s().turn.turn_number - 1;
        s().battlefields[bf].facedown.push_back(id);
        return id;
    }
    GameObjectId addRuneOnBoard(PlayerId p, Domain d) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        return id;
    }
    /// Give battlefield `bf` a card object of `def_id`.
    GameObjectId setBattlefieldCard(int bf, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.card_def_id = def_id;
        c.card_type = CardType::Battlefield;
        c.name = card_db.get(def_id).name;
        c.owner = c.controller = PlayerId::None;
        s().battlefields[bf].card_object_id = id;
        return id;
    }
    void control(int bf, PlayerId p) { s().battlefields[bf].controller = p; }

    Intent playIntent(PlayerId p, GameObjectId card,
                      std::optional<LocationId> loc = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = loc;
        return i;
    }
    std::vector<Intent> playsOf(const std::vector<Intent>& legal, GameObjectId card) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if ((i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                 i.type == IntentType::PlayReaction) && i.card == card) out.push_back(i);
        return out;
    }
    static bool playsTo(const std::vector<Intent>& plays, LocationId loc) {
        return std::any_of(plays.begin(), plays.end(), [&](const Intent& i) {
            return i.play_location.has_value() && *i.play_location == loc; });
    }
    std::vector<Intent> movesOf(const std::vector<Intent>& legal, GameObjectId unit) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if (i.type == IntentType::StandardMove &&
                std::find(i.units_to_move.begin(), i.units_to_move.end(), unit) !=
                    i.units_to_move.end()) out.push_back(i);
        return out;
    }
    std::vector<Intent> activationsOf(const std::vector<Intent>& legal, GameObjectId src) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if ((i.type == IntentType::ActivateAbility ||
                 i.type == IntentType::ActivateActionAbility ||
                 i.type == IntentType::ActivateReactionAbility) && i.ability_source == src)
                out.push_back(i);
        return out;
    }
    static bool has(const std::vector<Intent>& legal, IntentType t) {
        return std::any_of(legal.begin(), legal.end(),
                           [&](const Intent& i) { return i.type == t; });
    }
    /// Put the engine in a showdown with `focus` holding focus.
    void enterShowdown(PlayerId focus) {
        s().turn.ns_state = NeutralShowdownState::Showdown;
        s().turn.oc_state = OpenClosedState::Open;
        s().turn.focus_holder = focus;
        s().turn.priority_holder = focus;
    }
    /// Put a spell of `def_id` on the chain for `p` and close the state with
    /// `holder` holding priority.
    GameObjectId spellOnChain(PlayerId p, CardDefId def_id, PlayerId holder) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Chain;
        ChainItem item;
        item.id = s().chain.allocateId();
        item.source = id;
        item.controller = p;
        item.card_def_id = def_id;
        item.is_spell = true;
        item.status = ChainItemStatus::Finalized;
        s().chain.items.push_back(item);
        s().turn.oc_state = OpenClosedState::Closed;
        s().turn.priority_holder = holder;
        return id;
    }
};

// ═══ 1. 05 item 5.4: play-location and battlefield text is a Card virtual ═══

TEST_F(SweepFlowTest, PlayLocation_OpenBattlefieldPermissionComesFromTheCard) {
    makeEngine();
    control(0, P1);                      // BF#1 stays open
    auto plain = inHand(P1, kTVanilla);
    auto roamer = inHand(P1, kTOpenBfUnit);
    auto deckhand = inHand(P1, kSneakyDeckhand);
    s().player(P1).rune_pool.energy = 20;
    s().player(P1).rune_pool.universal_power = 20;
    const auto legal = engine_->generateLegalActions();
    const LocationId open_bf{BattlefieldLocation{1}};
    EXPECT_FALSE(playsTo(playsOf(legal, plain), open_bf));
    EXPECT_TRUE(playsTo(playsOf(legal, roamer), open_bf));
    EXPECT_TRUE(playsTo(playsOf(legal, deckhand), open_bf))
        << "Sneaky Deckhand: \"You may play me to an open battlefield.\"";
}

TEST_F(SweepFlowTest, PlayLocation_OccupiedEnemyBattlefieldPermissionComesFromTheCard) {
    makeEngine();
    control(0, P2);
    onBoard(P2, kTVanilla, 0);
    control(1, P2);                      // enemy-controlled but empty: not "occupied"
    auto predator = inHand(P1, kDeadbloomPredator);
    auto plain = inHand(P1, kTVanilla);
    s().player(P1).rune_pool.energy = 20;
    s().player(P1).rune_pool.universal_power = 20;
    const auto legal = engine_->generateLegalActions();
    EXPECT_TRUE(playsTo(playsOf(legal, predator), LocationId{BattlefieldLocation{0}}));
    EXPECT_FALSE(playsTo(playsOf(legal, predator), LocationId{BattlefieldLocation{1}}));
    EXPECT_FALSE(playsTo(playsOf(legal, plain), LocationId{BattlefieldLocation{0}}));
}

TEST_F(SweepFlowTest, PlayLocation_ATriggerThatMentionsPlayingToABattlefieldGrantsNothing) {
    // Blitzcrank, Impassive: "When you play me to a battlefield, ..." is a
    // trigger condition, not a permission (CR 355.2). The old text match on
    // "play me to a battlefield" let it be played anywhere.
    makeEngine();
    control(0, P2);
    onBoard(P2, kTVanilla, 0);
    auto blitz = inHand(P1, kBlitzcrankImpassive);
    s().player(P1).rune_pool.energy = 20;
    s().player(P1).rune_pool.universal_power = 20;
    const auto plays = playsOf(engine_->generateLegalActions(), blitz);
    EXPECT_TRUE(playsTo(plays, LocationId{BaseLocation{P1}}));
    EXPECT_FALSE(playsTo(plays, LocationId{BattlefieldLocation{0}}));
    EXPECT_FALSE(playsTo(plays, LocationId{BattlefieldLocation{1}}));
}

TEST_F(SweepFlowTest, BattlefieldRestrictions_AreReadFromTheCardAtSetup) {
    // A real game start: each player brings one battlefield.
    auto d1 = DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), card_db);
    auto d2 = DeckValidator::loadFromDeckList(deckFile("fiora.txt"), card_db);
    d1.battlefields = {kRockfallPath};
    d2.battlefields = {kVilemawsLair};
    EventBus bus;
    GameEngine engine(card_db, bus, card_registry);
    auto step = engine.beginGame(d1, d2, 7);
    for (int i = 0; i < 8 && engine.state().battlefields.size() < 2 &&
                    step.kind == StepKind::NeedDecision; ++i)
        step = engine.applyChoice(0);
    ASSERT_EQ(engine.state().battlefields.size(), 2u);
    const auto& bfs = engine.state().battlefields;
    EXPECT_TRUE(bfs[0].blocks_unit_play) << "Rockfall Path: \"Units can't be played here.\"";
    EXPECT_FALSE(bfs[0].blocks_move_to_base);
    EXPECT_TRUE(bfs[1].blocks_move_to_base)
        << "Vilemaw's Lair: \"Units can't move from here to base.\"";
    EXPECT_FALSE(bfs[1].blocks_unit_play);
}

// ═══ 2. Target legality that needs card data ════════════════════════════════

TEST_F(SweepFlowTest, Defy_IsNotOfferedAgainstASpellItCannotCounter) {
    // "Counter a spell that costs no more than [4] and no more than [A]."
    makeEngine();
    spellOnChain(P2, kTFiveCost, P1);
    auto defy = inHand(P1, kDefy);
    s().player(P1).rune_pool.energy = 5;
    s().player(P1).rune_pool.universal_power = 5;
    EXPECT_TRUE(playsOf(engine_->generateLegalActions(), defy).empty())
        << "a [5] spell is not a legal target, so Defy is not playable";
}

TEST_F(SweepFlowTest, Defy_IsOfferedAgainstACheapSpell) {
    makeEngine();
    spellOnChain(P2, kTHiddenSpell, P1);   // costs nothing
    auto defy = inHand(P1, kDefy);
    s().player(P1).rune_pool.energy = 5;
    s().player(P1).rune_pool.universal_power = 5;
    EXPECT_EQ(playsOf(engine_->generateLegalActions(), defy).size(), 1u);
}

TEST_F(SweepFlowTest, Rebuttal_IsNotOfferedAgainstASpellCostingMoreThanFour) {
    // "Choose a spell with Energy cost no more than [4]."
    makeEngine();
    spellOnChain(P2, kTFiveCost, P1);
    auto rebuttal = inHand(P1, kRebuttal);
    s().player(P1).rune_pool.energy = 9;
    s().player(P1).rune_pool.universal_power = 9;
    EXPECT_TRUE(playsOf(engine_->generateLegalActions(), rebuttal).empty());
}

TEST_F(SweepFlowTest, Rebuttal_IsOfferedAgainstACheapSpell) {
    makeEngine();
    spellOnChain(P2, kTHiddenSpell, P1);
    auto rebuttal = inHand(P1, kRebuttal);
    s().player(P1).rune_pool.energy = 9;
    s().player(P1).rune_pool.universal_power = 9;
    EXPECT_EQ(playsOf(engine_->generateLegalActions(), rebuttal).size(), 1u);
}

TEST_F(SweepFlowTest, HasLegalTargets_AFixedCountSpellNeedsThatManyTargets) {
    makeEngine();
    onBoard(P1, kTVanilla);
    const Card* two = card_registry.get(kTTwoExact);
    EXPECT_FALSE(two->hasLegalTargets(s(), P1, card_db))
        << "one unit on the board: a spell that chooses exactly two has no legal play";
    onBoard(P2, kTVanilla);
    EXPECT_TRUE(two->hasLegalTargets(s(), P1, card_db));
    auto spell = inHand(P1, kTTwoExact);
    EXPECT_EQ(playsOf(engine_->generateLegalActions(), spell).size(), 2u)
        << "both orders of the two units";
}

// ═══ 3. Target roles survive a target that became illegal ═══════════════════

TEST_F(SweepFlowTest, TargetRoles_TheSecondTargetIsNotPromotedWhenTheFirstIsGone) {
    // The spell damages its first target and pumps its second. P2 responds by
    // killing the first: the second must still be the one that is pumped, and
    // must not take the damage meant for the first.
    makeEngine();
    auto hurt = onBoard(P1, kTVanilla);
    auto pump = onBoard(P1, kTVanilla);
    auto spell = inHand(P1, kTRoleSpell);
    Intent play = playIntent(P1, spell);
    play.targets = {hurt, pump};
    a2_.respond = [&](const std::vector<Intent>& legal) -> const Intent* {
        // P2 "responds" by removing the first target from the board.
        (void)legal;
        engine_->testHook_executor().killObject(hurt);
        return nullptr;
    };
    engine_->testHook_executeIntent(play);
    EXPECT_FALSE(s().getObject(hurt).location.has_value());
    EXPECT_EQ(s().getObject(pump).damage_marked, 0)
        << "the surviving target keeps its own role";
    EXPECT_EQ(s().getObject(pump).temp_might_bonus, 3);
}

TEST_F(SweepFlowTest, TargetRoles_Deathgrip_ThePumpTargetIsNotKilledInPlaceOfTheFirst) {
    // Deathgrip: "Kill a friendly unit. If you do, give +[M] equal to its
    // Might to another friendly unit this turn. Draw 1."
    makeEngine();
    auto kill = onBoard(P1, kTVanilla);
    auto pump = onBoard(P1, kTVanilla);
    auto spell = inHand(P1, kDeathgrip);
    s().player(P1).rune_pool.energy = 9;
    s().player(P1).rune_pool.universal_power = 9;
    Intent play = playIntent(P1, spell);
    play.targets = {kill, pump};
    a2_.respond = [&](const std::vector<Intent>&) -> const Intent* {
        engine_->testHook_executor().bounceToHand(kill);
        return nullptr;
    };
    engine_->testHook_executeIntent(play);
    EXPECT_TRUE(s().getObject(pump).location.has_value())
        << "the unit chosen to be pumped is not killed in the first one's place";
    EXPECT_EQ(s().getObject(pump).temp_might_bonus, 0) << "nothing was killed: no pump";
}

// ═══ 4. Hidden plays and showdowns ══════════════════════════════════════════

TEST_F(SweepFlowTest, Showdown_OffersAReactionAbility) {
    // CR 813.1.b: [Reaction] grants every permission of [Action].
    makeEngine();
    auto unit = onBoard(P1, kTReactionAbility, 0);
    enterShowdown(P1);
    const auto acts = activationsOf(engine_->generateLegalActions(), unit);
    ASSERT_EQ(acts.size(), 1u);
    int count = 0;
    engine_->testHook_resolveShowdownDecision(BattlefieldId::fromRaw(0), acts.front(), P1, count);
    EXPECT_EQ(s().getObject(unit).card_counters["resolved"], 1);
}

TEST_F(SweepFlowTest, Showdown_OffersAnAuraGrantedAbility) {
    makeEngine();
    auto bearer = onBoard(P1, kTVanilla, 0);
    s().getObject(bearer).temp_granted_abilities.push_back({kTReactionAbility, 0, false});
    engine_->testHook_cleanup();          // the aura pass publishes the grant
    enterShowdown(P1);
    const auto acts = activationsOf(engine_->generateLegalActions(), bearer);
    ASSERT_EQ(acts.size(), 1u);
    EXPECT_EQ(acts.front().granted_ability_def, kTReactionAbility);
    int count = 0;
    engine_->testHook_resolveShowdownDecision(BattlefieldId::fromRaw(0), acts.front(), P1, count);
    EXPECT_EQ(s().getObject(bearer).card_counters["resolved"], 1);
}

TEST_F(SweepFlowTest, ClosedState_OffersAnAuraGrantedReactionAbility) {
    makeEngine();
    auto bearer = onBoard(P1, kTVanilla, 0);
    s().getObject(bearer).temp_granted_abilities.push_back({kTReactionAbility, 0, false});
    engine_->testHook_cleanup();
    spellOnChain(P2, kTHiddenSpell, P1);
    const auto acts = activationsOf(engine_->generateLegalActions(), bearer);
    ASSERT_EQ(acts.size(), 1u);
    EXPECT_EQ(acts.front().type, IntentType::ActivateReactionAbility);
}

TEST_F(SweepFlowTest, Showdown_OffersAHiddenCard) {
    // CR 811.1.b: from the turn after it was hidden the card has [Reaction],
    // which includes [Action] timing (CR 813.1.b).
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    auto hidden = facedown(P1, kTHiddenSpell, 0);
    enterShowdown(P1);
    const auto plays = playsOf(engine_->generateLegalActions(), hidden);
    ASSERT_EQ(plays.size(), 1u);
    int count = 0;
    engine_->testHook_resolveShowdownDecision(BattlefieldId::fromRaw(0), plays.front(), P1, count);
    EXPECT_EQ(s().player(P1).xp, 1) << "the hidden spell resolved";
    EXPECT_TRUE(s().battlefields[0].facedown.empty());
}

TEST_F(SweepFlowTest, Hide_AHiddenChampionCanBeHiddenFromTheChampionZone) {
    // CR 811.1.b: "While this card is in your hand or in your Champion Zone".
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    auto champ = inChampionZone(P1, kTHiddenChampion);
    s().player(P1).rune_pool.universal_power = 1;
    const auto legal = engine_->generateLegalActions();
    const Intent* hide = nullptr;
    for (const auto& i : legal)
        if (i.type == IntentType::HideCard && i.card == champ) hide = &i;
    ASSERT_NE(hide, nullptr);
    engine_->testHook_executeIntent(*hide);
    EXPECT_EQ(s().getObject(champ).zone, ZoneType::FacedownZone);
    EXPECT_EQ(s().player(P1).champion_zone, kInvalidId)
        << "the champion left its zone";
    ASSERT_EQ(s().battlefields[0].facedown.size(), 1u);
    EXPECT_EQ(s().battlefields[0].facedown.front(), champ);
}

// ═══ 5. The main-phase decision is never reached with a chain pending ═══════

TEST_F(SweepFlowTest, MainPhase_ATriggerFromAShowdownResolvesBeforeTheNextDecision) {
    // A unit moves to an open battlefield, the showdown closes, it conquers
    // and its "When I conquer" goes on the chain. The turn player must not be
    // handed that chain as their main-phase decision.
    makeEngine();
    auto unit = onBoard(P1, kTConquerWatcher);
    Intent move = Intent::standardMove(P1, {unit}, LocationId{BattlefieldLocation{0}});
    engine_->testHook_executeIntent(move);
    engine_->testHook_advanceMainPhase();
    EXPECT_FALSE(s().chain.exists()) << "nothing is left on the chain";
    EXPECT_TRUE(s().turn.isNeutralOpen());
    EXPECT_EQ(s().getObject(unit).card_counters["fired"], 1);
    EXPECT_TRUE(has(engine_->generateLegalActions(), IntentType::EndTurn))
        << "the decision offered is the turn player's main-phase decision";
}

TEST_F(SweepFlowTest, Equip_ATriggerQueuedWhileEquippingResolves) {
    // Equip [A] recycles a rune, which triggers "When you recycle a rune".
    // No ability item is queued for an Equip, so nothing used to run the chain.
    makeEngine();
    auto watcher = onBoard(P1, kTRecycleWatcher);
    auto bearer = onBoard(P1, kTVanilla);
    auto gear = onBoard(P1, kTEquipGear);
    addRuneOnBoard(P1, Domain::Fury);
    Intent equip;
    equip.type = IntentType::ActivateAbility;
    equip.player = P1;
    equip.ability_source = gear;
    equip.targets = {bearer};
    engine_->testHook_executeIntent(equip);
    ASSERT_EQ(s().getObject(gear).attached_to, std::optional<GameObjectId>(bearer));
    EXPECT_FALSE(s().chain.exists());
    EXPECT_TRUE(s().turn.isNeutralOpen());
    EXPECT_EQ(s().getObject(watcher).card_counters["fired"], 1);
}

// ═══ 6. Finalization and resolve caps ═══════════════════════════════════════

TEST_F(SweepFlowTest, Finalize_ACardMayAskMoreThanSixteenPrompts) {
    makeEngine();
    auto unit = inHand(P1, kTFinalizePrompts);
    a1_.value = 1;
    engine_->testHook_executeIntent(playIntent(P1, unit, LocationId{BaseLocation{P1}}));
    EXPECT_EQ(a1_.value_prompts, kPrompts);
    EXPECT_EQ(a1_.passes_at_last_value_prompt, 0)
        << "every prompt was asked at finalization, before anyone got priority "
           "(the old cap of 16 left the rest to be asked at resolution)";
    EXPECT_EQ(s().getObject(unit).card_counters["answers"], kPrompts)
        << "the effect ran with every answer";
    EXPECT_FALSE(s().chain.exists());
}

TEST_F(SweepFlowTest, Resolve_ACardMayAskManyPrompts) {
    makeEngine();
    auto spell = inHand(P1, kTResolvePrompts);
    a1_.value = 1;
    engine_->testHook_executeIntent(playIntent(P1, spell));
    EXPECT_EQ(a1_.value_prompts, kResolvePrompts);
    EXPECT_EQ(s().player(P1).xp, kResolvePrompts);
}

TEST_F(SweepFlowTest, Finalize_ACardThatNeverStopsAskingIsAnError) {
    makeEngine();
    auto unit = inHand(P1, kTEndlessPrompts);
    EXPECT_THROW(
        engine_->testHook_executeIntent(playIntent(P1, unit, LocationId{BaseLocation{P1}})),
        std::logic_error);
}

// ═══ 4 (cont). Hidden spells: each target is judged on its own ══════════════

TEST_F(SweepFlowTest, HiddenPlay_SmokeAndMirrors_SecondTargetIsFreeOfTheBattlefield) {
    // CR 811.1.d.4: the first unit must be at the battlefield the card was
    // hidden at; the second is "at a different location", so it cannot be.
    makeEngine();
    control(0, P1);
    auto here = onBoard(P1, kTVanilla, 0);
    auto elsewhere = onBoard(P1, kTVanilla);       // base
    auto hidden = facedown(P1, kSmokeAndMirrors, 0);
    spellOnChain(P2, kTHiddenSpell, P1);
    const auto plays = playsOf(engine_->generateLegalActions(), hidden);
    ASSERT_EQ(plays.size(), 1u) << "offered, and only with the first unit here";
    EXPECT_EQ(plays.front().targets, (std::vector<GameObjectId>{here, elsewhere}));
}

TEST_F(SweepFlowTest, HiddenPlay_AnOrdinaryTwoTargetSpellNeedsBothTargetsHere) {
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    onBoard(P1, kTVanilla);                        // base: not an option
    auto hidden = facedown(P1, kTHiddenTwoHere, 0);
    spellOnChain(P2, kTHiddenSpell, P1);
    EXPECT_TRUE(playsOf(engine_->generateLegalActions(), hidden).empty())
        << "CR 811.1.d: no valid targets under the restriction, so not playable";
    onBoard(P2, kTVanilla, 0);
    EXPECT_EQ(playsOf(engine_->generateLegalActions(), hidden).size(), 2u);
}

// ═══ 7. Moves ═══════════════════════════════════════════════════════════════

TEST_F(SweepFlowTest, Vex_ThePlayedUnitCannotBeMovedThisTurn) {
    // Vex, Apathetic: "When an opponent plays a unit while I'm at a
    // battlefield, [Stun] it. They can't move it this turn."
    makeEngine();
    control(0, P1);
    onBoard(P1, kVexApathetic, 0);
    s().turn.turn_player = P2;
    auto unit = inHand(P2, kTVanilla);
    engine_->testHook_executeIntent(playIntent(P2, unit, LocationId{BaseLocation{P2}}));
    ASSERT_TRUE(s().getObject(unit).is_stunned);
    EXPECT_TRUE(s().getObject(unit).cant_move_this_turn);
    s().getObject(unit).is_exhausted = false;     // even readied, it stays put
    EXPECT_TRUE(movesOf(engine_->generateLegalActions(), unit).empty());
    engine_->testHook_expirationStep();
    EXPECT_FALSE(s().getObject(unit).cant_move_this_turn) << "\"this turn\" ended";
    s().getObject(unit).is_exhausted = false;
    s().turn.phase = TurnPhase::MainPhase;        // (the step above left it)
    EXPECT_FALSE(movesOf(engine_->generateLegalActions(), unit).empty());
}

TEST_F(SweepFlowTest, StandardMove_SeveralUnitsMoveAsOneAction) {
    // CR 144.3: several units' Standard Moves as one action, one destination.
    makeEngine();
    auto first = onBoard(P1, kTVanilla);
    auto second = onBoard(P1, kTVanilla);
    auto third = onBoard(P1, kTVanilla);
    a1_.want = second;
    engine_->testHook_executeIntent(
        Intent::standardMove(P1, {first}, LocationId{BattlefieldLocation{0}}));
    EXPECT_EQ(s().getObject(first).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(s().getObject(second).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_TRUE(s().getObject(second).is_exhausted) << "each unit pays its own cost";
    EXPECT_TRUE(s().getObject(third).isAtBase())
        << "asked again after the second, the agent's want was gone: it stopped";
    EXPECT_EQ(a1_.object_prompts, 2);
}

TEST_F(SweepFlowTest, StandardMove_DecliningToAddUnitsMovesOne) {
    makeEngine();
    auto first = onBoard(P1, kTVanilla);
    auto second = onBoard(P1, kTVanilla);
    engine_->testHook_executeIntent(
        Intent::standardMove(P1, {first}, LocationId{BattlefieldLocation{0}}));
    EXPECT_EQ(s().getObject(first).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_TRUE(s().getObject(second).isAtBase());
    EXPECT_FALSE(s().getObject(second).is_exhausted);
}

TEST_F(SweepFlowTest, MageseekerInvestigator_TaxesEachUnitBeyondTheFirst) {
    // "Opponents must pay [A] for each unit beyond the first to move multiple
    //  units to my battlefield at the same time."
    makeEngine();
    control(0, P1);
    onBoard(P1, kMageseekerInvestigator, 0);
    engine_->testHook_recalculateAuras();
    ASSERT_EQ(s().battlefields[0].multi_move_tax[1], 1);
    ASSERT_EQ(s().battlefields[0].multi_move_tax[0], 0) << "not its own controller";
    s().turn.turn_player = P2;
    auto first = onBoard(P2, kTVanilla);
    auto second = onBoard(P2, kTVanilla);
    auto third = onBoard(P2, kTVanilla);
    s().player(P2).rune_pool.universal_power = 1;   // enough for one extra unit
    a2_.want = second;
    engine_->testHook_executeIntent(
        Intent::standardMove(P2, {first}, LocationId{BattlefieldLocation{0}}));
    EXPECT_EQ(s().getObject(second).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(s().player(P2).rune_pool.universal_power, 0) << "[A] paid for the second unit";
    EXPECT_TRUE(s().getObject(third).isAtBase());
    EXPECT_EQ(a2_.object_prompts, 1) << "a third unit could not be paid for: not offered";
}

TEST_F(SweepFlowTest, ArachnoidHorror_MayBePlayedToALoneEnemyUnitsBattlefield) {
    // "I can be played to an occupied battlefield if an enemy unit is alone there."
    makeEngine();
    control(0, P2);
    onBoard(P2, kTVanilla, 0);
    control(1, P2);
    onBoard(P2, kTVanilla, 1);
    onBoard(P2, kTVanilla, 1);                       // two there: not alone
    auto horror = inHand(P1, kArachnoidHorror);
    auto plain = inHand(P1, kTVanilla);
    s().player(P1).rune_pool.energy = 20;
    s().player(P1).rune_pool.universal_power = 20;
    auto legal = engine_->generateLegalActions();
    EXPECT_TRUE(playsTo(playsOf(legal, horror), LocationId{BattlefieldLocation{0}}));
    EXPECT_FALSE(playsTo(playsOf(legal, horror), LocationId{BattlefieldLocation{1}}));
    EXPECT_FALSE(playsTo(playsOf(legal, plain), LocationId{BattlefieldLocation{0}}));
}

TEST_F(SweepFlowTest, ArachnoidHorror_GrantsThePermissionToOtherFriendlyUnits) {
    // "Friendly units can be played to an occupied battlefield if an enemy
    //  unit is alone there."
    makeEngine();
    control(0, P2);
    onBoard(P2, kTVanilla, 0);
    onBoard(P1, kArachnoidHorror);                   // in P1's base
    auto plain = inHand(P1, kTVanilla);
    auto theirs = inHand(P2, kTVanilla);
    engine_->testHook_recalculateAuras();
    EXPECT_TRUE(s().player(P1).grant_units_lone_enemy_bf);
    EXPECT_FALSE(s().player(P2).grant_units_lone_enemy_bf);
    EXPECT_TRUE(playsTo(playsOf(engine_->generateLegalActions(), plain),
                        LocationId{BattlefieldLocation{0}}));
    (void)theirs;
}

// ═══ 8. Combat and scoring ══════════════════════════════════════════════════

TEST_F(SweepFlowTest, Combat_NoResultWithBothPlayersPresentStagesAnotherCombat) {
    // CR 468.2 / 468.3.d.1. The lone defender dies and its "When I die" plays
    // a token here. That trigger resolves before the result is determined, so
    // both players have units: No Result, and a new combat is staged with the
    // same attacker — the attacker does not conquer in between.
    makeEngine();
    control(0, P2);
    onBoard(P2, kTDeathSummoner, 0);
    auto attacker = onBoard(P1, kTVanilla);
    std::vector<PlayerId> attackers;
    std::vector<int> p1_score_at_combat_start;
    boost::signals2::scoped_connection c1 = events.on_combat_started.connect(
        [&](const CombatStartedEvent& e) {
            attackers.push_back(e.attacker);
            p1_score_at_combat_start.push_back(s().player(P1).score);
        });
    engine_->testHook_executeIntent(
        Intent::standardMove(P1, {attacker}, LocationId{BattlefieldLocation{0}}));
    engine_->testHook_cleanup();
    engine_->testHook_advanceMainPhase();
    ASSERT_EQ(attackers.size(), 2u) << "the token's arrival staged a second combat";
    EXPECT_EQ(attackers[1], P1) << "the same player attacks again";
    EXPECT_EQ(p1_score_at_combat_start[1], 0) << "P1 had not conquered between the combats";
    EXPECT_EQ(s().battlefields[0].controller, std::optional<PlayerId>(P1));
    EXPECT_EQ(s().player(P1).score, 1);
    EXPECT_FALSE(s().chain.exists());
    EXPECT_FALSE(s().battlefields[0].combat_staged);
    EXPECT_FALSE(s().battlefields[0].is_contested);
}

TEST_F(SweepFlowTest, VictoryScore_LeonaReadsTheScoreInEffect) {
    // Leona, Zealot: "If an opponent's score is within 3 points of the Victory
    // Score, I enter ready." Aspirant's Climb raises the Victory Score to 9.
    for (int opp_score : {5, 6}) {
        makeEngine();
        setBattlefieldCard(0, kAspirantsClimb);
        s().player(P2).score = opp_score;
        engine_->testHook_recalculateAuras();
        ASSERT_EQ(s().effectiveVictoryScore(), s().mode.victory_score + 1);
        auto leona = inHand(P1, kLeonaZealot);
        s().player(P1).rune_pool.energy = 20;
        s().player(P1).rune_pool.universal_power = 20;
        engine_->testHook_executeIntent(playIntent(P1, leona, LocationId{BaseLocation{P1}}));
        ASSERT_TRUE(s().getObject(leona).location.has_value());
        EXPECT_EQ(s().getObject(leona).is_exhausted, opp_score == 5)
            << "opponent at " << opp_score << " of 9";
    }
}

TEST_F(SweepFlowTest, VictoryScore_ChemBaronessReadsTheScoreInEffect) {
    // "While your score is within 3 points of the Victory Score, your Gold
    //  [ADD] an additional [1]."
    for (int score : {5, 6}) {
        makeEngine();
        setBattlefieldCard(0, kAspirantsClimb);
        auto& legend = s().getObject(s().player(P1).legend_zone);
        legend.card_def_id = kChemBaroness;
        s().player(P1).score = score;
        engine_->testHook_cleanup();
        EXPECT_EQ(legend.card_counters["__gold_add_bonus_energy"], score == 6 ? 1 : 0);
    }
}

TEST_F(SweepFlowTest, VictoryScore_FindYourCenterAndCorruptedDragonReadTheScoreInEffect) {
    makeEngine();
    setBattlefieldCard(0, kAspirantsClimb);
    engine_->testHook_recalculateAuras();
    s().player(P2).score = 5;   // 4 short of 9
    EXPECT_EQ(card_registry.get(kFindYourCenter)->selfCostReduction(s(), P1), 0);
    s().player(P2).score = 6;
    EXPECT_EQ(card_registry.get(kFindYourCenter)->selfCostReduction(s(), P1), 2);
    // Corrupted Dragon: "If your score is not within 3 points of the Victory
    // Score, I enter ready."
    s().player(P1).score = 5;
    EXPECT_TRUE(card_registry.get(kCorruptedDragon)->entersReadyOnPlay(s(), P1));
    s().player(P1).score = 6;
    EXPECT_FALSE(card_registry.get(kCorruptedDragon)->entersReadyOnPlay(s(), P1));
}

TEST_F(SweepFlowTest, VictoryScore_PoppyReadsTheScoreInEffect) {
    // Poppy, Paragon: "When you play me, if an opponent's score is within 3
    // points of the Victory Score, ready me and gain 3 XP."
    for (int opp_score : {5, 6}) {
        makeEngine();
        setBattlefieldCard(0, kAspirantsClimb);
        s().player(P2).score = opp_score;
        engine_->testHook_recalculateAuras();
        auto poppy = inHand(P1, kPoppyParagon);
        s().player(P1).rune_pool.energy = 20;
        s().player(P1).rune_pool.universal_power = 20;
        engine_->testHook_executeIntent(playIntent(P1, poppy, LocationId{BaseLocation{P1}}));
        EXPECT_EQ(s().player(P1).xp, opp_score == 6 ? 3 : 0);
    }
}

TEST_F(SweepFlowTest, Yone_TriggersOnlyForABattlefieldThatWasUncontrolled) {
    // Yone, Blademaster: "When I conquer a battlefield that was uncontrolled,
    // deal damage equal to my Might to an enemy unit in a base."
    for (bool was_controlled : {false, true}) {
        makeEngine();
        a1_ = DrivingAgent{};
        if (was_controlled) control(0, P2);        // held by P2, nobody home
        auto yone = onBoard(P1, kYoneBlademaster);
        auto victim = onBoard(P2, kTBigVanilla);
        s().getObject(victim).base_might = s().getObject(victim).current_might = 20;
        a1_.want = victim;
        engine_->testHook_executeIntent(
            Intent::standardMove(P1, {yone}, LocationId{BattlefieldLocation{0}}));
        engine_->testHook_advanceMainPhase();
        ASSERT_EQ(s().battlefields[0].controller, std::optional<PlayerId>(P1));
        EXPECT_EQ(s().battlefields[0].conquered_from,
                  was_controlled ? std::optional<PlayerId>(P2) : std::nullopt);
        EXPECT_EQ(s().getObject(victim).damage_marked > 0, !was_controlled);
    }
}

// ═══ 9. Smaller rows ════════════════════════════════════════════════════════

TEST_F(SweepFlowTest, WujuMaster_LevelElevenUnitsEnterReady) {
    for (int xp : {10, 11}) {
        makeEngine();
        s().getObject(s().player(P1).legend_zone).card_def_id = kWujuMaster;
        s().player(P1).xp = xp;
        engine_->testHook_recalculateAuras();
        auto unit = inHand(P1, kTVanilla);
        engine_->testHook_executeIntent(playIntent(P1, unit, LocationId{BaseLocation{P1}}));
        EXPECT_EQ(s().getObject(unit).is_exhausted, xp < 11) << "xp " << xp;
    }
}

TEST_F(SweepFlowTest, ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities) {
    // "Use only if you've chosen enemy units and/or gear twice this turn with
    //  spells or unit abilities."
    makeEngine();
    auto legend = s().player(P1).legend_zone;
    s().getObject(legend).card_def_id = kProdigalExplorer;
    auto enemy = onBoard(P2, kTVanilla);
    auto enemy_gear = onBoard(P2, kTEquipGear);
    auto mine = onBoard(P1, kTVanilla);
    auto poke = [&](GameObjectId target) {
        Intent play = playIntent(P1, inHand(P1, kTPoke));
        play.targets = {target};
        engine_->testHook_executeIntent(play);
    };
    poke(mine);
    EXPECT_EQ(s().player(P1).enemy_choices_this_turn, 0) << "a friendly unit does not count";
    poke(enemy);
    EXPECT_EQ(s().player(P1).enemy_choices_this_turn, 1);
    EXPECT_TRUE(activationsOf(engine_->generateLegalActions(), legend).empty())
        << "once is not twice";
    poke(enemy_gear);
    EXPECT_EQ(s().player(P1).enemy_choices_this_turn, 2) << "enemy gear counts";
    EXPECT_EQ(activationsOf(engine_->generateLegalActions(), legend).size(), 1u);
}

TEST_F(SweepFlowTest, MightDependentAura_FioraSeesAnotherCardsAuraInTheSamePass) {
    // Fiora, Victorious: "While I'm [Mighty], I have [Deflect], [Ganking], and
    // [Shield]." Her hook runs before the Anthem's (lower object id); one aura
    // pass must still see the Anthem's +1.
    makeEngine();
    auto fiora = onBoard(P1, kFioraVictorious);
    s().getObject(fiora).base_might = s().getObject(fiora).current_might = 4;
    engine_->testHook_recalculateAuras();
    ASSERT_FALSE(s().getObject(fiora).hasKeyword(Keyword::Deflect));
    auto anthem = onBoard(P1, kTAnthem);
    engine_->testHook_recalculateAuras();          // exactly one pass
    EXPECT_EQ(s().getObject(fiora).current_might, 5);
    EXPECT_TRUE(s().getObject(fiora).hasKeyword(Keyword::Deflect));
    EXPECT_TRUE(s().getObject(fiora).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(s().getObject(fiora).hasKeyword(Keyword::Shield));
    // And she loses them in the pass the Anthem leaves.
    s().getObject(anthem).location = std::nullopt;
    s().getObject(anthem).zone = ZoneType::Trash;
    engine_->testHook_recalculateAuras();
    EXPECT_EQ(s().getObject(fiora).current_might, 4);
    EXPECT_FALSE(s().getObject(fiora).hasKeyword(Keyword::Deflect));
}

TEST_F(SweepFlowTest, MightDependentAura_FiorasOwnShieldDoesNotKeepHerMighty) {
    // CR 478.4 example: defending at 5 [M] she has [Shield] (+1 = 6); at 4 [M]
    // she drops to 4 with no keywords — the Shield never props up the test.
    makeEngine();
    auto fiora = onBoard(P1, kFioraVictorious, 0);
    auto& f = s().getObject(fiora);
    f.base_might = f.current_might = 5;
    f.combat_designation = CombatDesignation::Defender;
    engine_->testHook_recalculateAuras();
    EXPECT_EQ(f.current_might, 6);
    engine_->testHook_recalculateAuras();
    EXPECT_EQ(f.current_might, 6) << "stable across passes";
    f.base_might = 4;
    engine_->testHook_recalculateAuras();
    EXPECT_EQ(f.current_might, 4);
    EXPECT_FALSE(f.hasKeyword(Keyword::Shield));
}

TEST_F(SweepFlowTest, MightDependentAura_AlphaWildclawComparesThisPassesMight) {
    // "Your units here with less Might than me can't be chosen by enemy spells
    //  and abilities."
    makeEngine();
    auto alpha = onBoard(P1, kAlphaWildclaw, 0);
    auto pal = onBoard(P1, kTVanilla, 0);
    s().getObject(alpha).base_might = s().getObject(alpha).current_might = 2;
    engine_->testHook_recalculateAuras();
    EXPECT_FALSE(s().getObject(pal).untargetable_by_enemy) << "2 is not less than 2";
    s().getObject(alpha).base_might = 3;
    engine_->testHook_recalculateAuras();          // one pass
    EXPECT_TRUE(s().getObject(pal).untargetable_by_enemy);
}

TEST_F(SweepFlowTest, CrowdFavorite_MayBeActivatedWhileAlreadyBuffed) {
    // "Spend 2 XP: [Buff] me." No "use only if": CR 427.1.c — a unit with a
    // Buff can still be chosen for an action that Buffs, it just is not
    // Buffed again (427.1.b.1). The activation is legal and costs the XP.
    makeEngine();
    auto unit = onBoard(P1, kCrowdFavorite);
    s().getObject(unit).buff_count = 1;
    s().player(P1).xp = 2;
    const auto acts = activationsOf(engine_->generateLegalActions(), unit);
    ASSERT_EQ(acts.size(), 1u);
    engine_->testHook_executeIntent(acts.front());
    EXPECT_EQ(s().player(P1).xp, 0);
    EXPECT_EQ(s().getObject(unit).buff_count, 1);
}

TEST_F(SweepFlowTest, RecomputePendingLegalActions_FollowsAnEditMadeAtTheDecision) {
    // A god-mode edit while the engine waits: the options are rebuilt.
    makeEngine();
    EXPECT_FALSE(engine_->recomputePendingLegalActions().has_value())
        << "no decision is pending";
    GameObjectId added = kInvalidId;
    bool saw_main = false, saw_priority = false;
    a1_.respond = [&](const std::vector<Intent>& legal) -> const Intent* {
        if (!has(legal, IntentType::EndTurn)) return nullptr;
        added = inHand(P1, kTVanilla);                         // the edit
        auto fresh = engine_->recomputePendingLegalActions();
        saw_main = fresh.has_value() && !playsOf(*fresh, added).empty() &&
                   playsOf(legal, added).empty();
        return nullptr;
    };
    engine_->testHook_queryAgent(P1);
    EXPECT_TRUE(saw_main);
    EXPECT_FALSE(engine_->recomputePendingLegalActions().has_value());

    a2_.respond = [&](const std::vector<Intent>& legal) -> const Intent* {
        if (!has(legal, IntentType::PassPriority)) return nullptr;
        auto reaction = inHand(P2, kTRoleSpell);               // [Reaction], needs 2 units
        onBoard(P2, kTVanilla);
        onBoard(P2, kTVanilla);
        auto fresh = engine_->recomputePendingLegalActions();
        saw_priority = fresh.has_value() && has(*fresh, IntentType::PassPriority) &&
                       !playsOf(*fresh, reaction).empty();
        return nullptr;
    };
    engine_->testHook_executeIntent(playIntent(P1, inHand(P1, kTHiddenSpell)));
    EXPECT_TRUE(saw_priority);
}

void SweepFlowTest::registerMoreCards() {
    card_registry.registerCard(kTPoke, std::make_unique<TPoke>());
    card_registry.registerCard(kTHiddenTwoHere, std::make_unique<THiddenTwoHere>());
    card_registry.registerCard(kTAnthem, std::make_unique<TAnthem>());
    card_registry.registerCard(kTBigVanilla, std::make_unique<TBigVanilla>());
}

}  // namespace
}  // namespace test_review_fix_sweep_flow
}  // namespace riftbound::test
