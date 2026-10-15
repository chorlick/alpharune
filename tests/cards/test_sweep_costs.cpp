// Stage 3 sweep, part 1: costs, payment and play paths.
//
// Every test goes through the real engine (legal-action generation and
// executeIntent), because what changed is where the engine asks for and
// charges a cost: the Equip cost a gear declares, Weaponmaster paying it,
// additional costs chosen before the card's cost is paid, card-declared
// Repeat costs, restricted resource pools, the Hide cost and play-from-trash.

#include "card_test_fixture.h"

#include "agents/random_agent.h"
#include "cards/card_helpers.h"
#include "engine/resource_payment.h"
#include "rules/deck_validator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace riftbound::test::sweep_costs {

constexpr CardDefId kBrazenBuccaneer = 2;
constexpr CardDefId kPoutyPoro = 13;
constexpr CardDefId kWallop = 146;
constexpr CardDefId kKrakenHunter = 150;
constexpr CardDefId kCallToGlory = 207;
constexpr CardDefId kCruelPatron = 208;
constexpr CardDefId kLedros = 231;
constexpr CardDefId kSwiftScout = 259;
constexpr CardDefId kDaughterOfTheVoid = 294;
constexpr CardDefId kLuxCrownguard = 312;
constexpr CardDefId kArmedAssailant = 324;
constexpr CardDefId kLongSword = 345;         // [Equip] [R]
constexpr CardDefId kSkyfallOfAreion = 353;   // [Equip] [1][R]
constexpr CardDefId kLegionQuartermaster = 367;
constexpr CardDefId kAphelios = 372;
constexpr CardDefId kFrigidTouch = 389;       // [2], [Repeat] [2]
constexpr CardDefId kJaxUnrelenting = 440;
constexpr CardDefId kEzrealProdigy = 470;
constexpr CardDefId kZaunPunk = 481;
constexpr CardDefId kFireBelowTheMountain = 506;
constexpr CardDefId kEmperorOfTheSands = 512;
constexpr CardDefId kBardMercurial = 540;
constexpr CardDefId kUndyingLegion = 587;
constexpr CardDefId kShepherdsHeirloom = 720; // [Equip] — Spend 1 XP
constexpr CardDefId kUndyingLoyalty = 730;
constexpr CardDefId kAtakhan = 732;
constexpr CardDefId kPoppyDefender = 740;
constexpr CardDefId kCurtainCall = 743;
constexpr CardDefId kScornOfTheMoon = 786;
constexpr CardDefId kGangplankNaval = 873;    // [Empower] [O][O]
constexpr CardDefId kDeathMark = 931;         // [Flow] [1][A][A]
constexpr CardDefId kProgressDay = 114;       // [6][B]: Draw 4.
constexpr CardDefId kEndlessRiches = 809;
constexpr CardDefId kFaeDragon = 423;
constexpr CardDefId kIreliaGraceful = 462;

// ── Test cards (ids above the registered range) ──
constexpr CardDefId kTCostThenPrompt = 1852;       // spell: spend 1 XP, then choose a unit
constexpr CardDefId kTCostThenPromptFinal = 1853;  // the same, choices at finalization
constexpr CardDefId kTPlaysFromHand = 1854;        // spell: play a card from hand, ignoring its cost
constexpr CardDefId kTPowerTouch = 1855;           // spell costing one Fury Power: buff a unit
constexpr CardDefId kTCounter = 1856;              // [Reaction] spell: counter the item under it
constexpr CardDefId kTFalseDeclaration = 1857;     // [2] finalize-pass spell that chooses nothing
constexpr CardDefId kTUnnamedOption = 1858;        // spell whose prompt has an option with no value / object
constexpr CardDefId kTNoMarker = 1859;             // finalize-pass spell that returns before its marker
constexpr CardDefId kTDeclinedMay = 1850;          // finalize-pass spell: "you may gain 1 XP"
constexpr CardDefId kTEndlessChoices = 1851;       // finalize-pass spell with more sequences than the probe walks
constexpr CardDefId kTUnrepeatable = 1849;         // finalize-pass spell that asks differently each run
constexpr CardDefId kTNoMarkerDear = 1848;         // kTNoMarker costing [2], declaring a friendly-unit choice
constexpr CardDefId kTRealRunOnly = 1847;          // finalize-pass spell that asks differently in the run that is seen
constexpr CardDefId kRisenAltar = 950;
constexpr CardDefId kSkyCrusier = 847;
constexpr CardDefId kFlameChompers = 6;
constexpr CardDefId kImmortalPhoenix = 37;
constexpr CardDefId kSoulgorger = 196;
constexpr CardDefId kFirstMate = 132;
constexpr CardDefId kSafetyInspector = 726;
constexpr CardDefId kChallenge = 128;          // [2] + 1 Power; a friendly and an enemy unit
constexpr CardDefId kBondsOfStrength = 472;    // two friendly units
constexpr CardDefId kFoxFire = 253;            // [3]; chooses in its finalize pass
constexpr CardDefId kRocketBarrage = 400;      // [4][B]; chooses in its finalize pass
constexpr CardDefId kResonatingStrike = 821;   // [2] + 1 Power; chooses in its finalize pass
constexpr CardDefId kMelDefiantSoul = 897;

/// "Spend 1 XP. Buff a unit." The XP is a cost that is not idempotent, and a
/// prompt follows it, so the callback is re-entered after the payment.
class TCostThenPrompt : public SpellCard {
public:
    explicit TCostThenPrompt(CardDefId id, bool finalize) : id_(id), finalize_(finalize) {
        def_.id = id;
        def_.name = finalize ? "Test Cost Then Prompt (finalized)" : "Test Cost Then Prompt";
        def_.card_type = CardType::Spell;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return finalize_ ? kFinalizeResolve : 0u; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;   // the test cards take no discount
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (costDueNow(ctx)) {
            ctx.state.player(ctx.controller).xp -= 1;
            ctx.state.getObject(ctx.source).card_counters["paid"] += 1;
        }
        std::vector<GameObjectId> units;
        for (const auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.location.has_value()) units.push_back(id);
        const GameObjectId t = pickTarget(ctx, "Test: buff a unit", units);
        if (finalizingChoices(ctx)) return;
        if (t == kInvalidId) return;   // waiting, or nothing to choose
        ctx.executor.buffUnit(t);
    }
private:
    CardDefId id_;
    bool finalize_;
    CardDef def_;
};

/// A targeted spell whose whole cost is one Power: "[R] — Buff a unit." No
/// printed card fits the "[A] less" half of Irelia, Graceful's discount this
/// cleanly (a cheap spell with a Power cost and one unit target), so the test
/// uses this one.
class TPowerTouch : public SpellCard {
public:
    TPowerTouch() {
        def_.id = kTPowerTouch;
        def_.name = "Test Power Touch";
        def_.card_type = CardType::Spell;
        def_.domains = {Domain::Fury};
        def_.power_cost = 1;
    }
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId t = pickTarget(ctx, "Test Power Touch",
                                          enumerateLegalTargets(ctx.state, ctx.controller));
        if (t != kInvalidId) ctx.executor.buffUnit(t);
    }
private:
    CardDef def_;
};

/// A finalize-pass spell costing [2] whose declaration is wrong on purpose:
/// it says it could choose any friendly unit, and chooses nothing.
class TFalseDeclaration : public SpellCard {
public:
    TFalseDeclaration() {
        def_.id = kTFalseDeclaration;
        def_.name = "Test False Declaration";
        def_.card_type = CardType::Spell;
        def_.energy_cost = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (finalizingChoices(ctx)) return;
    }
private:
    CardDef def_;
};

/// A spell whose prompt offers "yes" by value and a decline that carries
/// neither a value nor an object.
class TUnnamedOption : public SpellCard {
public:
    TUnnamedOption() {
        def_.id = kTUnnamedOption;
        def_.name = "Test Unnamed Option";
        def_.card_type = CardType::Spell;
    }
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == 20) { ctx.executor.takeChoice(); ri.resume_point = 21; return; }
        if (ri.resume_point == 21) return;
        Intent yes, decline;
        yes.type = decline.type = IntentType::MakeChoice;
        yes.player = decline.player = ctx.controller;
        yes.chosen_value = 1;
        ctx.executor.requestChoice(ctx.controller, {decline, yes}, "Test Unnamed Option");
        ri.resume_point = 20;
    }
private:
    CardDef def_;
};

/// "You may gain 1 XP." Declined, it returns before its marker — the legitimate
/// early end: confirmOptional stored the answer.
class TDeclinedMay : public SpellCard {
public:
    TDeclinedMay() {
        def_.id = kTDeclinedMay;
        def_.name = "Test Declined May";
        def_.card_type = CardType::Spell;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId, GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const int go = confirmOptional(ctx, "Test Declined May: gain 1 XP?", [] { return true; });
        if (go < 1) return;   // waiting, or declined
        if (finalizingChoices(ctx)) return;
        ctx.state.player(ctx.controller).xp += 1;
    }
private:
    CardDef def_;
};

/// A finalize-pass spell costing [2] that says it could choose a friendly unit
/// and instead asks nine two-option questions: 512 sequences, none of which
/// earns a discount — more than the probe walks (GameEngine::kPrePlayProbeRuns).
class TEndlessChoices : public SpellCard {
public:
    TEndlessChoices() {
        def_.id = kTEndlessChoices;
        def_.name = "Test Endless Choices";
        def_.card_type = CardType::Spell;
        def_.energy_cost = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        constexpr int kQuestions = 9, kWaiting = 100, kAsked = 200;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point != kAsked) {
            int answered = 0;
            if (ri.resume_point >= kWaiting) {
                (void)takenValue(ctx, 0);
                answered = ri.resume_point - kWaiting + 1;
            }
            if (answered < kQuestions) {
                ctx.executor.requestChoice(ctx.controller,
                    {valChoice(ctx.controller, 0, "This"), valChoice(ctx.controller, 1, "That")},
                    "Test Endless Choices");
                ri.resume_point = kWaiting + answered;
                return;
            }
            ri.resume_point = kAsked;
        }
        if (finalizingChoices(ctx)) return;
    }
private:
    CardDef def_;
};

/// A finalize-pass spell costing [2] whose pass is not repeatable: the first
/// time it is run it offers the friendly units, every later time nothing. It
/// keeps that in the card object, outside the game state.
class TUnrepeatable : public SpellCard {
public:
    TUnrepeatable() {
        def_.id = kTUnrepeatable;
        def_.name = "Test Unrepeatable";
        def_.card_type = CardType::Spell;
        def_.energy_cost = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.state.chain.resuming->resume_point == 0) ++starts_;
        std::vector<GameObjectId> legal;
        if (starts_ <= 1)
            legal = chooseableUnits(ctx.state, ctx.controller,
                [&](const GameObject& u) { return u.controller == ctx.controller; });
        (void)pickTarget(ctx, "Test Unrepeatable", legal);
        if (finalizingChoices(ctx)) return;
    }
private:
    CardDef def_;
    mutable int starts_ = 0;
};

/// Breaks repeatability in the one run the others cannot stand in for: it
/// offers every friendly unit while the engine is only trying its choices out
/// (the bus is quiet then) and only the first of them in the run that counts.
/// What it looks at is outside the GameState and its chain item — exactly what
/// the contract forbids.
class TRealRunOnly : public SpellCard {
public:
    TRealRunOnly() {
        def_.id = kTRealRunOnly;
        def_.name = "Test Real Run Only";
        def_.card_type = CardType::Spell;
        def_.energy_cost = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> legal = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& u) { return u.controller == ctx.controller; });
        if (!ctx.events.holding() && legal.size() > 1) legal.resize(1);
        (void)pickTarget(ctx, "Test Real Run Only", legal);
        if (finalizingChoices(ctx)) return;
    }
private:
    CardDef def_;
};

/// Opted into choices at finalization, and returns without reaching the marker.
class TNoMarker : public SpellCard {
public:
    explicit TNoMarker(CardDefId id = kTNoMarker, int energy_cost = 0) {
        def_.id = id;
        def_.name = energy_cost > 0 ? "Test No Marker (dear)" : "Test No Marker";
        def_.card_type = CardType::Spell;
        def_.energy_cost = energy_cost;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return def_.energy_cost > 0 ? friendlyUnitChoice(state, controller, object)
                                    : FinalizeChoice::No;
    }
    void onResolve(CardContext&, const std::vector<GameObjectId>&) override {}
private:
    CardDef def_;
};

/// "[Reaction] Counter a spell or ability." Peek-and-pop: it resolves, then
/// removes the item that is now on top of the chain.
class TCounter : public SpellCard {
public:
    TCounter() {
        def_.id = kTCounter;
        def_.name = "Test Counter";
        def_.card_type = CardType::Spell;
        def_.keywords.set(Keyword::Reaction);
    }
    const CardDef& def() const override { return def_; }
    bool hasLegalTargets(const GameState& state, PlayerId) const override {
        return !state.chain.items.empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.items.empty()) ctx.state.chain.items.pop_back();
    }
private:
    CardDef def_;
};

/// "Play a spell from your hand, ignoring its cost."
class TPlaysFromHand : public SpellCard {
public:
    TPlaysFromHand() {
        def_.id = kTPlaysFromHand;
        def_.name = "Test Plays From Hand";
        def_.card_type = CardType::Spell;
    }
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto hand = ctx.state.player(ctx.controller).hand;
        for (auto id : hand)
            if (ctx.state.getObject(id).isSpell()) {
                ctx.executor.playIgnoringCost(ctx.controller, id);
                return;
            }
    }
private:
    CardDef def_;
};

class SweepCostsTest : public CardTestFixture {
protected:
    void SetUp() override {
        CardTestFixture::SetUp();
        card_registry.registerCard(kTCostThenPrompt,
            std::make_unique<TCostThenPrompt>(kTCostThenPrompt, false));
        card_registry.registerCard(kTCostThenPromptFinal,
            std::make_unique<TCostThenPrompt>(kTCostThenPromptFinal, true));
        card_registry.registerCard(kTPlaysFromHand, std::make_unique<TPlaysFromHand>());
        card_registry.registerCard(kTPowerTouch, std::make_unique<TPowerTouch>());
        card_registry.registerCard(kTCounter, std::make_unique<TCounter>());
        card_registry.registerCard(kTFalseDeclaration, std::make_unique<TFalseDeclaration>());
        card_registry.registerCard(kTUnnamedOption, std::make_unique<TUnnamedOption>());
        card_registry.registerCard(kTNoMarker, std::make_unique<TNoMarker>());
        card_registry.registerCard(kTDeclinedMay, std::make_unique<TDeclinedMay>());
        card_registry.registerCard(kTEndlessChoices, std::make_unique<TEndlessChoices>());
        card_registry.registerCard(kTNoMarkerDear, std::make_unique<TNoMarker>(kTNoMarkerDear, 2));
        card_registry.registerCard(kTUnrepeatable, std::make_unique<TUnrepeatable>());
        card_registry.registerCard(kTRealRunOnly, std::make_unique<TRealRunOnly>());
        card_db.buildFromClasses(card_registry);
    }

    GameObject& o(GameObjectId id) { return state.getObject(id); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
    int handSize(PlayerId p) { return static_cast<int>(state.player(p).hand.size()); }

    GameObjectId addGear(PlayerId owner, CardDefId def_id = kInvalidId, bool equipment = false) {
        auto id = state.createObject();
        auto& g = state.getObject(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        g.name = "TestGear";
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            g.name = def.name;
            g.tags = def.tags;
            g.keywords = def.keywords;
            g.might_bonus = def.might_bonus;
        }
        if (equipment) g.tags.push_back("Equipment");
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = state.getObject(id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        l.name = def.name;
        l.card_type = def.card_type;
        l.domains = def.domains;
        l.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    /// The player's whole floating pool: `energy` Energy and `power` [A].
    void setPool(PlayerId p, int energy, int power) {
        state.player(p).rune_pool = RunePool{};
        state.player(p).rune_pool.energy = energy;
        state.player(p).rune_pool.universal_power = power;
    }
    /// What the engine offers `p` now, after a cleanup (continuous effects
    /// and standing play-from-trash permissions are rebuilt there).
    std::vector<Intent> legal(PlayerId p = P1, bool settle = true) {
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            if (settle) e.testHook_cleanup();
            out = e.generateLegalActions();
        }, nullptr, nullptr, p);
        return out;
    }
    bool playOffered(GameObjectId card) {
        for (const auto& i : legal(o(card).controller))
            if (i.card == card && (i.type == IntentType::PlayCard ||
                                   i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayReaction)) return true;
        return false;
    }
    /// Play `card` paying from exactly what the test laid out (no funding).
    void play(GameObjectId card, AgentInterface* agent = nullptr,
              std::vector<GameObjectId> targets = {}) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = o(card).controller;
        i.card = card;
        i.targets = std::move(targets);
        if (!o(card).isSpell()) i.play_location = LocationId{BaseLocation{i.player}};
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); }, agent, nullptr, i.player);
    }
    void execute(const Intent& i, AgentInterface* agent = nullptr) {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); }, agent, nullptr, i.player);
    }
    std::vector<Intent> equipOffers(GameObjectId gear, bool settle = true) {
        std::vector<Intent> out;
        for (const auto& i : legal(P1, settle))
            if (i.type == IntentType::ActivateAbility && i.ability_source == gear) out.push_back(i);
        return out;
    }
    /// Put `card` (in its owner's hand) facedown at battlefield `bf`, hidden
    /// since last turn.
    void hideAt(GameObjectId card, int bf) {
        auto& hand = state.player(o(card).owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        o(card).zone = ZoneType::FacedownZone;
        o(card).is_hidden = true;
        o(card).hidden_at = BattlefieldId::fromRaw(bf);
        o(card).hidden_on_turn = state.turn.turn_number - 1;
        state.battlefields[bf].facedown.push_back(card);
    }
    /// A hidden card is played with [Reaction] timing: an enemy item on the
    /// chain (it does nothing), priority with `p`.
    void reactionWindowFor(PlayerId p) {
        pushSpellOnChain(opponent(p));
        state.chain.items.back().status = ChainItemStatus::Finalized;
        state.turn.oc_state = OpenClosedState::Closed;
        state.turn.priority_holder = p;
    }
    bool canAfford(GameObjectId card) {
        bool ok = false;
        withEngine([&](GameEngine& e) { ok = e.testHook_canAfford(o(card).controller, card); },
                   nullptr, nullptr, o(card).controller);
        return ok;
    }
};

