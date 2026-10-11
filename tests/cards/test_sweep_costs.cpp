// Stage 3 sweep, part 1: costs, payment and play paths.
//
// Every test goes through the real engine (legal-action generation and
// executeIntent), because what changed is where the engine asks for and
// charges a cost: the Equip cost a gear declares, Weaponmaster paying it,
// additional costs chosen before the card's cost is paid, card-declared
// Repeat costs, restricted resource pools, the Hide cost and play-from-trash.

#include "card_test_fixture.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
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
constexpr CardDefId kTCostThenPrompt = 1052;       // spell: spend 1 XP, then choose a unit
constexpr CardDefId kTCostThenPromptFinal = 1053;  // the same, choices at finalization
constexpr CardDefId kTPlaysFromHand = 1054;        // spell: play a card from hand, ignoring its cost
constexpr CardDefId kTPowerTouch = 1055;           // spell costing one Fury Power: buff a unit
constexpr CardDefId kTCounter = 1056;              // [Reaction] spell: counter the item under it
constexpr CardDefId kRisenAltar = 950;
constexpr CardDefId kSkyCrusier = 847;
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
        o(card).hidden_at = static_cast<BattlefieldId>(bf);
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
    EnginePathAgent yes(EnginePathAgent::script({1, static_cast<int>(mine),
                                                 static_cast<int>(theirs)}));
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
    EnginePathAgent yes(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    play(bard2, &yes);
    EXPECT_TRUE(onBoard(bard2));
    EXPECT_TRUE(o(legend).is_exhausted) << "paid during the play";

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
        {static_cast<int>(c), static_cast<int>(a), 0}));
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
    EnginePathAgent no(EnginePathAgent::script({0, static_cast<int>(unit)}));
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
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(poro)}));
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
        {0, 1, 1, static_cast<int>(at_bf)}));
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
    EnginePathAgent plain(EnginePathAgent::script({static_cast<int>(enemy)}));
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
    hide.chosen_battlefield = 0;
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
        {0, static_cast<int>(r3), static_cast<int>(r2), 0}));   // mode; two runes; "None"
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
    addRune(P1, Domain::Mind);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(one), EnginePathAgent::kRest}));   // the target; then rune picks
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
    EXPECT_EQ(state.player(P1).power_spent_this_turn, 1);
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
    o(spell).hidden_at = 0;
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
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(irelia)}));
    play(spell, &agent);
    EXPECT_TRUE(agent.wasOffered(irelia));
    EXPECT_FALSE(agent.wasOffered(other)) << "[2] is out of reach for any other unit";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(o(irelia).battlefieldId().value_or(99), 0u) << "it was played and resolved";
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
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(other)}));
    play(spell, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(other).battlefieldId().value_or(99), 0u);
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
    addRune(P1, Domain::Mind);
    EnginePathAgent short_agent(EnginePathAgent::script(
        {0, static_cast<int>(plain), EnginePathAgent::kRest}));   // mode, unit; then rune picks
    play(spell, &short_agent);
    EXPECT_TRUE(short_agent.wasOffered(plain));
    EXPECT_FALSE(short_agent.wasOffered(guarded)) << "[4][B] + its Deflect is more than there is";
    EXPECT_EQ(o(plain).damage_marked, 4);
    EXPECT_TRUE(inTrash(P1, spell)) << "the spell was paid for and resolved";

    // With one more Power the guarded unit is offered, and everything is spent.
    auto again = addToHand(P1, kRocketBarrage);
    setPool(P1, 4, 1);
    auto rune = addRune(P1, Domain::Mind);
    EnginePathAgent agent(EnginePathAgent::script(
        {0, static_cast<int>(guarded), EnginePathAgent::kRest}));
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
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(gear2), static_cast<int>(enemy)}));
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
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(spell2)}));
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
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(jax, TriggerType::WhenEquipmentAttachedToMe, &yes);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(o(first).is_exhausted) << "the lowest-id rune in the base";
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
    // Pay; which gear; then the play trigger's own "kill a gear" pick (the
    // trigger is countered before it resolves).
    EnginePathAgent pays(EnginePathAgent::script(
        {1, static_cast<int>(mine), EnginePathAgent::kRest}));
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
        {static_cast<int>(irelia), static_cast<int>(enemy), EnginePathAgent::kRest}));
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
        {2, static_cast<int>(irelia), EnginePathAgent::kRest}));
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

// ─── 9.AA2: an attempt that cannot be paid for leaves nothing behind ────────

TEST_F(SweepCostsTest, FinalizePassSpell_AnAttemptThatEndsUnpayableIsUndoneCompletely) {
    // Resonating Strike with [1] and one Power is playable only by choosing
    // Irelia. The player picks the battlefield she stands at, so she is no
    // longer "at a different location" and nothing chosen earns the discount.
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    auto irelia = addUnit(P1, kIreliaGraceful, 3, 0);
    auto holder = addUnit(P1, kInvalidId, 2, 1);
    withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    auto spell = addToHand(P1, kResonatingStrike);
    setPool(P1, 1, 1);
    ASSERT_TRUE(playOffered(spell)) << "battlefield 1, then Irelia, would pay for it";
    const int chain_ids_before = static_cast<int>(state.chain.next_id);
    EnginePathAgent agent(EnginePathAgent::script({0, EnginePathAgent::kRest}));   // battlefield 0
    play(spell, &agent);
    EXPECT_TRUE(inHand(P1, spell)) << "not played";
    EXPECT_EQ(o(spell).zone, ZoneType::Hand);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_EQ(state.player(P1).cards_played_this_turn, 0);
    EXPECT_EQ(o(irelia).battlefieldId().value_or(99), 0u);
    EXPECT_EQ(o(holder).battlefieldId().value_or(99), 1u);
    EXPECT_FALSE(state.chain.exists());
    EXPECT_EQ(static_cast<int>(state.chain.next_id), chain_ids_before)
        << "the whole pass was rolled back, the chain id it took included";
    EXPECT_FALSE(agent.prompts.empty()) << "the decisions themselves were made";
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
        {static_cast<int>(irelia), static_cast<int>(other), EnginePathAgent::kRest}));
    execute(*offer, &agent);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(o(irelia).temp_might_bonus, 1);
    EXPECT_EQ(o(other).temp_might_bonus, 1) << "the second pick was still payable";
}

}  // namespace riftbound::test::sweep_costs