// ─── Additional costs chosen before payment (CR 356.2) ──────────────────────

TEST_F(SweepCostsTest, CruelPatron_KillIsPaidBeforeItEntersAndGatesThePlay) {
    auto patron = addToHand(P1, kCruelPatron);
    setPool(P1, 4, 0);
    EXPECT_FALSE(playOffered(patron)) << "no friendly unit: the mandatory cost can't be paid";

    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    EXPECT_TRUE(playOffered(patron));
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    play(patron, &agent);
    EXPECT_TRUE(agent.wasOffered(a)) << "which unit dies is the player's choice";
    EXPECT_FALSE(agent.wasOffered(patron)) << "paid before I am on the board";
    EXPECT_FALSE(onBoard(b));
    EXPECT_TRUE(onBoard(a));
    EXPECT_TRUE(onBoard(patron));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_FALSE(state.chain.exists()) << "the kill is a cost, not a trigger";
}

TEST_F(SweepCostsTest, LegionQuartermaster_ReturnsAGearAsItIsPlayed) {
    auto qm = addToHand(P1, kLegionQuartermaster);
    setPool(P1, 3, 0);
    EXPECT_FALSE(playOffered(qm));
    auto gear_a = addGear(P1);
    auto gear_b = addGear(P1);
    EXPECT_TRUE(playOffered(qm));
    EnginePathAgent agent(EnginePathAgent::choosing(gear_b));
    play(qm, &agent);
    EXPECT_TRUE(onBoard(qm));
    EXPECT_TRUE(inHand(P1, gear_b));
    EXPECT_TRUE(onBoard(gear_a));
}

TEST_F(SweepCostsTest, ZaunPunk_OptionalKillIsPaidDuringThePlay) {
    // Declined: nothing dies and the play trigger does not happen.
    auto mine = addGear(P1);
    auto theirs = addGear(P2);
    auto punk = addToHand(P1, kZaunPunk);
    setPool(P1, 3, 0);
    EnginePathAgent no(EnginePathAgent::script({0}));
    play(punk, &no);
    EXPECT_TRUE(onBoard(punk));
    EXPECT_EQ(no.prompts.size(), 1u) << "only the additional cost is asked";
    EXPECT_TRUE(onBoard(mine));
    EXPECT_TRUE(onBoard(theirs));

    // Paid: my gear dies as the cost, then "kill a gear" picks the enemy one.
    auto punk2 = addToHand(P1, kZaunPunk);
    setPool(P1, 3, 0);
    EnginePathAgent yes(EnginePathAgent::script({1, static_cast<int>(mine.raw()),
                                                 static_cast<int>(theirs.raw())}));
    play(punk2, &yes);
    EXPECT_TRUE(onBoard(punk2));
    EXPECT_FALSE(onBoard(mine)) << "the additional cost";
    EXPECT_FALSE(onBoard(theirs)) << "if you paid the additional cost, kill a gear";
}

TEST_F(SweepCostsTest, Bard_ExhaustsTheLegendAsItIsPlayed) {
    auto legend = addLegend(P1, kSwiftScout);
    auto bard = addToHand(P1, kBardMercurial);
    setPool(P1, 4, 1);
    EnginePathAgent no(EnginePathAgent::script({0}));
    play(bard, &no);
    EXPECT_TRUE(onBoard(bard));
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_EQ(no.prompts.size(), 1u) << "declined: no play trigger";

    auto bard2 = addToHand(P1, kBardMercurial);
    setPool(P1, 4, 1);
    // Pay (1); then the play trigger: the open battlefield to move to (#0),
    // and "None" (0) at the first "move a unit there" prompt.
    EnginePathAgent yes(EnginePathAgent::script({1, 0, 0}));
    play(bard2, &yes);
    EXPECT_TRUE(onBoard(bard2));
    EXPECT_TRUE(o(legend).is_exhausted) << "paid during the play";
    EXPECT_EQ(yes.prompts.size(), 3u);
    EXPECT_TRUE(o(bard).isAtBase() && o(bard2).isAtBase()) << "no unit was moved";

    // An exhausted legend cannot pay it: nothing is asked.
    auto bard3 = addToHand(P1, kBardMercurial);
    setPool(P1, 4, 1);
    EnginePathAgent none;
    play(bard3, &none);
    EXPECT_TRUE(onBoard(bard3));
    EXPECT_TRUE(none.prompts.empty());
}

TEST_F(SweepCostsTest, Atakhan_KillsAUnitAndCostsItsPrintedCostLess) {
    auto atakhan = addToHand(P1, kAtakhan);          // [10] + 3 Power
    setPool(P1, 6, 3);
    EXPECT_FALSE(playOffered(atakhan));
    auto token = addUnit(P1, kInvalidId, 1);         // costs nothing
    EXPECT_FALSE(playOffered(atakhan)) << "a unit with no printed cost takes nothing off";
    auto patron = addUnit(P1, kCruelPatron);         // printed [4]
    EXPECT_TRUE(playOffered(atakhan)) << "[10] - [4] = [6]";

    EnginePathAgent agent;   // first option of every prompt
    play(atakhan, &agent);
    EXPECT_TRUE(onBoard(atakhan));
    EXPECT_FALSE(onBoard(patron));
    EXPECT_TRUE(onBoard(token));
    EXPECT_FALSE(agent.wasOffered(token)) << "killing it would leave me unpayable";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(SweepCostsTest, Atakhan_TheKillCanBeDeclinedWhenTheFullCostIsPayable) {
    auto atakhan = addToHand(P1, kAtakhan);
    auto patron = addUnit(P1, kCruelPatron);
    setPool(P1, 10, 3);
    EnginePathAgent no(EnginePathAgent::script({0}));
    play(atakhan, &no);
    EXPECT_TRUE(onBoard(atakhan));
    EXPECT_TRUE(onBoard(patron));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(SweepCostsTest, Ledros_KillsUnitsOfThePlayersChoiceForPowerOff) {
    auto ledros = addToHand(P1, kLedros);            // [6] + 4 Power
    auto a = addUnit(P1, kInvalidId, 1);
    auto b = addUnit(P1, kInvalidId, 1);
    auto c = addUnit(P1, kInvalidId, 1);
    setPool(P1, 6, 2);                               // two Power short
    ASSERT_TRUE(playOffered(ledros));
    // Two kills are forced; then a third is offered and declined.
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(c.raw()), static_cast<int>(a.raw()), 0}));
    play(ledros, &agent);
    EXPECT_TRUE(onBoard(ledros));
    EXPECT_FALSE(onBoard(c));
    EXPECT_FALSE(onBoard(a));
    EXPECT_TRUE(onBoard(b)) << "the player stopped once the rest could be paid";
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(SweepCostsTest, KrakenHunter_SpendsBuffsBeforePaying) {
    auto hunter = addToHand(P1, kKrakenHunter);      // [3] + 2 Power
    auto buffed = addUnit(P1, kInvalidId, 2);
    o(buffed).buff_count = 3;
    setPool(P1, 3, 0);
    ASSERT_TRUE(playOffered(hunter)) << "two buffs cover the Power";
    EnginePathAgent agent;
    play(hunter, &agent);
    EXPECT_TRUE(onBoard(hunter));
    EXPECT_EQ(o(buffed).buff_count, 1) << "one buff per [O], never more than the Power cost";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(SweepCostsTest, Poppy_SpendsXpForADiscountChosenBeforePayment) {
    auto poppy = addToHand(P1, kPoppyDefender);      // [6] + 1 Power
    setPool(P1, 3, 1);
    setXp(P1, 2);
    EXPECT_FALSE(playOffered(poppy));
    setXp(P1, 3);
    ASSERT_TRUE(playOffered(poppy));
    EnginePathAgent agent;
    play(poppy, &agent);
    EXPECT_TRUE(onBoard(poppy));
    EXPECT_EQ(state.player(P1).xp, 0);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to decline: only the discount pays for me";

    auto poppy2 = addToHand(P1, kPoppyDefender);
    setPool(P1, 6, 1);
    setXp(P1, 3);
    EnginePathAgent no(EnginePathAgent::script({0}));
    play(poppy2, &no);
    EXPECT_TRUE(onBoard(poppy2));
    EXPECT_EQ(state.player(P1).xp, 3) << "declined";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(SweepCostsTest, BrazenBuccaneer_DiscardCountsTowardAffordability) {
    auto buccaneer = addToHand(P1, kBrazenBuccaneer);   // [6], discard 1: [2] less
    setPool(P1, 4, 0);
    EXPECT_FALSE(playOffered(buccaneer)) << "nothing else in hand to discard";
    auto fodder = addToHand(P1, kPoutyPoro);
    ASSERT_TRUE(playOffered(buccaneer));
    EnginePathAgent agent;
    play(buccaneer, &agent);
    EXPECT_TRUE(onBoard(buccaneer));
    EXPECT_TRUE(inTrash(P1, fodder)) << "the discard is forced: the play was offered on it";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(SweepCostsTest, Wallop_SpendingABuffIgnoresTheCost) {
    auto wallop = addToHand(P1, kWallop);            // [2]
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).is_exhausted = true;
    setPool(P1, 0, 0);
    EXPECT_FALSE(playOffered(wallop));
    o(unit).buff_count = 1;
    ASSERT_TRUE(playOffered(wallop)) << "a buff to spend makes it playable with no Energy";
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(wallop, &agent, {unit});
    EXPECT_EQ(o(unit).buff_count, 0);
    EXPECT_FALSE(o(unit).is_exhausted) << "Ready a unit";
    EXPECT_TRUE(inTrash(P1, wallop));

    // With the Energy in hand the buff may be kept.
    auto wallop2 = addToHand(P1, kWallop);
    o(unit).buff_count = 1;
    o(unit).is_exhausted = true;
    setPool(P1, 2, 0);
    EnginePathAgent no(EnginePathAgent::script({0, static_cast<int>(unit.raw())}));
    play(wallop2, &no, {unit});
    EXPECT_EQ(o(unit).buff_count, 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_FALSE(o(unit).is_exhausted);
}

TEST_F(SweepCostsTest, CallToGlory_SpendingABuffIgnoresTheCost) {
    auto spell = addToHand(P1, kCallToGlory);        // [3]
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).buff_count = 1;
    o(unit).recomputeMight();
    setPool(P1, 0, 0);
    ASSERT_TRUE(playOffered(spell));
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(spell, &agent, {unit});
    EXPECT_EQ(o(unit).buff_count, 0);
    EXPECT_EQ(o(unit).temp_might_bonus, 3);
}

TEST_F(SweepCostsTest, UndyingLoyalty_TheDiscountFollowsTheUnitChosenAsItIsPlayed) {
    auto spell = addToHand(P1, kUndyingLoyalty);     // [2] + 1 Power
    setPool(P1, 0, 1);
    addToTrash(P1, kCruelPatron);                    // costs [4]: not a legal choice
    EXPECT_FALSE(playOffered(spell)) << "no unit of cost [2] or less in the trash";
    auto poro = addToTrash(P1, kPoutyPoro);          // a Poro: [2] less
    ASSERT_TRUE(playOffered(spell)) << "[2] less with a Poro leaves only the Power";
    // Where it is played (the spell's finalize-pass choice), then the unit.
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(poro.raw())}));
    play(spell, &agent);
    EXPECT_TRUE(onBoard(poro));
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

// ─── Repeat (CR 820): the cost is the card's declaration ────────────────────

TEST_F(SweepCostsTest, CurtainCall_EachRepeatCostIsOfferedOnceAndRunsAnotherMode) {
    auto spell = addToHand(P1, kCurtainCall);        // [4]; [Repeat] — [1] / [A] / [1][A]
    auto at_bf = addUnit(P2, kInvalidId, 9, 0);
    setPool(P1, 5, 0);                               // the base cost and the [1] Repeat
    addToDeck(P1, kPoutyPoro);
    const int hand_before = handSize(P1);
    // Mode 0 (Draw 1) for the first execution; pay the [1] Repeat; then the
    // repeated execution's mode 1 (Deal 2) on the unit.
    EnginePathAgent agent(EnginePathAgent::script(
        {0, 1, 1, static_cast<int>(at_bf.raw())}));
    play(spell, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(handSize(P1), hand_before - 1 + 1) << "Draw 1 ran once";
    EXPECT_EQ(o(at_bf).damage_marked, 2) << "the repeat chose a mode not yet chosen";
}

TEST_F(SweepCostsTest, Ezreal_OptionalAdditionalCostsCostOneLess) {
    // Frigid Touch: [2], [Repeat] [2]. With [3] the Repeat is out of reach...
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto spell = addToHand(P1, kFrigidTouch);
    setPool(P1, 3, 0);
    EnginePathAgent plain(EnginePathAgent::script({static_cast<int>(enemy.raw())}));
    play(spell, &plain, {enemy});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "the Repeat could not be paid";

    // ...unless Ezreal, Prodigy is on the board: it costs [1].
    addUnit(P1, kEzrealProdigy, 3);
    auto again = addToHand(P1, kFrigidTouch);
    setPool(P1, 3, 0);
    // Explicit: pay the [Repeat] (the unlabelled 0/1 prompt: 1), and choose
    // the enemy unit whenever a target is asked for.
    EnginePathAgent with([enemy](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == enemy) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        ADD_FAILURE() << "unexpected prompt";
        return legal.front();
    });
    play(again, &with, {enemy});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "[2] + a [Repeat] of [2] - [1]";
}

TEST_F(SweepCostsTest, GrantedRepeatEqualToItsCost_ChargesThePowerToo) {
    // Temporal Portal / The Academy: "[Repeat] equal to its cost". Progress
    // Day costs [6][B]; the grant used to carry only the Energy.
    auto spell = addToHand(P1, kProgressDay);
    for (int i = 0; i < 8; ++i) addToDeck(P1, kPoutyPoro);
    state.player(P1).grant_repeat_base_to_next_spell = true;
    setPool(P1, 12, 1);                              // one Power short of the Repeat
    EnginePathAgent agent(EnginePathAgent::script({1}));
    play(spell, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 6) << "the Repeat was not payable";
    EXPECT_EQ(handSize(P1), 4);

    auto again = addToHand(P1, kProgressDay);
    for (int i = 0; i < 8; ++i) addToDeck(P1, kPoutyPoro);
    state.player(P1).grant_repeat_base_to_next_spell = true;
    setPool(P1, 12, 2);
    const int before = handSize(P1);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    play(again, &yes);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(handSize(P1), before - 1 + 8) << "Draw 4, twice";
}

// ─── Equip: the cost a gear declares (CR 818) ───────────────────────────────

TEST_F(SweepCostsTest, Equip_IsOfferedOnlyWhenItsCostCanBePaid) {
    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);            // [Equip] [R]
    setPool(P1, 0, 0);
    EXPECT_TRUE(equipOffers(sword).empty()) << "no Power: the Equip was offered and did nothing";
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
    EXPECT_TRUE(equipOffers(sword).empty()) << "[R] is Fury";
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto offers = equipOffers(sword);
    ASSERT_EQ(offers.size(), 1u);
    execute(offers.front());
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);
    EXPECT_EQ(state.player(P1).gear_abilities_activated_this_turn, 1);
}

// NOT A DEFECT (carry-over, cards-c0): an attached Equipment cannot activate
// its own Equip — its Rules Text is Inactive while attached (CR 718.2, 721.2).
// Only Weaponmaster moves it ("even if it's already attached", CR 821.1.c).
TEST_F(SweepCostsTest, Equip_AttachedEquipmentIsNotOfferedAgain) {
    auto holder = addUnit(P1, kInvalidId, 2);
    addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    auto offers = equipOffers(sword);
    ASSERT_EQ(offers.size(), 2u);
    Intent to_holder;
    for (const auto& i : offers)
        if (!i.targets.empty() && i.targets.front() == holder) to_holder = i;
    execute(to_holder);
    ASSERT_EQ(o(sword).attached_to, std::optional<GameObjectId>{holder});
    EXPECT_TRUE(equipOffers(sword).empty());
}

TEST_F(SweepCostsTest, Equip_FirstGearAbilityDiscountAppliesToTheEquipCost) {
    // Piltovan Forge: "the first friendly gear activated ability played each
    // turn costs [1] less" — an Equip is one. Skyfall of Areion: [Equip] [1][R].
    addUnit(P1, kInvalidId, 2);
    auto skyfall = addGear(P1, kSkyfallOfAreion);
    setPool(P1, 0, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EXPECT_TRUE(equipOffers(skyfall).empty()) << "the [1] is missing";
    // The discount is a continuous effect (rebuilt by the Forge on every aura
    // pass); it is set by hand here, so the offers are read without a cleanup.
    state.player(P1).first_gear_ability_discount = 1;
    ASSERT_EQ(equipOffers(skyfall, /*settle=*/false).size(), 1u);
    state.player(P1).gear_abilities_activated_this_turn = 1;
    EXPECT_TRUE(equipOffers(skyfall, /*settle=*/false).empty()) << "only the first each turn";
}

TEST_F(SweepCostsTest, Equip_ATriggerItCausesResolvesWithTheEquip) {
    // Jax, Unrelenting: "When you attach an Equipment to me, you may pay [1]
    // to draw 1." The trigger used to stay on the chain until the next action.
    auto jax = addUnit(P1, kJaxUnrelenting, 3);
    auto sword = addGear(P1, kLongSword);
    addToDeck(P1, kPoutyPoro);
    setPool(P1, 1, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto offers = equipOffers(sword);
    ASSERT_EQ(offers.size(), 1u);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    execute(offers.front(), &agent);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{jax});
    EXPECT_FALSE(state.chain.exists());
    EXPECT_EQ(handSize(P1), 1) << "paid [1] to draw 1";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

// ─── Weaponmaster pays the Equip cost, [A] less (CR 821) ────────────────────

TEST_F(SweepCostsTest, Weaponmaster_ChargesTheEquipCostLessOnePower) {
    auto skyfall = addGear(P1, kSkyfallOfAreion);    // [Equip] [1][R] -> [1]
    auto first = addToHand(P1, kArmedAssailant);
    EnginePathAgent a1(EnginePathAgent::choosing(skyfall, EnginePathAgent::May::Yes));
    playThroughEngine(first, &a1);                   // funded for the unit only
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(a1.prompts.empty()) << "the [1] can't be paid: not offered (CR 821.1.c.5)";
    EXPECT_FALSE(o(skyfall).attached_to.has_value());

    auto second = addToHand(P1, kArmedAssailant);
    state.player(P1).rune_pool.energy += 1;
    EnginePathAgent a2(EnginePathAgent::choosing(skyfall, EnginePathAgent::May::Yes));
    playThroughEngine(second, &a2);
    EXPECT_EQ(o(skyfall).attached_to, std::optional<GameObjectId>{second});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "the [1] was charged";
}

TEST_F(SweepCostsTest, Weaponmaster_MovesAnAttachedEquipmentAndPaysANonResourceCost) {
    auto holder = addUnit(P1, kInvalidId, 2);
    auto heirloom = addGear(P1, kShepherdsHeirloom); // [Equip] — Spend 1 XP
    o(heirloom).attached_to = holder;
    o(holder).attachments.push_back(heirloom);
    setXp(P1, 1);
    auto wm = addToHand(P1, kArmedAssailant);
    EnginePathAgent agent(EnginePathAgent::choosing(heirloom, EnginePathAgent::May::Yes));
    playThroughEngine(wm, &agent);
    EXPECT_EQ(o(heirloom).attached_to, std::optional<GameObjectId>{wm})
        << "even if it's already attached";
    EXPECT_TRUE(o(holder).attachments.empty());
    EXPECT_EQ(state.player(P1).xp, 0) << "no Power in the cost: nothing is reduced";
}

TEST_F(SweepCostsTest, SandSoldier_HasWeaponmasterFromTheEmperor) {
    // "Your Sand Soldiers have [Weaponmaster]." The token has no card class;
    // the ability is carried by the legend that grants the keyword.
    auto legend = addLegend(P1, kEmperorOfTheSands);
    auto sword = addGear(P1, kLongSword);            // [Equip] [R] -> free
    state.player(P1).equipment_played_this_turn = 1;
    setPool(P1, 1, 0);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = legend;
    EnginePathAgent agent(EnginePathAgent::choosing(sword, EnginePathAgent::May::Yes));
    execute(act, &agent);
    GameObjectId soldier = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Sand Soldier") soldier = id;
    ASSERT_NE(soldier, kInvalidId);
    ASSERT_FALSE(agent.prompts.empty()) << "the may, then the Equipment";
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{soldier});

    // Declined: a second soldier stays bare.
    auto shield = addGear(P1, kLongSword);
    o(legend).is_exhausted = false;
    setPool(P1, 1, 0);
    EnginePathAgent no(EnginePathAgent::script({0}));
    execute(act, &no);
    EXPECT_FALSE(o(shield).attached_to.has_value());
}

// ─── Restricted resources ───────────────────────────────────────────────────

TEST_F(SweepCostsTest, RestrictedPower_SpellOnlyPaysForSpellsOnly) {
    auto legend = addLegend(P1, kDaughterOfTheVoid);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, legend};
    card_registry.get(kDaughterOfTheVoid)->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).rune_pool.spell_power, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);

    state.player(P1).rune_pool.energy = 6;
    auto spell = addToHand(P1, kProgressDay);        // [6][B]
    auto unit = addToHand(P1, kPoppyDefender);       // [6] + 1 Power
    EXPECT_TRUE(canAfford(spell));
    EXPECT_FALSE(canAfford(unit)) << "Use only to play spells";
    for (int i = 0; i < 4; ++i) addToDeck(P1, kPoutyPoro);
    play(spell);
    EXPECT_EQ(state.player(P1).rune_pool.spell_power, 0);
}

TEST_F(SweepCostsTest, RestrictedEnergy_SpellOnlyPaysForSpellsOnly) {
    auto lux = addUnit(P1, kLuxCrownguard, 2);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, lux};
    card_registry.get(kLuxCrownguard)->onActivate(ctx, {});
    EXPECT_EQ(state.player(P1).rune_pool.spell_energy, 2);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    auto spell = addToHand(P1, kWallop);             // [2]
    auto unit = addToHand(P1, kPoutyPoro);           // [2]
    EXPECT_TRUE(canAfford(spell));
    EXPECT_FALSE(canAfford(unit));
}

TEST_F(SweepCostsTest, RestrictedPower_GearOnlyPaysForGearAndGearAbilities) {
    auto legend = addLegend(P1, kFireBelowTheMountain);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, legend};
    card_registry.get(kFireBelowTheMountain)->onActivate(ctx, {});
    ASSERT_EQ(state.player(P1).rune_pool.gear_power, 1);

    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);            // [Equip] [R]
    auto offers = equipOffers(sword);
    ASSERT_EQ(offers.size(), 1u) << "[A] for a gear ability";
    state.player(P1).rune_pool.energy = 6;
    EXPECT_FALSE(canAfford(addToHand(P1, kPoppyDefender))) << "not for a unit";
    state.player(P1).rune_pool.energy = 2;
    EXPECT_TRUE(canAfford(addToHand(P1, kLongSword))) << "playing a gear: [2] + 1 Power";
    execute(offers.front());
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_EQ(state.player(P1).rune_pool.gear_power, 0);
}

TEST_F(SweepCostsTest, RestrictedEnergy_ShowdownOnlyIsUnusableOutsideAShowdown) {
    auto legend = addLegend(P1, kScornOfTheMoon);
    EffectExecutor exec(state, events, card_db, &card_registry);
    CardContext ctx{state, events, exec, P1, legend};
    card_registry.get(kScornOfTheMoon)->onActivate(ctx, {});
    ASSERT_EQ(state.player(P1).rune_pool.showdown_energy, 1);
    state.player(P1).rune_pool.energy = 1;
    auto poro = addToHand(P1, kPoutyPoro);           // [2]
    EXPECT_FALSE(canAfford(poro)) << "no showdown: the Energy can't be spent";
    state.battlefields[0].showdown_in_progress = true;
    EXPECT_TRUE(canAfford(poro));
}

TEST_F(SweepCostsTest, RestrictedResources_EmptyWithThePool) {
    auto& pool = state.player(P1).rune_pool;
    pool.spell_energy = 2;
    pool.showdown_energy = 1;
    pool.spell_power = 1;
    pool.gear_power = 1;
    pool.clear();
    EXPECT_EQ(pool.spell_energy + pool.showdown_energy + pool.spell_power + pool.gear_power, 0);
}

// ─── Hide cost (CR 811.1.b) ─────────────────────────────────────────────────

TEST_F(SweepCostsTest, SwiftScout_HidesForOneEnergyInsteadOfThePower) {
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    CardDefId hidden_def = kInvalidId;   // the lowest-numbered [Hidden] card
    for (const auto& [id, def] : card_db.all())
        if (def.keywords.has(Keyword::Hidden) && (hidden_def == kInvalidId || id < hidden_def))
            hidden_def = id;
    ASSERT_NE(hidden_def, kInvalidId);
    const GameObjectId hidden_card = addToHand(P1, hidden_def);
    auto hideOffered = [&] {
        for (const auto& i : legal())
            if (i.type == IntentType::HideCard && i.card == hidden_card) return true;
        return false;
    };
    setPool(P1, 1, 0);
    EXPECT_FALSE(hideOffered()) << "[A] and no Power";
    addLegend(P1, kSwiftScout);
    ASSERT_TRUE(hideOffered()) << "You may pay [1] to hide a card instead of [A]";
    Intent hide;
    hide.type = IntentType::HideCard;
    hide.player = P1;
    hide.card = hidden_card;
    hide.chosen_battlefield = BattlefieldId::fromRaw(0);
    execute(hide);
    EXPECT_TRUE(o(hidden_card).is_hidden);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

// ─── Play from trash ────────────────────────────────────────────────────────

TEST_F(SweepCostsTest, UndyingLegion_IsPlayedFromTheTrashForItsLegionCost) {
    auto legion = addToTrash(P1, kUndyingLegion);    // "[Legion][>] ... from your trash for [3][R]"
    setPool(P1, 3, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto trashPlay = [&]() -> std::optional<Intent> {
        for (const auto& i : legal())
            if (i.card == legion && i.play_source == Intent::PlaySource::Trash) return i;
        return std::nullopt;
    };
    EXPECT_FALSE(trashPlay().has_value()) << "[Legion]: no other card played this turn";
    state.player(P1).cards_played_this_turn = 1;
    auto offer = trashPlay();
    ASSERT_TRUE(offer.has_value());
    execute(*offer);
    EXPECT_TRUE(onBoard(legion));
    EXPECT_FALSE(inTrash(P1, legion));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);
}

TEST_F(SweepCostsTest, DeathMark_FlowPaysTwoAnyPowerFromTwoDomains) {
    auto mark = addToTrash(P1, kDeathMark);          // [Flow] [1][A][A]
    for (int i = 0; i < 3; ++i) addToDeck(P1, kPoutyPoro);
    setPool(P1, 1, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
    std::optional<Intent> offer;
    for (const auto& i : legal())
        if (i.card == mark && i.play_source == Intent::PlaySource::Trash) offer = i;
    ASSERT_TRUE(offer.has_value()) << "[A][A] need not share a Domain";
    execute(*offer);
    EXPECT_EQ(o(mark).zone, ZoneType::Banishment) << "Flow: then banish it";
    EXPECT_EQ(state.player(P1).rune_pool.totalPower(), 0);
}

// ─── Activation cost reductions on the Power side ───────────────────────────

TEST_F(SweepCostsTest, EmpowerDiscount_TakesAPowerWhereThereIsNoEnergy) {
    // Risen Altar: "[Empower] costs of your units here cost [1] or [A] less."
    // Gangplank, Naval: [Empower] [O][O] — no Energy to take the [1] from.
    auto gp = addUnit(P1, kGangplankNaval, 4, 0);
    setPool(P1, 0, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 1;
    const Card* card = card_registry.get(kGangplankNaval);
    const int idx = card->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    EXPECT_EQ(card->activationPowerReductionFor(state, P1, gp, idx, 1), 0) << "no Altar";
    o(gp).empower_cost_reduction = 1;
    EXPECT_EQ(card->activationPowerReductionFor(state, P1, gp, idx, 1), 1);
    EXPECT_EQ(card->activationPowerReductionFor(state, P1, gp, idx, 0), 0)
        << "[1] OR [A]: nothing when the Energy took the reduction";
}

// ─── Aphelios, Exalted: which runes are readied is a choice ─────────────────

TEST_F(SweepCostsTest, Aphelios_ReadiesTheTwoRunesThePlayerChooses) {
    auto aphelios = addUnit(P1, kAphelios, 4);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script(
        {0, static_cast<int>(r3.raw()), static_cast<int>(r2.raw()), 0}));   // mode; two runes; "None"
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent);
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

// ─── One buff-spend primitive and one attach path (9.D13, 9.D6) ─────────────

TEST_F(SweepCostsTest, BuffSpentAsACost_IsCreditedToThePlayerWhoSpentIt) {
    // Fae Dragon: "When you spend a buff, play a Gold token exhausted." Both
    // players have one; P2 spends the buff to play Wallop for free.
    addUnit(P1, kFaeDragon, 3);
    addUnit(P2, kFaeDragon, 3);
    auto unit = addUnit(P2, kInvalidId, 2);
    o(unit).buff_count = 1;
    o(unit).is_exhausted = true;
    auto wallop = addToHand(P2, kWallop);
    setPool(P2, 0, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = P2;
    i.card = wallop;
    i.targets = {unit};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); }, nullptr, &agent, P2);
    ASSERT_EQ(o(unit).buff_count, 0);
    int gold_p1 = 0, gold_p2 = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.isToken() && obj.location.has_value())
            (obj.controller == P1 ? gold_p1 : gold_p2)++;
    EXPECT_EQ(gold_p2, 1) << "the spender's Fae Dragon";
    EXPECT_EQ(gold_p1, 0) << "not the other player's";
}

TEST_F(SweepCostsTest, QuickDraw_AttachesThroughTheExecutorsAttachPath) {
    // Long Sword: [Quick-Draw] "When you play it, attach it to a unit you
    // control." The engine had its own attach beside EffectExecutor::attachGear.
    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = addToHand(P1, kLongSword);
    int attached_events = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == sword && e.what_changed == "attached") ++attached_events;
    });
    playThroughEngine(sword);
    conn.disconnect();
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_TRUE(o(sword).is_rules_text_inactive) << "CR 718.2";
    EXPECT_EQ(o(unit).attachment_might_bonus, o(sword).might_bonus);
    EXPECT_EQ(attached_events, 1);
}

// ─── Endless Riches: play from the trash, and the trash is replaced ─────────

TEST_F(SweepCostsTest, EndlessRiches_CardsArePlayedFromTheTrashAtTheirOwnCost) {
    auto riches = addGear(P1, kEndlessRiches);
    (void)riches;
    auto poro = addToTrash(P1, kPoutyPoro);          // unit, [2]
    auto wallop = addToTrash(P1, kWallop);           // spell, [2]
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).is_exhausted = true;
    auto trashPlay = [&](GameObjectId card) -> std::optional<Intent> {
        for (const auto& i : legal())
            if (i.card == card && i.play_source == Intent::PlaySource::Trash) return i;
        return std::nullopt;
    };
    setPool(P1, 1, 0);
    EXPECT_FALSE(trashPlay(poro).has_value()) << "its normal cost is still owed";
    setPool(P1, 2, 0);
    auto offer = trashPlay(poro);
    ASSERT_TRUE(offer.has_value()) << "a unit, not only a spell";
    execute(*offer);
    EXPECT_TRUE(onBoard(poro));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);

    setPool(P1, 2, 0);
    auto spell_offer = trashPlay(wallop);
    ASSERT_TRUE(spell_offer.has_value());
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    execute(*spell_offer, &agent);
    EXPECT_FALSE(o(unit).is_exhausted);
    EXPECT_EQ(o(wallop).zone, ZoneType::Banishment)
        << "the resolved spell would go to the trash: banished instead";
    EXPECT_FALSE(inTrash(P1, wallop));
}

TEST_F(SweepCostsTest, EndlessRiches_WhatWouldGoToTheTrashIsBanishedExceptFromTheDeck) {
    addGear(P1, kEndlessRiches);
    auto dying = addUnit(P1, kPoutyPoro, 2);
    auto in_hand = addToHand(P1, kPoutyPoro);
    auto top = addToDeck(P1, kPoutyPoro);
    auto theirs = addUnit(P2, kPoutyPoro, 2);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.recalculateContinuousEffects();   // the Riches' flags are continuous
        exec.killObject(dying);
        exec.killObject(theirs);
        exec.applyDiscard(P1, in_hand);
        exec.burn(P1, 1);
    });
    EXPECT_EQ(o(dying).zone, ZoneType::Banishment);
    EXPECT_FALSE(inTrash(P1, dying));
    EXPECT_EQ(o(in_hand).zone, ZoneType::Banishment);
    EXPECT_TRUE(inTrash(P1, top)) << "from the Main Deck: not replaced";
    EXPECT_TRUE(inTrash(P2, theirs)) << "only the Riches' controller's cards";
}

// ─── The play gate counts the card's cost and Deflect together (2.11) ───────

TEST_F(SweepCostsTest, PlayGate_ATargetWhoseDeflectCannotBePaidWithTheCostIsNotOffered) {
    // Frigid Touch costs [2]. One ready rune in base and one floating Energy
    // pay it — by exhausting the rune — and then nothing is left to recycle
    // for a Deflect of 2... but one recycle is: the rune just exhausted. So
    // Deflect 1 is payable together with the cost and Deflect 2 is not.
    auto spell = addToHand(P1, kFrigidTouch);
    auto one = addUnit(P2, kInvalidId, 5, 0);
    o(one).keywords.set(Keyword::Deflect);
    o(one).deflect_value = 1;
    auto two = addUnit(P2, kInvalidId, 5, 0);
    o(two).keywords.set(Keyword::Deflect);
    o(two).deflect_value = 2;
    setPool(P1, 1, 0);
    auto rune = addRune(P1, Domain::Mind);
    // The target; then the rune to exhaust for the second Energy of the cost.
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(one.raw()), static_cast<int>(rune.raw())}));
    play(spell, &agent);
    EXPECT_TRUE(agent.wasOffered(one));
    EXPECT_FALSE(agent.wasOffered(two)) << "cost + Deflect 2 is more than there is";
}

TEST_F(SweepCostsTest, PlayGate_ADiscountFromTheChosenTargetCountsBeforePayment) {
    // Irelia, Graceful: "Your spells that choose me cost [1] or [A] less."
    // With [1] for a [2] spell, she is the only unit it can choose.
    auto irelia = addUnit(P1, kIreliaGraceful, 3);
    auto other = addUnit(P1, kInvalidId, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });   // her continuous effect
    auto spell = addToHand(P1, kFrigidTouch);
    setPool(P1, 1, 0);
    ASSERT_TRUE(playOffered(spell)) << "the generator counts the discount of a target it can choose";
    EnginePathAgent agent(EnginePathAgent::choosing(irelia));
    play(spell, &agent);
    EXPECT_TRUE(agent.wasOffered(irelia));
    EXPECT_FALSE(agent.wasOffered(other)) << "[2] is out of reach without the discount";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_TRUE(inTrash(P1, spell)) << "it was played and resolved";
}

// ─── A cost paid while resolving is due once per chain item (9.A4) ──────────

TEST_F(SweepCostsTest, CostDueNow_IsPaidOnceWhenAPromptFollowsIt) {
    // No finalize pass (the card does not opt in): the cost is paid in the
    // first run of the callback, the prompt re-enters it, and it must not be
    // paid again.
    auto unit = addUnit(P1, kInvalidId, 2);
    addUnit(P1, kInvalidId, 2);
    auto spell = addToHand(P1, kTCostThenPrompt);
    setXp(P1, 5);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(spell, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_EQ(o(spell).card_counters["paid"], 1);
    EXPECT_EQ(state.player(P1).xp, 4);
    EXPECT_EQ(o(unit).buff_count, 1);
}

TEST_F(SweepCostsTest, CostDueNow_IsPaidOnceWhenTheCardIsPlayedByAnEffect) {
    // The same card played by another spell's effect.
    auto unit = addUnit(P1, kInvalidId, 2);
    addUnit(P1, kInvalidId, 2);
    auto inner = addToHand(P1, kTCostThenPrompt);
    auto outer = addToHand(P1, kTPlaysFromHand);
    setXp(P1, 5);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    play(outer, &agent);
    EXPECT_EQ(o(inner).card_counters["paid"], 1);
    EXPECT_EQ(state.player(P1).xp, 4);
    EXPECT_EQ(o(unit).buff_count, 1);
}

// ─── Weaponmaster: choices at finalization, unattach through the executor ──

TEST_F(SweepCostsTest, Weaponmaster_TheMayAndTheEquipmentAreChosenAtFinalization) {
    // The opponent gets priority only after the choice is made: when P2 is
    // first asked, P1's prompts are already answered and nothing is attached.
    auto sword = addGear(P1, kLongSword);
    auto wm = addToHand(P1, kArmedAssailant);
    EnginePathAgent p1(EnginePathAgent::choosing(sword, EnginePathAgent::May::Yes));
    struct Watcher : AgentInterface {
        EnginePathAgent* p1 = nullptr;
        GameState* st = nullptr;
        GameObjectId gear = kInvalidId;
        int p1_prompts_at_first_priority = -1;
        bool attached_at_first_priority = false;
        Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
            if (p1_prompts_at_first_priority < 0) {
                p1_prompts_at_first_priority = static_cast<int>(p1->prompts.size());
                attached_at_first_priority = s.getObject(gear).attached_to.has_value();
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.front();
        }
    } p2;
    p2.p1 = &p1;
    p2.gear = sword;
    playThroughEngine(wm, &p1, std::nullopt, &p2);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{wm});
    EXPECT_EQ(p2.p1_prompts_at_first_priority, 2) << "the may and the Equipment, before priority";
    EXPECT_FALSE(p2.attached_at_first_priority) << "attached as the ability resolves";
}

TEST_F(SweepCostsTest, Weaponmaster_TakingAnAttachedEquipmentAnnouncesTheUnattach) {
    auto holder = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    o(sword).attached_to = holder;
    o(holder).attachments.push_back(sword);
    o(holder).attachment_might_bonus = o(sword).might_bonus;
    o(holder).recomputeMight();
    int unattached = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == sword && e.what_changed == "detached") ++unattached;
    });
    auto wm = addToHand(P1, kArmedAssailant);
    EnginePathAgent agent(EnginePathAgent::choosing(sword, EnginePathAgent::May::Yes));
    playThroughEngine(wm, &agent);
    conn.disconnect();
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{wm});
    EXPECT_TRUE(o(holder).attachments.empty());
    EXPECT_EQ(o(holder).attachment_might_bonus, 0);
    EXPECT_EQ(unattached, 1) << "EffectExecutor::unattachGear, not a hand-edited vector";
}

// ─── The cost contract is enforced (9.J2) ───────────────────────────────────

TEST_F(SweepCostsTest, CostContract_APromptAfterAPaidCostBeforeTheMarkerIsAnEngineError) {
    // kTCostThenPromptFinal opts into choices at finalization, pays under
    // costDueNow and then asks for a unit before its end-of-choices marker.
    // Re-entered after the prompt, it would find the cost due again; the
    // engine reports the card instead of charging the player twice.
    auto unit = addUnit(P1, kInvalidId, 2);
    addUnit(P1, kInvalidId, 2);
    auto spell = addToHand(P1, kTCostThenPromptFinal);
    setXp(P1, 5);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    try {
        play(spell, &agent);
        ADD_FAILURE() << "the finalize pass accepted a prompt after a paid cost";
    } catch (const std::logic_error& e) {
        EXPECT_NE(std::string(e.what()).find("Test Cost Then Prompt (finalized)"),
                  std::string::npos) << e.what();
    }
}

// ─── 9.J4: the tests the first report said were missing ─────────────────────

TEST_F(SweepCostsTest, PlayGate_TheTargetDiscountTakesAPowerWhereThereIsNoEnergy) {
    // Irelia, Graceful: "Your spells that choose me cost [1] or [A] less."
    // Test Power Touch costs one Fury Power and no Energy (a test card: see
    // TPowerTouch), so the discount can only be the "[A] less" half.
    auto irelia = addUnit(P1, kIreliaGraceful, 3);
    auto other = addUnit(P1, kInvalidId, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });   // her continuous effect
    auto spell = addToHand(P1, kTPowerTouch);
    setPool(P1, 0, 0);                                         // no Power at all
    ASSERT_TRUE(playOffered(spell)) << "free when it chooses Irelia";
    EnginePathAgent agent(EnginePathAgent::choosing(irelia));
    play(spell, &agent);
    EXPECT_TRUE(agent.wasOffered(irelia));
    EXPECT_FALSE(agent.wasOffered(other)) << "the Power is still owed for any other unit";
    EXPECT_EQ(o(irelia).buff_count, 1) << "it was played and resolved";
}

TEST_F(SweepCostsTest, RisenAltar_EmpowerCostsOnePowerLessThroughARealActivation) {
    // Risen Altar: "[Empower] costs of your units here cost [1] or [A] less."
    // Gangplank, Naval at the Altar: [Empower] [O][O] is paid with one [O].
    auto altar = state.createObject();
    o(altar).card_def_id = kRisenAltar;
    o(altar).card_type = CardType::Battlefield;
    o(altar).name = card_db.get(kRisenAltar).name;
    state.battlefields[0].card_object_id = altar;
    state.battlefields[0].controller = P1;
    auto here = addUnit(P1, kGangplankNaval, 4, 0);
    auto elsewhere = addUnit(P1, kGangplankNaval, 4);
    setPool(P1, 0, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 1;
    auto empowerOffer = [&](GameObjectId unit) -> std::optional<Intent> {
        for (const auto& i : legal())
            if (i.type == IntentType::ActivateAbility && i.ability_source == unit) return i;
        return std::nullopt;
    };
    EXPECT_FALSE(empowerOffer(elsewhere).has_value()) << "not at the Altar: [O][O]";
    auto offer = empowerOffer(here);
    ASSERT_TRUE(offer.has_value());
    execute(*offer);
    EXPECT_EQ(o(here).empowered, 1);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 0);
}

TEST_F(SweepCostsTest, PayAnyPower_SpendsFloatingPowerThenRecyclesExhaustedRunesFirst) {
    setPool(P1, 0, 1);
    auto ready = addRune(P1, Domain::Calm);
    auto exhausted = addRune(P1, Domain::Fury, /*exhausted=*/true);
    bool paid = false, refused = true;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        refused = !exec.payAnyPower(P1, 4);     // 1 floating + 2 runes: not enough
    });
    EXPECT_TRUE(refused);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1) << "all or nothing";
    EXPECT_TRUE(onBoard(ready));
    EXPECT_TRUE(onBoard(exhausted));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        paid = exec.payAnyPower(P1, 2);
    });
    EXPECT_TRUE(paid);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "floating Power first";
    EXPECT_FALSE(onBoard(exhausted)) << "then the exhausted rune is recycled";
    EXPECT_TRUE(onBoard(ready)) << "the ready rune can still give Energy";
    EXPECT_EQ(o(exhausted).zone, ZoneType::RuneDeck);
    // The floating Power and the rune: both spent (testing phase I3, by
    // design — old expectation 1, the rune only).
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 2);
}

TEST_F(SweepCostsTest, TrashReplayGrant_ADiscountAppliesToTheGrantCost) {
    // CR 356.4: a discount applies to whatever cost is being paid. Undying
    // Legion's "[3][R] from your trash" under "your cards cost [1] less".
    auto legion = addToTrash(P1, kUndyingLegion);
    state.player(P1).cards_played_this_turn = 1;
    setPool(P1, 2, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto trashPlay = [&]() -> std::optional<Intent> {
        for (const auto& i : legal())
            if (i.card == legion && i.play_source == Intent::PlaySource::Trash) return i;
        return std::nullopt;
    };
    EXPECT_FALSE(trashPlay().has_value()) << "[3] with [2]";
    PlayerState::CostModifier less;
    less.energy_reduction = 1;
    less.this_turn_only = true;
    state.player(P1).cost_modifiers.push_back(less);
    auto offer = trashPlay();
    ASSERT_TRUE(offer.has_value()) << "[3] - [1] = [2]";
    execute(*offer);
    EXPECT_TRUE(onBoard(legion));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

// ─── 9.L2: target discounts on plays that are not from hand ─────────────────

TEST_F(SweepCostsTest, TrashReplayGrant_ATargetsDiscountCountsTowardTheGrantCost) {
    // A grant to replay Frigid Touch from the trash for [2]; Irelia, Graceful
    // takes [1] off a spell that chooses her. With [1] the play is offered,
    // and she is the only unit it can choose.
    auto irelia = addUnit(P1, kIreliaGraceful, 3);
    auto other = addUnit(P1, kInvalidId, 3);
    auto spell = addToTrash(P1, kFrigidTouch);
    PlayerState::TrashReplayGrant grant;
    grant.card = spell;
    grant.energy = 2;
    state.player(P1).trash_replay_grants.push_back(grant);
    setPool(P1, 1, 0);
    std::optional<Intent> offer;
    for (const auto& i : legal())
        if (i.card == spell && i.play_source == Intent::PlaySource::Trash) offer = i;
    ASSERT_TRUE(offer.has_value()) << "[2] - [1] for choosing Irelia";
    EnginePathAgent agent(EnginePathAgent::choosing(irelia));
    execute(*offer, &agent);
    EXPECT_TRUE(agent.wasOffered(irelia));
    EXPECT_FALSE(agent.wasOffered(other));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(irelia).temp_might_bonus, -2) << "it was played and resolved";
}

TEST_F(SweepCostsTest, TrashReplayGrant_ATargetsDiscountTakesThePowerOfAGrantWithNoEnergy) {
    // The "[A] less" half: a grant of one Fury Power (Test Power Touch, a test
    // card), nothing to pay it with, playable by choosing Irelia.
    auto irelia = addUnit(P1, kIreliaGraceful, 3);
    auto other = addUnit(P1, kInvalidId, 3);
    auto spell = addToTrash(P1, kTPowerTouch);
    PlayerState::TrashReplayGrant grant;
    grant.card = spell;
    grant.power = 1;
    grant.power_domain = Domain::Fury;
    state.player(P1).trash_replay_grants.push_back(grant);
    setPool(P1, 0, 0);
    std::optional<Intent> offer;
    for (const auto& i : legal())
        if (i.card == spell && i.play_source == Intent::PlaySource::Trash) offer = i;
    ASSERT_TRUE(offer.has_value());
    EnginePathAgent agent(EnginePathAgent::choosing(irelia));
    execute(*offer, &agent);
    EXPECT_FALSE(agent.wasOffered(other));
    EXPECT_EQ(o(irelia).buff_count, 1);
}

// NOT A DEFECT (9.L2, hidden plays): a card played from facedown ignores its
// base cost (CR 811), so there is no cost for a target's discount to make
// payable — the generator has no cost gate and needs none. What a target adds
// is its Deflect, and that does gate the offer.
TEST_F(SweepCostsTest, HiddenPlay_HasNoCostGateAndATargetIsOfferedOnlyIfItsDeflectCanBePaid) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 3;
    addUnit(P1, kInvalidId, 2, 0);
    auto plain = addUnit(P2, kInvalidId, 3, 0);
    auto guarded = addUnit(P2, kInvalidId, 3, 0);
    o(guarded).keywords.set(Keyword::Deflect);
    o(guarded).deflect_value = 1;
    // Test Power Touch (printed cost: one Fury Power) facedown at battlefield 0
    // since last turn.
    auto spell = addToHand(P1, kTPowerTouch);
    auto& hand = state.player(P1).hand;
    hand.erase(std::remove(hand.begin(), hand.end(), spell), hand.end());
    o(spell).zone = ZoneType::FacedownZone;
    o(spell).is_hidden = true;
    o(spell).hidden_at = BattlefieldId::fromRaw(0);
    o(spell).hidden_on_turn = 2;
    state.battlefields[0].facedown.push_back(spell);
    // A hidden card is played with [Reaction] timing: an enemy item on the
    // chain (it does nothing), priority with P1.
    pushSpellOnChain(P2);
    state.chain.items.back().status = ChainItemStatus::Finalized;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.priority_holder = P1;
    auto offerOn = [&](GameObjectId target) -> std::optional<Intent> {
        for (const auto& i : legal())
            if (i.card == spell && !i.targets.empty() && i.targets.front() == target) return i;
        return std::nullopt;
    };
    setPool(P1, 0, 0);
    EXPECT_TRUE(offerOn(plain).has_value()) << "no resources at all: the base cost is ignored";
    EXPECT_FALSE(offerOn(guarded).has_value()) << "its Deflect cannot be paid";
    setPool(P1, 0, 1);
    auto offer = offerOn(guarded);
    ASSERT_TRUE(offer.has_value());
    execute(*offer);
    EXPECT_EQ(o(guarded).buff_count, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "the Deflect, and nothing else";
}

// ─── 9.N2: a spell that chooses in its finalize pass pays after choosing ────

TEST_F(SweepCostsTest, FinalizePassSpell_ADiscountFromWhatItChoosesCountsBeforePayment) {
    // Resonating Strike ([2] + 1 Power; kFinalizeResolve): "Choose a
    // battlefield you control and a unit you control at a different location."
    // Irelia, Graceful: "Your spells that choose me cost [1] or [A] less."
    // With [1] and one Power the spell is payable only by choosing Irelia.
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);                       // holds the battlefield
    auto irelia = addUnit(P1, kIreliaGraceful, 3);       // in base
    auto other = addUnit(P1, kInvalidId, 3);             // in base
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    ASSERT_TRUE(playOffered(spell)) << "the generator counts a choice that makes it payable";
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(irelia.raw())}));
    play(spell, &agent);
    EXPECT_TRUE(agent.wasOffered(irelia));
    EXPECT_FALSE(agent.wasOffered(other)) << "[2] is out of reach for any other unit";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(o(irelia).battlefieldId().value_or(BattlefieldId::fromRaw(99)), BattlefieldId::fromRaw(0)) << "it was played and resolved";
    EXPECT_TRUE(inTrash(P1, spell));
}

TEST_F(SweepCostsTest, FinalizePassSpell_WithoutADiscountItPaysItsFullCostAndIsNotOfferedShort) {
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    auto other = addUnit(P1, kInvalidId, 3);
    auto spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    EXPECT_FALSE(playOffered(spell)) << "nothing it can choose takes anything off";
    setPool(P1, 2, 1);
    ASSERT_TRUE(playOffered(spell));
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(other.raw())}));
    play(spell, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(other).battlefieldId().value_or(BattlefieldId::fromRaw(99)), BattlefieldId::fromRaw(0));
}

// ─── 9.T1: its Deflect is judged, and paid, with the spell's cost ───────────

TEST_F(SweepCostsTest, FinalizePassSpell_AUnitIsOfferedOnlyIfItsDeflectCanBePaidWithTheCost) {
    // Rocket Barrage ([4][B]; kFinalizeResolve): "Deal 4 to a unit in a base."
    // Its choices are made before its cost is paid, each one's Deflect in the
    // same step — so a unit is on offer only while the cost AND its Deflect
    // can be paid together.
    auto plain = addUnit(P2, kInvalidId, 9);
    auto guarded = addUnit(P2, kInvalidId, 9);
    o(guarded).keywords.set(Keyword::Deflect);
    o(guarded).deflect_value = 1;

    // [4] floating and one Mind rune: the rune is the spell's [B]. It could
    // pay the Deflect instead — and then the spell could not be paid for.
    auto spell = addToHand(P1, kRocketBarrage);
    setPool(P1, 4, 0);
    auto only_rune = addRune(P1, Domain::Mind);
    // Mode 0; the unit; then the rune to recycle for the [B].
    EnginePathAgent short_agent(EnginePathAgent::script(
        {0, static_cast<int>(plain.raw()), static_cast<int>(only_rune.raw())}));
    play(spell, &short_agent);
    EXPECT_TRUE(short_agent.wasOffered(plain));
    EXPECT_FALSE(short_agent.wasOffered(guarded)) << "[4][B] + its Deflect is more than there is";
    EXPECT_EQ(o(plain).damage_marked, 4);
    EXPECT_TRUE(inTrash(P1, spell)) << "the spell was paid for and resolved";

    // With one more Power the guarded unit is offered, and everything is spent.
    auto again = addToHand(P1, kRocketBarrage);
    setPool(P1, 4, 1);
    auto rune = addRune(P1, Domain::Mind);
    // Mode 0; the guarded unit; its Deflect: the floating Power (value 0) —
    // the payer names it, the rune being the other way that keeps the spell
    // payable (testing phase D9, by design: the engine used to take it
    // unasked); then the rune to recycle for the [B].
    EnginePathAgent agent(EnginePathAgent::script(
        {0, static_cast<int>(guarded.raw()), 0, static_cast<int>(rune.raw())}));
    play(again, &agent);
    EXPECT_TRUE(agent.wasOffered(guarded));
    EXPECT_EQ(o(guarded).damage_marked, 4);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_FALSE(onBoard(rune)) << "recycled for the [B]";
}

// ─── 9.T2: a hidden spell needs its choices at its battlefield (CR 811.1.d) ─

TEST_F(SweepCostsTest, HiddenPlay_AFinalizePassSpellNeedsSomethingToChooseAtItsBattlefield) {
    // Resonating Strike hidden at battlefield 0: "Choose a battlefield you
    // control and a unit you control at a different location." From facedown
    // the battlefield is the one it was hidden at.
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 3;
    addUnit(P1, kInvalidId, 2, 0);                      // here: not "at a different location"
    auto spell = addToHand(P1, kResonatingStrike);
    hideAt(spell, 0);
    reactionWindowFor(P1);
    auto offered = [&] {
        for (const auto& i : legal())
            if (i.card == spell) return true;
        return false;
    };
    EXPECT_FALSE(offered()) << "no unit of mine anywhere else to move here";
    addUnit(P1, kInvalidId, 2);                         // in base
    EXPECT_TRUE(offered());
}

// NOT A DEFECT (9.T2, Fox-Fire 253): "Kill any number of units" includes zero
// (CR 355.13), so the spell is playable with nothing to choose — from
// facedown as from hand.
TEST_F(SweepCostsTest, HiddenPlay_FoxFireMayBePlayedWithNothingToChoose) {
    state.battlefields[0].controller = P1;
    state.turn.turn_number = 3;
    addUnit(P1, kInvalidId, 9, 0);                      // too big for Fox-Fire
    auto spell = addToHand(P1, kFoxFire);
    hideAt(spell, 0);
    reactionWindowFor(P1);
    bool offered = false;
    for (const auto& i : legal())
        if (i.card == spell) offered = true;
    EXPECT_TRUE(offered);
}

// ─── 9.N5: Weaponmaster needs an Equip cost to pay (CR 821.1.c.4) ───────────

TEST_F(SweepCostsTest, Weaponmaster_AnEquipmentWithNoEquipCostCannotBeEquippedThisWay) {
    auto bare = addGear(P1, kInvalidId, /*equipment=*/true);   // no card, so no Equip cost
    auto wm = addToHand(P1, kArmedAssailant);
    EnginePathAgent agent(EnginePathAgent::choosing(bare));
    playThroughEngine(wm, &agent);
    ASSERT_TRUE(onBoard(wm));
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to offer";
    EXPECT_FALSE(o(bare).attached_to.has_value());
}

// ─── 9.M26: a typed discard as an activation cost is the player's choice ────

TEST_F(SweepCostsTest, TypedDiscardCost_SkyCrusierDiscardsTheGearThePlayerPicks) {
    // "Discard a gear, [1], [E]: Deal 4 to a unit at a battlefield."
    auto crusier = addUnit(P1, kSkyCrusier, 3);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto not_gear = addToHand(P1, kPoutyPoro);
    setPool(P1, 1, 0);
    auto offers = [&] {
        std::vector<Intent> out;
        for (const auto& i : legal())
            if (i.type == IntentType::ActivateAbility && i.ability_source == crusier)
                out.push_back(i);
        return out;
    };
    EXPECT_TRUE(offers().empty()) << "no gear in hand: the cost can't be paid";
    auto gear1 = addToHand(P1, kLongSword);
    auto gear2 = addToHand(P1, kSkyfallOfAreion);
    auto act = offers();
    ASSERT_FALSE(act.empty());
    // The target is chosen first, then the cost is paid (CR 403.2 -> 405.1;
    // testing phase G15: the discard used to come first).
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(enemy.raw()), static_cast<int>(gear2.raw())}));
    execute(act.front(), &agent);
    EXPECT_TRUE(agent.wasOffered(gear1));
    EXPECT_FALSE(agent.wasOffered(not_gear)) << "only gear pays a \"Discard a gear\" cost";
    EXPECT_TRUE(inTrash(P1, gear2)) << "the one the player picked";
    EXPECT_TRUE(inHand(P1, gear1));
    EXPECT_TRUE(inHand(P1, not_gear));
    EXPECT_EQ(o(enemy).damage_marked, 4);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(SweepCostsTest, TypedDiscardCost_MelEmpowersByDiscardingTheSpellThePlayerPicks) {
    // Mel, Defiant Soul: "[Empower] — Discard a spell".
    auto mel = addUnit(P1, kMelDefiantSoul, 3);
    auto not_spell = addToHand(P1, kPoutyPoro);
    auto offers = [&] {
        std::vector<Intent> out;
        for (const auto& i : legal())
            if (i.type == IntentType::ActivateAbility && i.ability_source == mel)
                out.push_back(i);
        return out;
    };
    EXPECT_TRUE(offers().empty()) << "no spell in hand";
    auto spell1 = addToHand(P1, kWallop);
    auto spell2 = addToHand(P1, kFrigidTouch);
    auto act = offers();
    ASSERT_FALSE(act.empty());
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(spell2.raw())}));
    execute(act.front(), &agent);
    EXPECT_TRUE(agent.wasOffered(spell1));
    EXPECT_FALSE(agent.wasOffered(not_spell));
    EXPECT_TRUE(inTrash(P1, spell2));
    EXPECT_TRUE(inHand(P1, spell1));
    EXPECT_EQ(o(mel).empowered, 1);
}

// ─── 9.M27: Energy comes from runes in the player's base, lowest id first ───

TEST_F(SweepCostsTest, PayEnergy_ExhaustsTheLowestIdBaseRuneAndNoRuneOutsideTheBase) {
    // Jax, Unrelenting: "When you attach an Equipment to me, you may pay [1]
    // to draw 1." pays through payEnergy (card_helpers.h).
    auto jax = addUnit(P1, kJaxUnrelenting, 3);
    auto stray = addRune(P1, Domain::Calm);          // lowest id, but not in the base
    o(stray).zone = ZoneType::BattlefieldZone;
    o(stray).location = BattlefieldLocation{0};
    addToDeck(P1, kPoutyPoro);
    addToDeck(P1, kPoutyPoro);
    setPool(P1, 0, 0);

    // Only the stray rune: nothing to pay with, so the may is not offered.
    EnginePathAgent none(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(jax, TriggerType::WhenEquipmentAttachedToMe, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_FALSE(o(stray).is_exhausted);
    EXPECT_EQ(handSize(P1), 0);

    auto first = addRune(P1, Domain::Fury);
    auto second = addRune(P1, Domain::Mind);
    // The payer names the rune (testing phase D9, by design: the engine used
    // to exhaust the lowest-id one unasked). Both base runes are offered, the
    // stray one is not.
    EnginePathAgent yes(EnginePathAgent::script({1, static_cast<int>(first.raw())}));
    fireTriggerThroughEngine(jax, TriggerType::WhenEquipmentAttachedToMe, &yes);
    EXPECT_EQ(handSize(P1), 1);
    ASSERT_EQ(yes.prompts.size(), 2u);
    EXPECT_EQ(yes.prompts[1].size(), 2u) << "the two runes in the base";
    EXPECT_FALSE(yes.wasOffered(stray));
    EXPECT_TRUE(o(first).is_exhausted) << "the rune named";
    EXPECT_FALSE(o(second).is_exhausted);
    EXPECT_FALSE(o(stray).is_exhausted) << "a rune outside the base gives no Energy";
}

// ─── 9.R2: what was paid for one play is forgotten when the next begins ─────

/// The opponent answers the first thing they get priority on with Test
/// Counter (a [Reaction] test spell that counters the item under it by
/// peek-and-pop, as the engine's counters do), then passes.
class CountersOnceAgent : public AgentInterface {
public:
    GameObjectId counter_card = kInvalidId;
    int played = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (played == 0)
            for (const auto& i : legal)
                if (i.card == counter_card && (i.type == IntentType::PlayReaction ||
                                               i.type == IntentType::PlayActionCard ||
                                               i.type == IntentType::PlayCard)) {
                    ++played;
                    return i;
                }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

TEST_F(SweepCostsTest, PaidFlag_ZaunPunkReplayedWithoutPayingDoesNotKillAGear) {
    auto mine = addGear(P1);
    auto theirs = addGear(P2);
    auto punk = addToHand(P1, kZaunPunk);
    setPool(P1, 3, 0);
    // Pay; which gear; then the play trigger's own "kill a gear" pick, made
    // as the trigger is finalized (it is countered before it resolves).
    EnginePathAgent pays(EnginePathAgent::script(
        {1, static_cast<int>(mine.raw()), static_cast<int>(theirs.raw())}));
    CountersOnceAgent counters;
    counters.counter_card = addToHand(P2, kTCounter);
    playThroughEngine(punk, &pays, std::nullopt, &counters, /*fund=*/false);
    ASSERT_EQ(counters.played, 1) << "the play trigger was on the chain and was countered";
    ASSERT_FALSE(onBoard(mine)) << "the additional cost was paid";
    ASSERT_TRUE(onBoard(theirs)) << "the trigger never resolved";

    // Back to hand, and played again with no gear to pay the cost with.
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.bounceToHand(punk); });
    ASSERT_TRUE(inHand(P1, punk));
    setPool(P1, 3, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    play(punk, &agent);
    EXPECT_TRUE(onBoard(punk));
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to pay with, so nothing is asked";
    EXPECT_TRUE(onBoard(theirs)) << "this play paid nothing: no \"kill a gear\"";
}

TEST_F(SweepCostsTest, PaidFlag_SafetyInspectorReplayedWithoutPayingStillKillsItsOwnUnit) {
    // "You may spend 3 XP as an additional cost ... if you paid it, skip your kill."
    auto my_unit = addUnit(P1, kInvalidId, 2);
    auto their_unit = addUnit(P2, kInvalidId, 2);
    auto inspector = addToHand(P1, kSafetyInspector);
    const auto& def = card_db.get(kSafetyInspector);
    setPool(P1, def.energy_cost, def.power_cost);
    setXp(P1, 3);
    EnginePathAgent pays(EnginePathAgent::script({1}));
    CountersOnceAgent counters;
    counters.counter_card = addToHand(P2, kTCounter);
    playThroughEngine(inspector, &pays, std::nullopt, &counters, /*fund=*/false);
    ASSERT_EQ(counters.played, 1) << "the play trigger was on the chain and was countered";
    ASSERT_EQ(state.player(P1).xp, 0) << "the additional cost was paid";
    ASSERT_TRUE(onBoard(my_unit));

    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.bounceToHand(inspector); });
    ASSERT_TRUE(inHand(P1, inspector));
    setPool(P1, def.energy_cost, def.power_cost);          // and no XP this time
    EnginePathAgent agent(EnginePathAgent::choosing(my_unit));
    EnginePathAgent them(EnginePathAgent::choosing(their_unit));
    playThroughEngine(inspector, &agent, std::nullopt, &them, /*fund=*/false);
    EXPECT_TRUE(onBoard(inspector));
    EXPECT_FALSE(onBoard(my_unit)) << "unpaid this time: I kill one of my units too";
    EXPECT_FALSE(onBoard(their_unit));
}

// ─── 9.AA1: every finalize-pass spell says what it can choose ───────────────

TEST_F(SweepCostsTest, EveryFinalizePassSpellDeclaresWhatItCanChoose) {
    // The play gate offers a spell that is too dear on its own when a choice
    // would make it payable; only the card knows its pick list. A spell that
    // leaves Card::mayChooseAtFinalization Undeclared is never offered that way.
    int checked = 0;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (!card || def.card_type != CardType::Spell) continue;
        if (!(card->choicesAtFinalization() & Card::kFinalizeResolve)) continue;
        if (card->needsPlayTimeTarget()) continue;   // chosen as it is played: the engine sees it
        ++checked;
        EXPECT_NE(card->mayChooseAtFinalization(state, P1, kInvalidId),
                  Card::FinalizeChoice::Undeclared)
            << def.name << " (" << id << ")";
    }
    EXPECT_GE(checked, 30) << "the finalize-pass spells were found";
}

TEST_F(SweepCostsTest, FinalizePassSpell_ASecondSpellIsOfferedThroughADiscount) {
    // Challenge ([2] + 1 Power): "Choose a friendly unit and an enemy unit."
    auto irelia = addUnit(P1, kIreliaGraceful, 3, 0);
    auto other = addUnit(P1, kInvalidId, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kChallenge);
    setPool(P1, 1, 1);
    ASSERT_TRUE(playOffered(spell)) << "[2] - [1] for choosing Irelia";
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(irelia.raw()), static_cast<int>(enemy.raw())}));
    play(spell, &agent);
    EXPECT_FALSE(agent.wasOffered(other)) << "no discount for choosing any other unit";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(enemy).damage_marked, 3) << "it was played and resolved";
}

TEST_F(SweepCostsTest, FinalizePassSpell_ASpellWithNoDeclaredTargetsIsOfferedThroughItsDeclaration) {
    // Curtain Call ([4]) declares no play-time targets; the generator knows
    // it could choose Irelia only from Card::mayChooseAtFinalization.
    auto irelia = addUnit(P1, kIreliaGraceful, 9);          // in base
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kCurtainCall);
    setPool(P1, 3, 0);
    ASSERT_TRUE(playOffered(spell)) << "[4] - [1] for choosing Irelia";
    // Mode 2: "Deal 3 to a unit at a base"; then Irelia.
    EnginePathAgent agent(EnginePathAgent::script(
        {2, static_cast<int>(irelia.raw())}));
    play(spell, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(irelia).damage_marked, 3);
}

TEST_F(SweepCostsTest, TargetDiscount_BelongsToTheControllerOfTheChosenUnit) {
    // "YOUR spells that choose me cost [1] or [A] less": the opponent's
    // Irelia takes nothing off my spell.
    auto theirs = addUnit(P2, kIreliaGraceful, 3, 0);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kFrigidTouch);        // [2]
    setPool(P1, 1, 0);
    EXPECT_FALSE(playOffered(spell));
    (void)theirs;
}

// ─── 9.DD1: a play offered through a discount cannot end unpaid ─────────────

TEST_F(SweepCostsTest, FinalizePassSpell_AChoiceThatWouldLeaveItUnpayableIsNotOffered) {
    // Resonating Strike with [1] and one Power is playable only by choosing
    // Irelia. She stands at battlefield 0, so choosing that battlefield would
    // rule her out ("a unit at a different location"): only battlefield 1 is
    // put to the player, then only Irelia.
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    auto irelia = addUnit(P1, kIreliaGraceful, 3, 0);
    auto holder = addUnit(P1, kInvalidId, 2, 1);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    ASSERT_TRUE(playOffered(spell));
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(irelia.raw())}));
    play(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    ASSERT_EQ(agent.prompts[0].size(), 1u) << "battlefield 0 would strand the spell";
    EXPECT_EQ(agent.prompts[0][0].chosen_value.value_or(-1), 1);
    EXPECT_FALSE(agent.wasOffered(holder));
    EXPECT_TRUE(inTrash(P1, spell)) << "played and paid for";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(o(irelia).battlefieldId().value_or(BattlefieldId::fromRaw(99)), BattlefieldId::fromRaw(1));
}

TEST_F(SweepCostsTest, FinalizePassSpell_IsNotOfferedWhenNoSequenceOfItsChoicesPaysForIt) {
    // A test spell that declares it could choose any friendly unit but in
    // fact chooses nothing. With Irelia in play the declaration says a
    // discount is within reach; no sequence of its choices earns it.
    addUnit(P1, kIreliaGraceful, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kTFalseDeclaration);   // [2]
    setPool(P1, 1, 0);
    EXPECT_FALSE(playOffered(spell));
    setPool(P1, 2, 0);
    EXPECT_TRUE(playOffered(spell)) << "at its full cost it is";
}

TEST_F(SweepCostsTest, FinalizePassSpell_ProbingAnOfferLeavesNoTrace) {
    // To know whether to offer the play the engine makes the spell's choices
    // on the side. Nothing of that is seen or kept.
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    addUnit(P1, kIreliaGraceful, 3, 0);
    addUnit(P1, kInvalidId, 2, 1);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    int logs = 0, events_seen = 0;
    std::vector<boost::signals2::connection> conns;
    conns.push_back(events.on_log.connect([&](const LogEvent&) { ++logs; }));
    conns.push_back(events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent&) { ++events_seen; }));
    conns.push_back(events.on_left_board.connect([&](const LeftBoardEvent&) { ++events_seen; }));
    conns.push_back(events.on_card_played.connect([&](const CardPlayedEvent&) { ++events_seen; }));
    const uint64_t before = Card::costStateFingerprint(state);
    const auto chain_id_before = state.chain.next_id;
    const auto history_before = state.action_history.size();
    EnginePathAgent agent;   // must not be asked anything
    bool offered = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.card == spell) offered = true;
    }, &agent);
    for (auto& c : conns) c.disconnect();
    ASSERT_TRUE(offered) << "the probe ran and found the sequence";
    EXPECT_EQ(logs, 0);
    EXPECT_EQ(events_seen, 0);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(Card::costStateFingerprint(state), before);
    EXPECT_EQ(state.chain.next_id, chain_id_before);
    EXPECT_EQ(state.action_history.size(), history_before);
    EXPECT_TRUE(inHand(P1, spell));
    EXPECT_FALSE(state.chain.exists());
}

// ─── 9.HH4: what a probe costs ──────────────────────────────────────────────

TEST_F(SweepCostsTest, FinalizePassSpell_AProbeCopiesTheStateOncePerRun) {
    // The probe plays on a copy of the game and the game itself is set aside
    // untouched: one copy of the GameState per run of the spell's choices (a
    // probe that needed one run used to make five copies). Resonating Strike
    // with Irelia at battlefield 0 needs two runs: battlefield 0 first (she
    // is ruled out), then battlefield 1.
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    const GameObjectId irelia = addUnit(P1, kIreliaGraceful, 3, 0);
    addUnit(P1, kInvalidId, 2, 1);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    const GameObjectId spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    const uint64_t before = Card::costStateFingerprint(state);
    withEngine([&](GameEngine& e) {
        // References taken before the probe still point at the game's objects
        // after it: the game was moved aside and back, not copied over.
        const GameObject* irelia_before = &e.state().getObject(irelia);
        const GameObjectId* hand_before = e.state().player(P1).hand.data();
        bool offered = false;
        for (const auto& i : e.generateLegalActions())
            if (i.card == spell) offered = true;
        EXPECT_TRUE(offered);
        EXPECT_EQ(e.testHook_prePlayProbes(), 1);
        EXPECT_EQ(e.testHook_prePlayRunsLast(), 2);
        EXPECT_EQ(e.testHook_prePlayStateCopies(), 2) << "one per run";
        EXPECT_EQ(&e.state().getObject(irelia), irelia_before);
        EXPECT_EQ(e.state().player(P1).hand.data(), hand_before);
        // And the board index of the game that came back answers for it.
        EXPECT_EQ(e.state().unitsAt(LocationId{BattlefieldLocation{0}}, P1),
                  std::vector<GameObjectId>{irelia});
    });
    EXPECT_EQ(Card::costStateFingerprint(state), before);
    EXPECT_TRUE(inHand(P1, spell));
}

// ─── 9.HH3: the probe's bound and the repeatability it relies on ────────────

TEST_F(SweepCostsTest, FinalizePassSpell_ReachingTheProbeBoundIsAnErrorNotAWithheldPlay) {
    // Test Endless Choices has 512 sequences of choices and no discount in any
    // of them. With [1] of its [2] and Irelia in play (so its declaration says
    // a discount is within reach) the engine cannot walk them all: it says so,
    // naming the card, instead of deciding the play is not legal.
    addUnit(P1, kIreliaGraceful, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kTEndlessChoices);
    setPool(P1, 1, 0);
    const uint64_t before = Card::costStateFingerprint(state);
    int logs = 0;
    auto conn = events.on_log.connect([&](const LogEvent&) { ++logs; });
    try {
        (void)playOffered(spell);
        ADD_FAILURE() << "the bound was reached and the play silently withheld";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("Test Endless Choices"), std::string::npos)
            << ex.what();
        EXPECT_NE(std::string(ex.what()).find("more than 256 sequences"), std::string::npos)
            << ex.what();
    }
    conn.disconnect();
    // The failed probe left nothing behind either.
    EXPECT_EQ(logs, 0);
    EXPECT_EQ(Card::costStateFingerprint(state), before);
    EXPECT_TRUE(inHand(P1, spell));
    EXPECT_FALSE(state.chain.exists());
    EXPECT_FALSE(events.holding());
    // At its full cost nothing is probed: it is offered and plays.
    setPool(P1, 2, 0);
    EXPECT_TRUE(playOffered(spell));
}

TEST_F(SweepCostsTest, FinalizePassSpell_AContractErrorInsideTheProbeIsNotAWithheldPlay) {
    // The probe runs the card's own finalize pass. A card that breaks a
    // contract there (here: a pass that ends before its marker) is reported,
    // as it is in the real pass — not counted as "no payable sequence".
    addUnit(P1, kIreliaGraceful, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kTNoMarkerDear);   // [2]
    setPool(P1, 1, 0);
    const uint64_t before = Card::costStateFingerprint(state);
    try {
        (void)playOffered(spell);
        ADD_FAILURE() << "the card's contract error was swallowed and the play withheld";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("Test No Marker (dear)"), std::string::npos)
            << ex.what();
        EXPECT_NE(std::string(ex.what()).find("end-of-choices marker"), std::string::npos);
    }
    EXPECT_EQ(Card::costStateFingerprint(state), before);
    EXPECT_TRUE(inHand(P1, spell));
    EXPECT_FALSE(state.chain.exists());
    EXPECT_FALSE(events.holding());
}

TEST_F(SweepCostsTest, FinalizePassSpell_APassThatIsNotRepeatableMakesTheViablePassThrow) {
    // The viable pass runs a spell's choices several times from copies of the
    // same state and relies on meeting the same prompts each time. Test
    // Unrepeatable offers Irelia the first time and nothing afterwards.
    addUnit(P1, kIreliaGraceful, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kTUnrepeatable);   // [2]
    setPool(P1, 1, 0);
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = P1;
    i.card = spell;
    try {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); });
        ADD_FAILURE() << "an unrepeatable pass went through the viable pass";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("choosePrePlayViable"), std::string::npos)
            << ex.what();
        EXPECT_NE(std::string(ex.what()).find("Test Unrepeatable"), std::string::npos)
            << ex.what();
        EXPECT_NE(std::string(ex.what()).find("not repeatable"), std::string::npos) << ex.what();
    }
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "nothing was paid";
}

TEST_F(SweepCostsTest, FinalizePassSpell_APassThatDiffersOnlyInTheRealRunMakesTheFinalRunThrow) {
    // The third "not repeatable" throw: every run that tries the choices out
    // agrees with every other, the player's answer is taken, and the run that
    // is made for real does not meet the prompt that answer was given to.
    // Two Irelias, so two options keep the spell payable; the second is
    // picked, and in the real run Test Real Run Only offers only the first.
    const GameObjectId first = addUnit(P1, kIreliaGraceful, 3);
    const GameObjectId second = addUnit(P1, kIreliaGraceful, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    const GameObjectId spell = addToHand(P1, kTRealRunOnly);   // [2]
    setPool(P1, 1, 0);
    ASSERT_TRUE(playOffered(spell)) << "the probe, which is quiet, finds a payable sequence";
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = P1;
    i.card = spell;
    EnginePathAgent agent(EnginePathAgent::choosing(second));
    try {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); }, &agent);
        ADD_FAILURE() << "the real run did not follow the choice and the play went through";
    } catch (const std::logic_error& ex) {
        const std::string what = ex.what();
        EXPECT_NE(what.find("choosePrePlayViable"), std::string::npos) << what;
        EXPECT_NE(what.find("Test Real Run Only"), std::string::npos) << what;
        EXPECT_NE(what.find("did not complete the choices it had just made"), std::string::npos)
            << what;
        EXPECT_NE(what.find("not repeatable"), std::string::npos) << what;
    }
    ASSERT_EQ(agent.prompts.size(), 1u) << "the player was asked once, before the real run";
    EXPECT_EQ(agent.prompts[0].size(), 2u) << "both Irelias kept the spell payable";
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1) << "nothing was paid";
    EXPECT_FALSE(events.holding());
}

// ─── 9.AA3: a grant play carries the discount of its earlier choices ────────

TEST_F(SweepCostsTest, TrashReplayGrant_ALaterChoiceKeepsTheDiscountOfAnEarlierOne) {
    // Bonds of Strength ("Give two friendly units each +1 [M]") replayed from
    // the trash for [2], with [1]: Irelia first earns the discount; the second
    // unit must then still be on offer.
    auto irelia = addUnit(P1, kIreliaGraceful, 3);
    auto other = addUnit(P1, kInvalidId, 3);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToTrash(P1, kBondsOfStrength);
    PlayerState::TrashReplayGrant grant;
    grant.card = spell;
    grant.energy = 2;
    state.player(P1).trash_replay_grants.push_back(grant);
    setPool(P1, 1, 0);
    std::optional<Intent> offer;
    for (const auto& i : legal())
        if (i.card == spell && i.play_source == Intent::PlaySource::Trash) offer = i;
    ASSERT_TRUE(offer.has_value());
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(irelia.raw()), static_cast<int>(other.raw())}));
    execute(*offer, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(irelia).temp_might_bonus, 1);
    EXPECT_EQ(o(other).temp_might_bonus, 1) << "the second pick was still payable";
}

// ─── 9.DD1: over whole random games no play changes nothing ─────────────────

TEST_F(SweepCostsTest, NoPlayChangesNothingOverTwoHundredRandomGames) {
    // A deck built so that the case actually arises: Irelia, Graceful with
    // the finalize-pass spells of her Domains (Resonating Strike, Switcheroo,
    // Star-Crossed, Defiant Dance, Last Breath, Flash), mirrored, played by
    // two random agents. A play that changed nothing would be seen from
    // outside as the same state at the player's next decision: there must be
    // none. That the case is reached is shown by the plays made with less
    // Energy at hand than the card's printed cost.
    const auto path = std::filesystem::temp_directory_path() /
                      "riftbound_sweep_costs_irelia_deck.txt";
    {
        std::ofstream f(path);
        f << "Legend:\n1 Blade Dancer\n\nChampion:\n1 Irelia, Graceful\n\nMainDeck:\n"
             "2 Irelia, Graceful\n3 Resonating Strike\n3 Switcheroo\n3 Star-Crossed\n"
             "3 Defiant Dance\n3 Last Breath\n3 Flash\n3 Mystic Poro\n3 Stalwart Poro\n"
             "3 Lonely Poro\n3 Traveling Merchant\n3 Tideturner\n3 Clockwork Keeper\n"
             "2 Shipyard Skulker\n\nBattlefields:\n1 Emperor's Dais\n1 Seat of Power\n"
             "1 Star Spring\n\nRunes:\n6 Calm Rune\n6 Chaos Rune\n";
    }
    const auto deck = DeckValidator::loadFromDeckList(path.string(), card_db);
    std::filesystem::remove(path);

    struct Watching : AgentInterface {
        explicit Watching(uint64_t seed) : random(seed) {}
        RandomAgent random;
        // The play chosen at this player's last top-level decision, and the
        // state it was chosen in.
        GameObjectId last_play = kInvalidId;
        uint64_t last_state = 0;
        int undone = 0, offered_again = 0, short_plays = 0;
        const CardDB* db = nullptr;
        Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
            const bool is_prompt = !legal.empty() && std::all_of(legal.begin(), legal.end(),
                [](const Intent& i) { return i.type == IntentType::MakeChoice; });
            if (!is_prompt) {
                const uint64_t now = Card::costStateFingerprint(state);
                if (last_play != kInvalidId && now == last_state) {
                    ++undone;   // the play changed nothing: it was attempted and undone
                    for (const auto& i : legal)
                        if (i.card == last_play && (i.type == IntentType::PlayCard ||
                                                    i.type == IntentType::PlayActionCard ||
                                                    i.type == IntentType::PlayReaction))
                            ++offered_again;
                }
                last_play = kInvalidId;
            }
            Intent chosen = random.selectAction(state, legal);
            if (!is_prompt && (chosen.type == IntentType::PlayCard ||
                               chosen.type == IntentType::PlayActionCard ||
                               chosen.type == IntentType::PlayReaction)) {
                last_play = chosen.card;
                last_state = Card::costStateFingerprint(state);
                const auto& obj = state.getObject(chosen.card);
                if (db && obj.isSpell() && obj.card_def_id != kInvalidId &&
                    resource::payableEnergy(state, obj.controller, SpendFor::Spell) <
                        db->get(obj.card_def_id).energy_cost)
                    ++short_plays;
            }
            return chosen;
        }
    };

    int undone = 0, offered_again = 0, short_plays = 0, games = 0;
    long probes = 0, probe_runs = 0, state_copies = 0;
    int most_runs = 0;
    for (uint64_t seed = 1; seed <= 200; ++seed) {
        EventBus bus;
        GameEngine engine(card_db, bus, card_registry);
        Watching p1(seed * 2), p2(seed * 2 + 1);
        p1.db = p2.db = &card_db;
        engine.runGame(deck, deck, p1, p2, seed);
        ++games;
        undone += p1.undone + p2.undone;
        offered_again += p1.offered_again + p2.offered_again;
        short_plays += p1.short_plays + p2.short_plays;
        probes += engine.testHook_prePlayProbes();
        probe_runs += engine.testHook_prePlayRunsTotal();
        state_copies += engine.testHook_prePlayStateCopies();
        most_runs = std::max(most_runs, engine.testHook_prePlayRunsMax());
    }
    EXPECT_EQ(games, 200);
    EXPECT_GT(short_plays, 0) << "the batch did reach the case (else it shows nothing)";
    EXPECT_EQ(undone, 0) << "plays that changed nothing";
    EXPECT_EQ(offered_again, 0);
    std::cout << "[ " << games << " random games: " << short_plays
              << " spells played with less Energy at hand than their printed cost, " << undone
              << " plays that changed nothing ]\n";
    // 9.HH4: what the probes cost. A probe that finds its answer in its first
    // run copies the GameState once (to play on); it was five copies.
    std::cout << "[ " << probes << " probes, " << probe_runs << " finalize-pass runs in all (most in one search: "
              << most_runs << "), " << state_copies << " GameState copies ]\n";
    EXPECT_GT(probes, 0);
    EXPECT_LE(state_copies, probes + 8 * short_plays)
        << "about one copy per probe and six per viable pass (it was five per probe and more "
           "per viable pass)";
    EXPECT_LT(most_runs, 16) << "no search of this deck's spells comes near the bound of 256";
}

// ─── 9.CC3: each declaration agrees with the card's real pick list ──────────

/// Follows a prefix of option indices, then takes the first option, and
/// records how many options each prompt had — for walking every sequence of
/// choices a card offers.
class WalkAgent : public AgentInterface {
public:
    std::vector<int> prefix;
    std::vector<int> option_counts;
    std::set<std::string>* labels = nullptr;   // every option label met, if wanted
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool is_choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (!is_choice) {
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.front();
        }
        if (labels)
            for (const auto& i : legal)
                if (!i.choice_label.empty()) labels->insert(i.choice_label);
        const size_t k = option_counts.size();
        option_counts.push_back(static_cast<int>(legal.size()));
        const int idx = k < prefix.size() ? prefix[k] : 0;
        return legal[std::min<size_t>(static_cast<size_t>(idx), legal.size() - 1)];
    }
};

TEST_F(SweepCostsTest, FinalizePassSpellDeclarationsMatchTheirPickLists) {
    // For every finalize-pass spell: on varied boards, walk every way the
    // engine offers to play it and every sequence of choices it then asks
    // for, and collect the friendly units it actually chooses (the engine's
    // "chosen_by_controller" announcement). Card::mayChooseAtFinalization
    // must say Yes for exactly those. Two boards, so that every mode of the
    // modal spells is reached: the second adds Empowered units (Sanction's
    // "Disempower a unit that's [Empowered]"). The modes are checked by the
    // labels the cards give them.
    const std::map<CardDefId, std::vector<std::string>> modes = {
        {kRocketBarrage, {"Deal 4 to a unit in a base", "Kill a gear"}},
        {kCurtainCall, {"Draw 1", "Deal 2 (BF)", "Deal 3 (base)", "-4M (BF)"}},
        {839, {"Return a friendly unit to its owner's hand",                 // Mesmerize
               "Give an enemy unit -2 [M] this turn"}},
        {822, {"Empower a unit (disempower it at end of turn)",              // Sanction
               "Disempower an Empowered unit (empower it at end of turn)"}},
        {606, {"Counter a spell", "Play four Bird tokens"}},                 // Flurry of Feathers
        {665, {"Recycle up to 3 from opponent's trash", "Draw 1"}},          // Disposal Order
    };
    std::map<CardDefId, std::set<std::string>> labels_seen;
    const GameState blank = state;
    std::vector<CardDefId> spells;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (!card || def.card_type != CardType::Spell || id >= 1000) continue;
        if (!(card->choicesAtFinalization() & Card::kFinalizeResolve)) continue;
        if (card->needsPlayTimeTarget()) continue;
        spells.push_back(id);
    }
    std::sort(spells.begin(), spells.end());
    ASSERT_GE(spells.size(), 30u);

    int capped = 0;
    for (int board = 0; board < 2; ++board)
    for (CardDefId id : spells) {
        state = blank;
        // The board: friendly units in base, at two battlefields, one Mighty,
        // one equipped; enemy units beside them and in their base; gear on
        // both sides; cards in hand, trash and deck; XP and resources to pay
        // for anything.
        state.battlefields[0].controller = P1;
        state.battlefields[1].controller = P1;
        std::vector<std::pair<GameObjectId, std::string>> mine;
        mine.emplace_back(addUnit(P1, kInvalidId, 2), "in base");
        mine.emplace_back(addUnit(P1, kInvalidId, 3, 0), "at battlefield 0");
        mine.emplace_back(addUnit(P1, kInvalidId, 6, 0), "Mighty, at battlefield 0");
        const GameObjectId equipped = addUnit(P1, kInvalidId, 4, 1);
        mine.emplace_back(equipped, "equipped, at battlefield 1");
        const GameObjectId sword = addGear(P1, kLongSword);
        o(sword).attached_to = equipped;
        o(sword).location = o(equipped).location;
        o(sword).zone = o(equipped).zone;
        o(equipped).attachments.push_back(sword);
        addGear(P1, kLongSword);
        const GameObjectId enemy_here = addUnit(P2, kInvalidId, 2, 0);
        addUnit(P2, kInvalidId, 5, 1);
        addUnit(P2, kInvalidId, 3);
        addGear(P2);
        if (board == 1) {
            // Empowered units on both sides.
            o(mine[1].first).empowered = 1;
            mine[1].second += ", Empowered";
            o(enemy_here).empowered = 1;
        }
        addToHand(P1, kPoutyPoro);
        addToHand(P1, kPoutyPoro);
        addToTrash(P1, kPoutyPoro);
        addToTrash(P1, kCruelPatron);
        for (int i = 0; i < 10; ++i) addToDeck(P1, kPoutyPoro);
        setXp(P1, 10);
        const GameObjectId spell = addToHand(P1, id);
        setPool(P1, 30, 10);
        const GameState start = state;

        const Card* card = card_registry.get(id);
        std::set<GameObjectId> chosen;
        auto conn = events.on_object_state_changed.connect(
            [&](const ObjectStateChangedEvent& e) {
                if (e.what_changed == "chosen_by_controller") chosen.insert(e.object);
            });
        std::vector<Intent> plays;
        for (const auto& i : legal())
            if (i.card == spell) plays.push_back(i);
        state = start;
        // A spell that chooses a spell or ability on the chain (a counter,
        // CR 355.9.a.2) is playable only with one there: an enemy spell that
        // chooses the friendly unit at battlefield 0 (testing phase D21).
        GameState with_chain = start;
        // (Flurry of Feathers, modal, is playable without one: its "Counter a
        // spell" mode is reached on the second board, which has the spell.)
        if (plays.empty() || (id == 606 && board == 1)) {
            plays.clear();
            for (int i = 0; i < 4; ++i) addToDeck(P2, kPoutyPoro);
            const auto src = pushSpellOnChain(P2, 836);   // Dredge Up
            state.chain.items.back().targets = {mine[1].first};
            state.chain.items.back().chosen_objects = {mine[1].first};
            o(src).owner = P2;
            with_chain = state;
            for (const auto& i : legal())
                if (i.card == spell) plays.push_back(i);
        }
        const GameState start2 = with_chain;
        int runs = 0;
        constexpr int kMaxRuns = 40000;
        for (const auto& intent : plays) {
            std::vector<std::vector<int>> todo{{}};
            while (!todo.empty() && runs < kMaxRuns) {
                const std::vector<int> prefix = std::move(todo.back());
                todo.pop_back();
                state = start2;
                WalkAgent walker;
                walker.prefix = prefix;
                walker.labels = &labels_seen[id];
                ++runs;
                try {
                    withEngine([&](GameEngine& e) { e.testHook_executeIntent(intent); },
                               &walker, nullptr, P1);
                } catch (const std::exception& ex) {
                    ADD_FAILURE() << card_db.get(id).name << " (" << id << ") threw: " << ex.what();
                }
                for (size_t k = prefix.size(); k < walker.option_counts.size() && k < 8; ++k)
                    for (int alt = 1; alt < walker.option_counts[k]; ++alt) {
                        std::vector<int> next = prefix;
                        next.resize(k, 0);
                        next.push_back(alt);
                        todo.push_back(std::move(next));
                    }
            }
        }
        conn.disconnect();
        state = start2;   // the board it was offered on (a chain spell for a counter)
        const bool complete = runs < kMaxRuns;
        if (!complete) {
            ++capped;
            ADD_FAILURE() << card_db.get(id).name << " (" << id << "): the walk was cut off at "
                          << kMaxRuns << " runs";
        }
        for (const auto& [unit, where] : mine) {
            const bool yes = card->mayChooseAtFinalization(state, P1, unit) ==
                             Card::FinalizeChoice::Yes;
            const bool was = chosen.count(unit) > 0;
            // A walk that hit the cap may have missed a choice: then only
            // "chosen implies declared" is checked.
            if (was || complete) {
                EXPECT_EQ(yes, was) << card_db.get(id).name << " (" << id << "), board " << board
                                    << ": the unit " << where
                                    << (was ? " is chosen by it but declared No"
                                            : " is declared Yes but never chosen")
                                    << " (" << plays.size() << " offered plays, " << runs
                                    << " walks)";
            }
        }
        EXPECT_FALSE(plays.empty()) << card_db.get(id).name << " (" << id
                                    << ") is not playable on the test board";
    }
    EXPECT_EQ(capped, 0) << "every spell's choices were walked to the end";
    // Every mode of every modal spell among them was reached on some board.
    for (const auto& [id, labels] : modes) {
        ASSERT_TRUE(std::find(spells.begin(), spells.end(), id) != spells.end()) << id;
        for (const auto& label : labels)
            EXPECT_TRUE(labels_seen[id].count(label) > 0)
                << card_db.get(id).name << " (" << id << "): mode \"" << label
                << "\" was never offered on the test boards";
    }
    // And those six are all the modal ones: the spells among them whose
    // text says "Choose one".
    for (CardDefId id : spells)
        EXPECT_EQ(card_db.get(id).ability_text.find("Choose one") != std::string::npos,
                  modes.count(id) > 0)
            << card_db.get(id).name << " (" << id << ")";
    state = blank;
}

// ─── 9.II1: how near the registered spells come to the probe's bound ────────

TEST_F(SweepCostsTest, FinalizePassSpell_NoRegisteredSpellComesNearTheProbeBound) {
    // The probe walks at most 256 sequences of a spell's choices and reaching
    // that without an answer is an engine error. For each of the finalize-pass
    // spells: put it one Energy (or one Power) short with Irelia, Graceful in
    // play, with and without Sandswept Tomb as a battlefield (the two ways a
    // registered card takes something off the cost of a spell that chooses a
    // unit), in each place she can stand, on a small board and on one with
    // five times the units and two more battlefields, and count the runs of
    // every search the engine makes to decide the offer. The most any spell
    // needs is three (Curtain Call: its modes). They must stay within
    // kMostRuns, and must not
    // grow with the number of units: the choice filter leaves, at the first
    // prompt for an object, only the objects that pay for the spell, and any
    // sequence that chose one is payable — so what a search can walk without
    // finding an answer are the sequences of a spell's modes, "may"s and
    // battlefields alone, which the card's text bounds.
    constexpr int kMostRuns = 4;
    const GameState blank = state;
    std::vector<CardDefId> spells;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (!card || def.card_type != CardType::Spell || id >= 1000) continue;
        if (!(card->choicesAtFinalization() & Card::kFinalizeResolve)) continue;
        if (card->needsPlayTimeTarget()) continue;
        spells.push_back(id);
    }
    std::sort(spells.begin(), spells.end());
    ASSERT_GE(spells.size(), 30u);
    int probed_spells = 0, most = 0;
    long searches = 0;
    for (CardDefId id : spells) {
        int most_small = 0, most_large = 0;
        bool probed = false;
        for (int size = 0; size < 2; ++size)
        for (int tomb = 0; tomb < 2; ++tomb)
        for (int irelia_at = -1; irelia_at < 2; ++irelia_at)
        for (int short_of = 0; short_of < 2; ++short_of) {
            state = blank;
            if (tomb == 1) {
                // Sandswept Tomb as battlefield 0: every friendly unit there
                // takes [A] off a spell that chooses it.
                const GameObjectId card = state.createObject();
                o(card).card_def_id = 951;
                o(card).card_type = CardType::Battlefield;
                o(card).name = card_db.get(951).name;
                state.battlefields[0].card_object_id = card;
            }
            const int copies = size == 0 ? 1 : 5;
            if (size == 1) { addBattlefield(); addBattlefield(); }
            for (auto& bf : state.battlefields) bf.controller = P1;
            for (int c = 0; c < copies; ++c) {
                addUnit(P1, kInvalidId, 2);
                addUnit(P1, kInvalidId, 3, 0);
                addUnit(P1, kInvalidId, 6, 1);
                addUnit(P2, kInvalidId, 2, 0);
                addUnit(P2, kInvalidId, 5, 1);
                addUnit(P2, kInvalidId, 3);
                addGear(P1, kLongSword);
                addGear(P2);
                addToHand(P1, kPoutyPoro);
                addToTrash(P1, kPoutyPoro);
            }
            addUnit(P1, kIreliaGraceful, 3, irelia_at);
            for (int i = 0; i < 10; ++i) addToDeck(P1, kPoutyPoro);
            setXp(P1, 10);
            withEngine([](GameEngine& e) { e.testHook_cleanup(); });
            const GameObjectId spell = addToHand(P1, id);
            const auto& def = card_db.get(id);
            // One short: of Energy, or of Power where the spell has a Power cost.
            if (short_of == 0) setPool(P1, std::max(0, def.energy_cost - 1), 10);
            else setPool(P1, 30, std::max(0, def.power_cost - 1));
            withEngine([&](GameEngine& e) {
                (void)e.generateLegalActions();
                if (e.testHook_prePlayProbes() == 0) return;
                probed = true;
                searches += e.testHook_prePlayProbes();
                int& m = size == 0 ? most_small : most_large;
                m = std::max(m, e.testHook_prePlayRunsMax());
            });
            (void)spell;
        }
        if (probed) ++probed_spells;
        most = std::max({most, most_small, most_large});
        EXPECT_LE(most_large, kMostRuns) << card_db.get(id).name << " (" << id << ")";
        // More units and more battlefields: not one more run. (A search
        // tries the answers that take something off the cost first, so it
        // does not walk the other units a prompt offers.)
        EXPECT_LE(most_large, most_small)
            << card_db.get(id).name << " (" << id << "): the runs of a search grew with the board ("
            << most_small << " -> " << most_large << ")";
        if (probed)
            std::cout << "[ " << card_db.get(id).name << " (" << id << "): at most " << most_small
                      << " run(s) on the small board, " << most_large << " on the large one ]\n";
    }
    EXPECT_GT(probed_spells, 10) << "the boards did put the spells one short with a discount in reach";
    EXPECT_GT(searches, 0);
    EXPECT_LE(most, kMostRuns);
    state = blank;
}

// ─── 9.DD2: what "the state changed" digests ────────────────────────────────

TEST_F(SweepCostsTest, CostFingerprint_SeesAUnitMoveBetweenLocations) {
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    const uint64_t before = Card::costStateFingerprint(state);
    o(unit).location = BattlefieldLocation{1};       // still "on the board"
    EXPECT_NE(Card::costStateFingerprint(state), before);
    o(unit).location = BattlefieldLocation{0};
    EXPECT_EQ(Card::costStateFingerprint(state), before);
    o(unit).is_stunned = true;
    EXPECT_NE(Card::costStateFingerprint(state), before);
    o(unit).is_stunned = false;
    o(unit).controller = P2;
    EXPECT_NE(Card::costStateFingerprint(state), before);
}

TEST_F(SweepCostsTest, CostFingerprint_SeesEachPoolAndTheChampionZone) {
    const uint64_t before = Card::costStateFingerprint(state);
    // One Power moved from one Domain to another: the total is the same.
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    const uint64_t fury = Card::costStateFingerprint(state);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 0;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
    EXPECT_NE(Card::costStateFingerprint(state), fury);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 0;
    state.player(P1).rune_pool.spell_power = 1;
    const uint64_t spell = Card::costStateFingerprint(state);
    state.player(P1).rune_pool.spell_power = 0;
    state.player(P1).rune_pool.gear_power = 1;
    EXPECT_NE(Card::costStateFingerprint(state), spell);
    state.player(P1).rune_pool.gear_power = 0;
    ASSERT_EQ(Card::costStateFingerprint(state), before);
    state.player(P1).champion_zone = GameObjectId::fromRaw(7);
    EXPECT_NE(Card::costStateFingerprint(state), before);
}

TEST_F(SweepCostsTest, CostFingerprint_SeesABattlefieldsFacedownCardsAndController) {
    const uint64_t before = Card::costStateFingerprint(state);
    state.battlefields[0].facedown.push_back(GameObjectId::fromRaw(42));
    EXPECT_NE(Card::costStateFingerprint(state), before);
    state.battlefields[0].facedown.clear();
    state.battlefields[0].controller = P1;
    EXPECT_NE(Card::costStateFingerprint(state), before);
}

// NOT digested, on purpose: an object's card_counters. Cards and the choice
// helpers write progress there while choices are made; a digest that moved
// with them would call bookkeeping a payment (the cost contract, 9.J2).
TEST_F(SweepCostsTest, CostFingerprint_DoesNotMoveWithACardsOwnCounters) {
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    const uint64_t before = Card::costStateFingerprint(state);
    o(unit).card_counters["__some_progress"] = 3;
    EXPECT_EQ(Card::costStateFingerprint(state), before);
}

// ─── 9.GG2: "where to play it" is settled at finalization even with no choice ─

/// The opponent, the first time they get priority, hands battlefield 0 to P1:
/// "a battlefield is controlled by then".
class GivesABattlefieldAgent : public AgentInterface {
public:
    GameEngine* engine = nullptr;
    int given = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (given == 0 && engine) {
            engine->mutableState().battlefields[0].controller = PlayerId::Player1;
            ++given;
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

TEST_F(SweepCostsTest, EffectPlayLocation_FlameChompersDoesNotAskAtResolution) {
    auto chompers = addToTrash(P1, kFlameChompers);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));   // pay [R] to play me: yes
    GivesABattlefieldAgent them;
    withEngine([&](GameEngine& e) {
        them.engine = &e;
        e.testHook_executor().queueTriggeredAbility(chompers, P1, TriggerType::WhenIAmDiscarded);
        e.testHook_runChain();
    }, &agent, &them, P1);
    ASSERT_EQ(them.given, 1);
    EXPECT_EQ(agent.prompts.size(), 1u) << "no location prompt as the ability resolves";
    EXPECT_TRUE(o(chompers).isAtBase()) << "the base, settled when it was finalized";
}

TEST_F(SweepCostsTest, EffectPlayLocation_ImmortalPhoenixDoesNotAskAtResolution) {
    auto phoenix = addToTrash(P1, kImmortalPhoenix);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    GivesABattlefieldAgent them;
    withEngine([&](GameEngine& e) {
        them.engine = &e;
        e.testHook_executor().queueTriggeredAbility(phoenix, P1,
                                                    TriggerType::WhenYouKillAUnitWithASpell);
        e.testHook_runChain();
    }, &agent, &them, P1);
    ASSERT_EQ(them.given, 1);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(o(phoenix).isAtBase());
}

TEST_F(SweepCostsTest, EffectPlayLocation_SoulgorgerDoesNotAskAtResolution) {
    auto dead = addToTrash(P1, kFirstMate);
    auto card = addToHand(P1, kSoulgorger);
    const auto& def = card_db.get(kSoulgorger);
    setPool(P1, def.energy_cost, def.power_cost);
    // Yes; the unit; then the revived First Mate's own trigger (whatever it asks).
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(dead.raw()), EnginePathAgent::kRest}));
    GivesABattlefieldAgent them;
    Intent play_it;
    play_it.type = IntentType::PlayCard;
    play_it.player = P1;
    play_it.card = card;
    play_it.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) {
        them.engine = &e;
        e.testHook_executeIntent(play_it);
    }, &agent, &them, P1);
    ASSERT_EQ(them.given, 1);
    EXPECT_TRUE(o(dead).isAtBase()) << "the base, settled when the ability was finalized";
    for (const auto& prompt : agent.prompts)
        for (const auto& option : prompt)
            EXPECT_TRUE(option.choice_label.rfind("Battlefield #", 0) != 0)
                << "a location was asked for as the ability resolved";
}

// ─── 9.EE1 / 9.EE2: the two card contracts the engine checks as it runs ─────

TEST_F(SweepCostsTest, CardContract_AnOptionWithNeitherValueNorObjectIsAnError) {
    // A prompt option an agent could tell from the others only by its
    // position: the engine refuses to ask, naming the card.
    auto spell = addToHand(P1, kTUnnamedOption);
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = P1;
    i.card = spell;
    try {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); });
        ADD_FAILURE() << "the engine put an unnameable option to the agent";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("Test Unnamed Option"), std::string::npos)
            << ex.what();
        EXPECT_NE(std::string(ex.what()).find("neither a value nor an object"), std::string::npos);
    }
}

TEST_F(SweepCostsTest, CardContract_AFinalizePassThatEndsAnywhereElseIsAnError) {
    // The rule (Card::finalizingChoices): a finalize pass ends at the marker,
    // or where a choice helper stored "nothing to decide".
    auto spell = addToHand(P1, kTNoMarker);
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = P1;
    i.card = spell;
    try {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); });
        ADD_FAILURE() << "the engine accepted a pass that returned for a reason of its own";
    } catch (const std::logic_error& ex) {
        EXPECT_NE(std::string(ex.what()).find("Test No Marker"), std::string::npos) << ex.what();
        EXPECT_NE(std::string(ex.what()).find("end-of-choices marker"), std::string::npos);
    }
}

TEST_F(SweepCostsTest, CardContract_APassMayEndAtTheMarkerOrAtADeclinedMay) {
    // At the marker.
    auto fine = addToHand(P1, kTFalseDeclaration);
    setPool(P1, 2, 0);
    EXPECT_NO_THROW(play(fine));
    // At a "may" the player declined: the helper stored the answer, and the
    // card is not asked again as it resolves.
    auto may = addToHand(P1, kTDeclinedMay);
    EnginePathAgent no(EnginePathAgent::script({0}));
    EXPECT_NO_THROW(play(may, &no));
    EXPECT_EQ(no.prompts.size(), 1u) << "asked once, at finalization";
    EXPECT_EQ(state.player(P1).xp, 0) << "declined: no effect";
    auto may2 = addToHand(P1, kTDeclinedMay);
    EnginePathAgent yes(EnginePathAgent::script({1}));
    EXPECT_NO_THROW(play(may2, &yes));
    EXPECT_EQ(state.player(P1).xp, 1);
}

/// Passes priority; answers a card's prompt with its first or its last option.
class EndAgent : public AgentInterface {
public:
    bool last = false;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool is_choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (!is_choice) {
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
            return legal.front();
        }
        return last ? legal.back() : legal.front();
    }
};

TEST_F(SweepCostsTest, CardContract_NoRegisteredCardEndsItsFinalizePassAnywhereElse) {
    // Every registered card that opts a callback into choices at finalization
    // is run through the engine: a spell is played every way it is offered,
    // each trigger type the card fires on is queued (with no subject, a
    // friendly unit and an enemy unit as the event's object), each activated
    // ability it is offered is activated — alone on the board and on a full
    // one, answering every prompt with the first option and with the last.
    // The engine throws when a finalize pass ends anywhere but at the marker
    // or at a helper's stored "nothing to decide" (Card::finalizingChoices).
    const GameState blank = state;
    std::vector<CardDefId> ids;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (card && id < 1000 && card->choicesAtFinalization() != 0) ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_GE(ids.size(), 200u);

    std::vector<std::string> trips, other;
    int spell_passes = 0, trigger_passes = 0, activate_passes = 0, equipped_passes = 0;
    for (CardDefId id : ids) {
        const Card* card = card_registry.get(id);
        const CardDef& def = card_db.get(id);
        const unsigned flags = card->choicesAtFinalization();
        GameObjectId friendly = kInvalidId, enemy = kInvalidId;
        // The card where it works from; on board 1 with company.
        auto setUp = [&](int board) -> GameObjectId {
            state = blank;
            state.battlefields[0].controller = P1;
            GameObjectId source = kInvalidId;
            switch (def.card_type) {
                case CardType::Spell: source = addToHand(P1, id); break;
                case CardType::Gear: source = addGear(P1, id); break;
                case CardType::Legend: source = addLegend(P1, id); break;
                case CardType::Battlefield: {
                    source = state.createObject();
                    auto& b = state.getObject(source);
                    b.owner = b.controller = P1;
                    b.card_def_id = id;
                    b.name = def.name;
                    b.card_type = CardType::Battlefield;
                    state.battlefields[0].card_object_id = source;
                    break;
                }
                default: source = addUnit(P1, id, std::max(def.might, 1), 0); break;
            }
            friendly = enemy = kInvalidId;
            if (board == 1) {
                state.battlefields[1].controller = P1;
                friendly = addUnit(P1, kInvalidId, 3, 0);
                addUnit(P1, kInvalidId, 6, 1);
                addUnit(P1, kInvalidId, 2);
                addGear(P1, kLongSword);
                enemy = addUnit(P2, kInvalidId, 2, 0);
                addUnit(P2, kInvalidId, 5, 1);
                addUnit(P2, kInvalidId, 3);
                addGear(P2);
                addToHand(P1, kPoutyPoro);
                addToHand(P1, kPoutyPoro);
                addToTrash(P1, kPoutyPoro);
                for (int i = 0; i < 10; ++i) addToDeck(P1, kPoutyPoro);
                for (int i = 0; i < 4; ++i) addRune(P1, Domain::Fury);
                setXp(P1, 10);
            }
            setPool(P1, 30, 10);
            return source;
        };
        auto guarded = [&](const std::string& what, const std::function<void()>& body) {
            try {
                body();
            } catch (const std::logic_error& ex) {
                if (std::string(ex.what()).find("end-of-choices marker") != std::string::npos)
                    trips.push_back(def.name + " (" + toString(id) + "), " + what);
            } catch (const std::exception& ex) {
                other.push_back(def.name + " (" + toString(id) + "), " + what + ": " +
                                ex.what());
            }
        };
        for (int board = 0; board < 2; ++board)
        for (int last = 0; last < 2; ++last) {
            EndAgent a1, a2;
            a1.last = a2.last = last == 1;
            const std::string where = std::string(board ? "full board" : "alone") +
                                      (last ? ", last option" : ", first option");
            if ((flags & (Card::kFinalizeResolve | Card::kFinalizeActivate)) != 0) {
                const GameObjectId source = setUp(board);
                std::vector<Intent> mine;
                guarded("offer, " + where, [&] {
                    for (const auto& i : legal())
                        if (def.card_type == CardType::Spell
                                ? i.card == source
                                : i.ability_source == source &&
                                  (i.type == IntentType::ActivateAbility ||
                                   i.type == IntentType::ActivateReactionAbility ||
                                   i.type == IntentType::ActivateActionAbility))
                            mine.push_back(i);
                });
                for (const auto& intent : mine) {
                    ASSERT_EQ(setUp(board), source);
                    guarded("intent, " + where, [&] {
                        withEngine([&](GameEngine& e) {
                            e.testHook_executeIntent(intent);
                            settleLikeTheEngine(e);
                        }, &a1, &a2, P1);
                    });
                    ++(def.card_type == CardType::Spell ? spell_passes : activate_passes);
                }
            }
            if ((flags & Card::kFinalizeTrigger) != 0) {
                for (TriggerType t : card->triggerTypes()) {
                    if (t == TriggerType::None) continue;
                    for (int subject = 0; subject < (board ? 3 : 1); ++subject) {
                        const GameObjectId source = setUp(board);
                        const GameObjectId about =
                            subject == 0 ? kInvalidId : subject == 1 ? friendly : enemy;
                        guarded("trigger " + std::to_string(static_cast<int>(t)) + ", " + where,
                                [&] { fireTriggerThroughEngine(source, t, &a1, about, &a2); });
                        ++trigger_passes;
                    }
                }
                // An Equipment's ability that speaks for the unit it is on
                // (Card::onEquippedTrigger), queued as TriggerManager::
                // queueEquippedTrigger does.
                const TriggerType worn = card->equippedTriggerType();
                if (def.card_type == CardType::Gear && worn != TriggerType::None && board == 1) {
                    const GameObjectId source = setUp(board);
                    o(source).attached_to = friendly;
                    o(source).location = o(friendly).location;
                    o(source).zone = o(friendly).zone;
                    o(friendly).attachments.push_back(source);
                    const GameObjectId bearer = friendly;
                    guarded("equipped trigger, " + where, [&] {
                        withEngine([&](GameEngine& e) {
                            e.testHook_executor().queueTriggeredAbility(source, P1, worn, bearer);
                            ASSERT_FALSE(e.mutableState().chain.items.empty());
                            e.mutableState().chain.items.back().equipped_unit = bearer;
                            settleLikeTheEngine(e);
                        }, &a1, &a2, P1);
                    });
                    ++equipped_passes;
                }
            }
        }
    }
    state = blank;
    std::sort(trips.begin(), trips.end());
    trips.erase(std::unique(trips.begin(), trips.end()), trips.end());
    std::string all;
    for (const auto& s : trips) all += "\n  " + s;
    EXPECT_TRUE(trips.empty()) << trips.size() << " finalize passes ended elsewhere:" << all;
    EXPECT_GE(spell_passes, 60);
    EXPECT_GE(trigger_passes, 600);
    EXPECT_GE(activate_passes, 10);
    EXPECT_GE(equipped_passes, 4);
    // Nothing else went wrong either: no card threw for any other reason.
    std::string rest;
    for (const auto& s : other) rest += "\n  " + s;
    EXPECT_TRUE(other.empty()) << other.size() << " runs threw something else:" << rest;
    std::cout << "[ " << ids.size() << " cards: " << spell_passes << " spell plays, "
              << trigger_passes << " triggers, " << equipped_passes << " equipped triggers, "
              << activate_passes << " activations ]\n";
}

// ─── Finding F2: a finalize-pass spell that is not played from a hand ───────
//
// Alpha Strike 751 read targets[0] and returned when there was none, before
// its end-of-choices marker: fine from hand (the play intent carries the
// target), an engine error when Blind Fury or Promising Future played it, or
// when Mystic Reversal made it choose again. The sweep above plays every
// spell the way the generator offers it, so it never saw that. This one puts
// every finalize-pass spell on the chain the two other ways: played by an
// effect (EffectExecutor::playIgnoringCost), and told to make new choices
// while it is on the chain (EffectExecutor::makeNewChoices, CR 750-755).
TEST_F(SweepCostsTest, CardContract_NoFinalizePassSpellTripsWhenPlayedByAnEffectOrChoosingAgain) {
    const GameState blank = state;
    std::vector<CardDefId> ids;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (card && id < 1000 && def.card_type == CardType::Spell &&
            (card->choicesAtFinalization() & Card::kFinalizeResolve))
            ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_GE(ids.size(), 30u);

    std::vector<std::string> trips;
    int effect_plays = 0, rechoices = 0;
    for (CardDefId id : ids) {
        const CardDef& def = card_db.get(id);
        auto setUp = [&](int board) -> GameObjectId {
            state = blank;
            state.battlefields[0].controller = P1;
            addUnit(P1, kInvalidId, 4, 0);
            const GameObjectId source = addToHand(P1, id);
            if (board == 1) {
                state.battlefields[1].controller = P2;
                addUnit(P1, kInvalidId, 6, 1);
                addUnit(P1, kInvalidId, 2);
                addGear(P1, kLongSword);
                addUnit(P2, kInvalidId, 2, 0);
                addUnit(P2, kInvalidId, 5, 1);
                addUnit(P2, kInvalidId, 3);
                addGear(P2);
                addToHand(P1, kPoutyPoro);
                addToTrash(P1, kPoutyPoro);
                for (int i = 0; i < 4; ++i) addRune(P1, Domain::Fury);
                setXp(P1, 10);
            }
            for (int i = 0; i < 10; ++i) addToDeck(P1, kPoutyPoro);
            for (int i = 0; i < 10; ++i) addToDeck(P2, kPoutyPoro);
            setPool(P1, 30, 10);
            return source;
        };
        auto guarded = [&](const std::string& what, const std::function<void()>& body) {
            try {
                body();
            } catch (const std::exception& ex) {
                trips.push_back(def.name + " (" + toString(id) + "), " + what + ": " + ex.what());
            }
        };
        for (int board = 0; board < 2; ++board)
        for (int last = 0; last < 2; ++last) {
            const std::string where = std::string(board ? "full board" : "small board") +
                                      (last ? ", last option" : ", first option");
            {   // Played by an effect.
                const GameObjectId source = setUp(board);
                EndAgent a1, a2;
                a1.last = a2.last = last == 1;
                guarded("played by an effect, " + where, [&] {
                    withEngine([&](GameEngine& e) {
                        e.testHook_executor().playIgnoringCost(P1, source);
                        settleLikeTheEngine(e);
                    }, &a1, &a2, P1);
                });
                ++effect_plays;
            }
            {   // Played from hand, then told to choose again while on the chain.
                const GameObjectId source = setUp(board);
                std::vector<Intent> mine;
                guarded("offer, " + where, [&] {
                    for (const auto& i : legal())
                        if (i.card == source) mine.push_back(i);
                });
                if (mine.empty()) continue;
                const Intent intent = last ? mine.back() : mine.front();
                ASSERT_EQ(setUp(board), source);
                EndAgent a1;
                a1.last = last == 1;
                GameEngine* live = nullptr;
                bool again = false;
                PickByPredicateAgent a2([&](int, const std::vector<Intent>& options) {
                    if (!again && live)
                        for (const auto& ci : live->mutableState().chain.items)
                            if (ci.source == source && ci.is_spell) {
                                again = true;
                                const ChainItemId item = ci.id;
                                live->testHook_executor().makeNewChoices(item);
                                break;
                            }
                    for (const auto& i : options)
                        if (i.type == IntentType::PassPriority) return i;
                    return options.front();
                });
                guarded("choosing again, " + where, [&] {
                    withEngine([&](GameEngine& e) {
                        live = &e;
                        e.testHook_executeIntent(intent);
                        settleLikeTheEngine(e);
                        live = nullptr;
                    }, &a1, &a2, P1);
                });
                if (again) ++rechoices;
            }
        }
    }
    state = blank;
    std::sort(trips.begin(), trips.end());
    trips.erase(std::unique(trips.begin(), trips.end()), trips.end());
    std::string all;
    for (const auto& t : trips) all += "\n  " + t;
    EXPECT_TRUE(trips.empty()) << trips.size() << " run(s) threw:" << all;
    EXPECT_GE(effect_plays, 120);
    EXPECT_GE(rechoices, 40);
    std::cout << "[ " << ids.size() << " finalize-pass spells: " << effect_plays
              << " effect plays, " << rechoices << " re-choices ]\n";
}

// ─── Finding D4: CR 403 removal against what the card would have done ───────
//
// A triggered ability that is not performed leaves the chain as it would have
// been finalized (GameEngine::runFinalizePass): its leading "may" was declined
// or could not be paid for (CR 403.1.a, 405.2), or a required choice had no
// legal option (CR 403.4). The engine decides that from what the choice
// helpers stored. This sweep runs every opted-in card's triggers twice from
// the same board — once as the engine now plays it, once with the removal
// switched off, so the item resolves as it used to — and lists every case in
// which the two games end differently. Each such case is an ability that would
// still have DONE something:
//   - after a declined "may": the card used confirmOptional for a "may" that
//     is not the first part of its effect (CR 383.3.a.3). It must say so
//     (Card::mayIsNotTheWholeAbility) and is then not removed;
//   - after a missing required choice: CR 403.4 removes the whole ability,
//     other instructions included. Those cards are listed below by hand, each
//     with its text; they are written up for the judges
//     (docs/judge-questions-2026-10.md) because the rule reads strictly.
TEST_F(SweepCostsTest, Removal_NoRegisteredTriggerLosesAnEffectExceptTheListedOnes) {
    const GameState blank = state;
    std::vector<CardDefId> ids;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (card && id < 1000 && (card->choicesAtFinalization() & Card::kFinalizeTrigger))
            ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_GE(ids.size(), 150u);

    auto digest = [&](const GameState& st) {
        uint64_t h = Card::costStateFingerprint(st);
        auto mix = [&](uint64_t x) { h = (h ^ x) * 0x100000001B3ULL; };
        for (PlayerId p : {P1, P2}) {
            mix(static_cast<uint64_t>(st.player(p).score));
            mix(st.player(p).hand.size());
            mix(st.player(p).main_deck.size());
            mix(st.player(p).trash.size());
            mix(st.player(p).banishment.size());
            for (auto c : st.player(p).main_deck) mix(static_cast<uint64_t>(c.raw()));
            for (auto c : st.player(p).hand) mix(static_cast<uint64_t>(c.raw()));
            mix(static_cast<uint64_t>(st.player(p).xp));
        }
        std::vector<GameObjectId> all;
        for (const auto& [oid, obj] : st.objects) all.push_back(oid);
        std::sort(all.begin(), all.end());
        for (auto oid : all) {
            const auto& obj = st.getObject(oid);
            mix(static_cast<uint64_t>(oid.raw()) * 31 + (obj.is_stunned ? 1 : 0) +
                (obj.location.has_value() ? 2 : 0) + (obj.isAtBattlefield() ? 4 : 0) +
                static_cast<uint64_t>(static_cast<uint32_t>(obj.current_might)) * 8 +
                static_cast<uint64_t>(static_cast<uint32_t>(obj.empowered)) * 4096 +
                static_cast<uint64_t>(obj.controller) * 65536 +
                static_cast<uint64_t>(static_cast<uint32_t>(obj.temp_might_bonus)) * 1048576);
            mix(obj.attached_to.has_value() ? static_cast<uint64_t>(obj.attached_to->raw()) + 1 : 0);
            mix(obj.keywords.bits);
        }
        mix(st.delayed_abilities.size());
        return h;
    };

    std::set<std::string> differs;
    int runs = 0, removed_runs = 0;
    for (CardDefId id : ids) {
        const Card* card = card_registry.get(id);
        const CardDef& def = card_db.get(id);
        GameObjectId friendly = kInvalidId, enemy = kInvalidId;
        auto setUp = [&](int board) -> GameObjectId {
            state = blank;
            state.battlefields[0].controller = P1;
            GameObjectId source = kInvalidId;
            switch (def.card_type) {
                case CardType::Spell: source = addToTrash(P1, id); break;
                case CardType::Gear: source = addGear(P1, id); break;
                case CardType::Legend: source = addLegend(P1, id); break;
                case CardType::Battlefield: {
                    source = state.createObject();
                    auto& b = state.getObject(source);
                    b.owner = b.controller = P1;
                    b.card_def_id = id;
                    b.name = def.name;
                    b.card_type = CardType::Battlefield;
                    state.battlefields[0].card_object_id = source;
                    break;
                }
                default: source = addUnit(P1, id, std::max(def.might, 1), 0); break;
            }
            friendly = enemy = kInvalidId;
            if (board == 1) {
                state.battlefields[1].controller = P1;
                friendly = addUnit(P1, kInvalidId, 3, 0);
                o(friendly).damage_marked = 1;
                addUnit(P1, kInvalidId, 6, 1);
                addUnit(P1, kInvalidId, 2);
                addGear(P1, kLongSword);
                enemy = addUnit(P2, kInvalidId, 2, 0);
                addUnit(P2, kInvalidId, 5, 1);
                addUnit(P2, kInvalidId, 3);
                addGear(P2);
                addToHand(P1, kPoutyPoro);
                addToTrash(P1, kPoutyPoro);
                for (int i = 0; i < 4; ++i) addRune(P1, Domain::Fury);
                setXp(P1, 10);
            }
            // Both boards: a deck to draw or reveal from, a hurt unit to heal.
            for (int i = 0; i < 10; ++i) addToDeck(P1, kPoutyPoro);
            o(addUnit(P1, kInvalidId, 4, 0)).damage_marked = 1;
            setPool(P1, 30, 10);
            return source;
        };
        for (int board = 0; board < 2; ++board)
        for (int last = 0; last < 2; ++last)
        for (TriggerType t : card->triggerTypes()) {
            if (t == TriggerType::None) continue;
            for (int subject = 0; subject < (board ? 3 : 1); ++subject) {
                uint64_t ended[2] = {0, 0};
                int removed = 0;
                bool threw = false;
                for (int keep = 0; keep < 2 && !threw; ++keep) {
                    const GameObjectId source = setUp(board);
                    const GameObjectId about =
                        subject == 0 ? kInvalidId : subject == 1 ? friendly : enemy;
                    EndAgent a1, a2;
                    a1.last = a2.last = last == 1;
                    int finalized = 0, queued = 0;
                    auto conn = events.on_chain_item_finalized.connect(
                        [&](const ChainItemFinalizedEvent&) { ++finalized; });
                    try {
                        withEngine([&](GameEngine& e) {
                            e.testHook_keepUnperformedTriggers(keep == 1);
                            const auto before = e.mutableState().chain.next_id;
                            e.testHook_executor().queueTriggeredAbility(source, P1, t, about);
                            queued = e.mutableState().chain.next_id.raw() - before.raw();
                            settleLikeTheEngine(e);
                        }, &a1, &a2, P1);
                    } catch (const std::exception&) {
                        threw = true;   // the other sweep reports card errors
                    }
                    conn.disconnect();
                    if (keep == 0 && queued > 0 && finalized == 0) removed = 1;
                    ended[keep] = digest(state);
                }
                if (threw) continue;
                ++runs;
                removed_runs += removed;
                if (ended[0] != ended[1])
                    differs.insert(def.name + " (" + toString(id) + ")");
            }
        }
    }
    state = blank;
    // CR 403.4, strict: these abilities have a required choice AND an
    // instruction that does not depend on it. With no legal option the whole
    // ability is removed, so that instruction is lost too. Reviewed by hand;
    // each is pinned by a test of its own and written up for the judges.
    const std::set<std::string> listed = {
        // "When I defend, choose an enemy unit here and reveal the top 5
        // cards of your Main Deck. Deal 1 to that unit for each card with
        // [Hidden] revealed this way, then recycle the revealed cards."
        // No enemy unit he can choose: no reveal, no recycle.
        "Teemo, Strategist (121)",
    };
    std::string extra, gone;
    for (const auto& d : differs) if (!listed.count(d)) extra += "\n  " + d;
    for (const auto& l : listed) if (!differs.count(l)) gone += "\n  " + l;
    EXPECT_TRUE(extra.empty()) << "removal changes the outcome for cards not on the list:" << extra;
    EXPECT_TRUE(gone.empty()) << "listed, but removal no longer changes anything:" << gone;
    EXPECT_GE(runs, 600);
    EXPECT_GE(removed_runs, 100);
    std::cout << "[ " << ids.size() << " cards, " << runs << " runs, " << removed_runs
              << " with the ability removed at finalization, " << differs.size()
              << " card(s) whose outcome it changes ]\n";
}

}  // namespace riftbound::test::sweep_costs
