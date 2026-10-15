/// @file test_review_fix_verify_cards.cpp
/// Tests added by the verifier of the 2026-10 card review (stream
/// sweep-verify-cards). Three kinds, each through the real engine path:
///   - Pin_*      : pins behaviour a finding questioned and that was closed as
///                  NOT A DEFECT (the plan line names the test);
///   - *_Accepts* : the accept path of a "you may" that the stage 2 tests only
///                  declined (Card::confirmOptional lists "No" first, so a test
///                  that passes no agent declines);
///   - the rest   : a fix that was merged without a test of its own, or whose
///                  test would still pass with the defect back.

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_verify_cards {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kFallingStar = 29;
constexpr CardDefId kVolibearFurious = 41;
constexpr CardDefId kPiratesHaven = 143;
constexpr CardDefId kSnapvine = 149;
constexpr CardDefId kSalvage = 224;
constexpr CardDefId kCandlelitSanctum = 286;
constexpr CardDefId kRelentlessStorm = 295;
constexpr CardDefId kAzirAscendant = 373;
constexpr CardDefId kArise = 513;
constexpr CardDefId kVoidBurrower = 553;
constexpr CardDefId kBloodRose = 671;
constexpr CardDefId kElderDragon = 680;
constexpr CardDefId kScryersBloom = 698;
constexpr CardDefId kAlphaStrike = 751;
constexpr CardDefId kRavenbloomPrefect = 889;
constexpr CardDefId kRecruit = 954;
constexpr CardDefId kSunDisc = 21;
constexpr CardDefId kWildclawShaman = 147;
constexpr CardDefId kMaddenedMarauder = 191;
constexpr CardDefId kIreliaFervent = 380;
constexpr CardDefId kJayceManOfProgress = 406;
constexpr CardDefId kBlastCone = 695;
constexpr CardDefId kDuskRoseLab = 765;
constexpr CardDefId kJayceBrilliantInventor = 855;
constexpr CardDefId kJaggedCutlass = 860;
constexpr CardDefId kMageseekerWarden = 70;
constexpr CardDefId kPromisingFuture = 115;
constexpr CardDefId kTeemoStrategist = 121;
constexpr CardDefId kIcathianRain = 247;
constexpr CardDefId kBulletTime = 263;
constexpr CardDefId kRekSaiSwarmQueen = 491;
constexpr CardDefId kArcaneShift = 515;
constexpr CardDefId kHeedlessResurrection = 704;
constexpr CardDefId kDeceiver = 756;
constexpr CardDefId kKeepersVerdict = 760;
constexpr CardDefId kForgottenRelic = 895;
constexpr CardDefId kSpiritsRefuge = 63;
constexpr CardDefId kConvergentMutation = 108;
constexpr CardDefId kUdyrWildman = 157;
constexpr CardDefId kBaitedHook = 242;
constexpr CardDefId kAltarOfMemories = 490;
constexpr CardDefId kSoulSword = 601;
constexpr CardDefId kAllay = 603;
constexpr CardDefId kToolsOfEmpire = 864;
constexpr CardDefId kArmedAssailant = 324;
constexpr CardDefId kSentinelAdept = 331;
constexpr CardDefId kSkyfallOfAreion = 353;
constexpr CardDefId kApheliosExalted = 372;
constexpr CardDefId kOrnnForgeGod = 407;
constexpr CardDefId kCombatChef = 414;
constexpr CardDefId kVeteranPoro421 = 421;
constexpr CardDefId kLucianMerciless = 435;
constexpr CardDefId kMasterBingwen = 448;
constexpr CardDefId kYoneBlademaster = 544;
constexpr CardDefId kRivenShattered = 828;
constexpr CardDefId kPoutyPoro = 13;
constexpr CardDefId kImmortalPhoenix = 37;
constexpr CardDefId kNocturneHorrifying = 194;
constexpr CardDefId kTwistedFateGambler = 200;
constexpr CardDefId kRecurveBow = 339;
constexpr CardDefId kDoransShield = 356;
constexpr CardDefId kBrutalizer = 365;
constexpr CardDefId kGuardianAngel = 374;
constexpr CardDefId kSteraksGage = 379;
constexpr CardDefId kSvellsongur = 382;
constexpr CardDefId kClothArmor = 387;
constexpr CardDefId kWindsinger = 459;
constexpr CardDefId kDefiantDance = 511;
constexpr CardDefId kFaefolk = 674;
constexpr CardDefId kKhaZixHunter = 681;
constexpr CardDefId kUndyingLoyalty = 730;
constexpr CardDefId kPunchFirst = 419;
constexpr CardDefId kIreliaGraceful = 462;
constexpr CardDefId kBardMercurial = 540;
constexpr CardDefId kAtakhan = 732;
constexpr CardDefId kSkyCrusier = 847;
constexpr CardDefId kRisenAltar = 950;
constexpr CardDefId kOvertOperation = 153;
constexpr CardDefId kFoxFire = 253;
constexpr CardDefId kAngleShot = 334;
constexpr CardDefId kBellowsBreath = 402;
constexpr CardDefId kSafetyInspector = 726;
constexpr CardDefId kVoidreaver = 787;
constexpr CardDefId kCorruptedDragon = 878;
constexpr CardDefId kDefenderOfTomorrow = 936;
constexpr CardDefId kBlindFury = 25;
constexpr CardDefId kReinforce = 62;
constexpr CardDefId kTheHarrowing = 198;
constexpr CardDefId kVoidHatchling = 341;
constexpr CardDefId kOrnnBlacksmith = 381;
constexpr CardDefId kZeroDrive = 412;
constexpr CardDefId kLastRites = 471;
constexpr CardDefId kVoidRush = 505;
constexpr CardDefId kRavenbloomConservatory = 529;
constexpr CardDefId kDoubleTrouble = 594;
constexpr CardDefId kIvernNurturer = 613;
constexpr CardDefId kFateWeaver = 626;
constexpr CardDefId kDianaLunari = 641;
constexpr CardDefId kCursedSarcophagus = 710;
constexpr CardDefId kRiftHerald = 741;
constexpr CardDefId kPakaaProtector = 820;
constexpr CardDefId kInferna = 560;
constexpr CardDefId kMysticVortex = 947;
constexpr CardDefId kSingularity = 105;
constexpr CardDefId kGuerillaWarfare = 260;
constexpr CardDefId kFlash = 309;
constexpr CardDefId kPiercingLight = 346;
constexpr CardDefId kEmperorsDivide = 366;
constexpr CardDefId kRelentlessPursuit = 503;
constexpr CardDefId kTricksyTentacles = 616;
constexpr CardDefId kDisposalOrder = 665;
constexpr CardDefId kMoonfall = 755;
constexpr CardDefId kShadowsOfThePast = 890;
constexpr CardDefId kDecreeOfDiscord = 894;
constexpr CardDefId kShurikenFlip = 927;
constexpr CardDefId kAccelerationGate = 937;
constexpr CardDefId kSonaHarmonious = 73;
constexpr CardDefId kKinkouMonk = 141;
constexpr CardDefId kKrakenHunter = 150;
constexpr CardDefId kAlbusFerros = 230;
constexpr CardDefId kCommanderLedros = 231;
constexpr CardDefId kJannaSavior = 376;
constexpr CardDefId kFaeDragon = 423;
constexpr CardDefId kAzirSovereign = 497;
constexpr CardDefId kForgeOfTheFuture = 212;
constexpr CardDefId kTargonsPeak = 284;
constexpr CardDefId kGentleGemdragon = 666;
constexpr CardDefId kCuratorOfTheSands = 932;
constexpr CardDefId kFlameChompers = 6;
constexpr CardDefId kSolariShrine = 72;
constexpr CardDefId kMistfall = 152;
constexpr CardDefId kCemeteryAttendant = 165;
constexpr CardDefId kSolariChief = 225;
constexpr CardDefId kVanguardHelm = 228;
constexpr CardDefId kMonasteryOfHirana = 277;
constexpr CardDefId kAnnieStubborn = 308;
constexpr CardDefId kLucianGunslinger = 351;
constexpr CardDefId kSpiritWheel = 465;
constexpr CardDefId kSunkenTemple = 532;
constexpr CardDefId kTreasureHoard = 534;
constexpr CardDefId kBladeDancer = 555;
constexpr CardDefId kMischievousMarai = 562;
constexpr CardDefId kFreshBeans = 573;
constexpr CardDefId kStarhound = 729;
constexpr CardDefId kRippersBay = 770;
constexpr CardDefId kValleyOfIdols = 774;
constexpr CardDefId kPiltoverEnforcer = 783;
constexpr CardDefId kTombRaiderBarbara = 824;
constexpr CardDefId kDameTheDespoiler = 866;
constexpr CardDefId kJayceHammerInHand = 875;
constexpr CardDefId kAmbessaRespectedAndFeared = 923;
constexpr CardDefId kProtectiveSands = 949;
constexpr CardDefId kVeiledTemple = 535;
constexpr CardDefId kImposingChallenger = 667;
constexpr CardDefId kVayne035 = 35;
constexpr CardDefId kBackAlleyBar = 272;
constexpr CardDefId kSoraka = 550;
constexpr CardDefId kAssemblyRig = 342;
constexpr CardDefId kStarSpring = 771;
constexpr CardDefId kAkshan = 431;
constexpr CardDefId kLongSword = 345;         // gear, Equipment
constexpr CardDefId kPurifier = 502;          // legend
constexpr CardDefId kForgeOfTheFluft = 522;
constexpr CardDefId kMeditation = 48;          // spell, [2]
constexpr CardDefId kBlitzcrank = 67;
constexpr CardDefId kMegaMech = 88;            // unit, [7], Mech, 8 [M]
constexpr CardDefId kKaiSaEvolutionary = 112;
constexpr CardDefId kAhri119 = 119;
constexpr CardDefId kMobilize = 134;
constexpr CardDefId kSealOfStrength163 = 163;
constexpr CardDefId kSett164 = 164;
constexpr CardDefId kShipyardSkulker = 175;    // unit, [3], 3 [M], no text
constexpr CardDefId kYasuo205 = 205;
constexpr CardDefId kSpectralMatron = 226;
constexpr CardDefId kKarma235 = 235;
constexpr CardDefId kDarius243 = 243;
constexpr CardDefId kDarkChild = 315;
constexpr CardDefId kGold = 326;
constexpr CardDefId kSerratedDirk = 332;       // gear, Equipment, [1]
constexpr CardDefId kRumbleHotheaded = 349;
constexpr CardDefId kNotSoFast = 368;
constexpr CardDefId kPlunderingPoro392 = 392;
constexpr CardDefId kBreakneckMech = 394;
constexpr CardDefId kEzrealDashing = 404;
constexpr CardDefId kDirewing = 416;
constexpr CardDefId kFaePorter = 446;
constexpr CardDefId kLoyalPup = 447;
constexpr CardDefId kOverzealousFan = 449;
constexpr CardDefId kFizzTrickster = 461;
constexpr CardDefId kGuards = 475;
constexpr CardDefId kGlascMixologist = 486;
constexpr CardDefId kShurelyasRequiem = 509;
constexpr CardDefId kForgottenMonument = 523;
constexpr CardDefId kVayne537 = 537;
constexpr CardDefId kAhri539 = 539;
constexpr CardDefId kSett543 = 543;
constexpr CardDefId kYasuo546 = 546;
constexpr CardDefId kDarius547 = 547;
constexpr CardDefId kKarma548 = 548;
constexpr CardDefId kVexMocking = 617;
constexpr CardDefId kHwei = 642;
constexpr CardDefId kRepulse = 668;
constexpr CardDefId kStalkingWolf = 728;
constexpr CardDefId kVoidAssault = 758;
constexpr CardDefId kVeteranPoro779 = 779;
constexpr CardDefId kNoxianDemolitionist = 867;
constexpr CardDefId kWildClaw = 876;
constexpr CardDefId kMatriarchOfWar = 940;
constexpr CardDefId kShadowClone = 955;

// ─── A test card for the shared selectSeveral helper (plans/09 item 9.HH5) ───
// "When I conquer, choose up to three other friendly units." It names them at
// finalization and, as it resolves, writes down exactly what selectSeveral
// handed back (on its own object, in card_counters), so a test can read the
// helper's result without going through any real card's guards or trace text.
constexpr CardDefId kSelectSeveralProbe = 1860;   // reserved range, test-only

class SelectSeveralProbeCard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects)
            if (id != ctx.source && obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value()) others.push_back(id);
        const auto chosen = selectSeveral(ctx, "Probe: choose a friendly unit", others,
                                          /*max_picks=*/3);
        if (!chosen.has_value()) return;        // waiting for the agent
        if (finalizingChoices(ctx)) return;     // choices end here
        auto& counters = ctx.state.getObject(ctx.source).card_counters;
        counters["__probe_count"] = static_cast<int>(chosen->size());
        for (size_t k = 0; k < chosen->size(); ++k)
            counters["__probe_" + std::to_string(k)] = static_cast<int>(((*chosen)[k]).raw());
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = kSelectSeveralProbe;
        d.name = "SelectSeveral Probe";
        d.card_type = CardType::Unit;
        d.might = 1;
        d.ability_text = "When I conquer, choose up to three other friendly units.";
        return d;
    }();
};

// The same probe for a list that names one object several times (plans/09
// item 9.HH6): "choose up to three buffs on other friendly units" — a unit is
// listed once per buff, as Albus Ferros 230 lists them.
constexpr CardDefId kSelectSeveralDupProbe = 1861;   // reserved range, test-only

class SelectSeveralDupProbeCard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> buffs;   // one entry per buff
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source || !obj.isUnit() || obj.controller != ctx.controller ||
                !obj.location.has_value()) continue;
            for (int k = 0; k < obj.buff_count; ++k) buffs.push_back(id);
        }
        const auto chosen = selectSeveral(ctx, "Probe: choose a buff", buffs, /*max_picks=*/3);
        if (!chosen.has_value()) return;        // waiting for the agent
        if (finalizingChoices(ctx)) return;     // choices end here
        auto& counters = ctx.state.getObject(ctx.source).card_counters;
        counters["__probe_count"] = static_cast<int>(chosen->size());
        for (size_t k = 0; k < chosen->size(); ++k)
            counters["__probe_" + std::to_string(k)] = static_cast<int>(((*chosen)[k]).raw());
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = kSelectSeveralDupProbe;
        d.name = "SelectSeveral Duplicate Probe";
        d.card_type = CardType::Unit;
        d.might = 1;
        d.ability_text = "When I conquer, choose up to three buffs on other friendly units.";
        return d;
    }();
};

class VerifyCardsTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
    bool inBanishment(PlayerId p, GameObjectId id) {
        auto& b = ps(p).banishment;
        return std::find(b.begin(), b.end(), id) != b.end();
    }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
    int& power(PlayerId p, Domain d) { return ps(p).rune_pool.power[static_cast<int>(d)]; }

    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = ps(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        if (def_id != kInvalidId) {
            o(id).base_might = card_db.get(def_id).might;
            o(id).current_might = o(id).base_might;
        }
        ps(owner).trash.push_back(id);
        return id;
    }
    /// Take an on-board object off the board into its owner's trash, as if it
    /// had died before the ability under test was finalized.
    void sendToTrash(GameObjectId id) {
        o(id).location = std::nullopt;
        o(id).zone = ZoneType::Trash;
        ps(o(id).owner).trash.push_back(id);
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            g.name = def.name;
            g.super_type = def.super_type;
            g.keywords = def.keywords;
            g.domains = def.domains;
            g.tags = def.tags;
            g.might_bonus = def.might_bonus;
        } else {
            g.name = "TestGear";
        }
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& def = card_db.get(def_id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        l.card_type = CardType::Legend;
        l.name = def.name;
        l.domains = def.domains;
        l.tags = def.tags;
        l.zone = ZoneType::LegendZone;
        ps(owner).legend_zone = id;
        return id;
    }
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = o(gear);
        auto& u = o(unit);
        g.attached_to = unit;
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    void activate(GameObjectId source, AgentInterface* agent = nullptr, int index = 0) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = o(source).controller;
        act.ability_source = source;
        act.ability_index = index;
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, agent, nullptr,
                   act.player);
    }
    /// Play `card` with play-time `targets`, its printed cost covered.
    void playAt(GameObjectId card, std::vector<GameObjectId> targets,
                AgentInterface* p1 = nullptr, AgentInterface* p2 = nullptr,
                const std::function<void(GameEngine&)>& after = nullptr) {
        const PlayerId player = o(card).controller;
        const auto& def = card_db.get(o(card).card_def_id);
        ps(player).rune_pool.energy += def.energy_cost;
        ps(player).rune_pool.universal_power += def.power_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = card;
        play.targets = std::move(targets);
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            if (after) after(e);
        }, p1, p2, player);
    }
    /// Queue `source`'s trigger and run the chain. `probe` is handed the live
    /// state the first time the opponent gets priority: the ability has been
    /// finalized (choices made, costs paid) and has not resolved yet.
    void fireAndProbe(GameObjectId source, TriggerType trigger, AgentInterface* p1_agent,
                      const std::function<void(GameState&)>& probe,
                      GameObjectId subject = kInvalidId) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    /// For a triggered ability that is NOT PERFORMED: its leading "may" is
    /// not offered or declined, its cost cannot be paid, or a required choice
    /// has no legal option as it would be finalized. CR 403.1.a / 403.4 /
    /// 405.2: it is removed from the chain, never finalized — so nobody gets
    /// priority (finding D4; these tests used to change the state "in
    /// response" through fireAndProbe and check that nothing was asked as
    /// the item resolved). The ability is gone for good: `after` changes the
    /// state once the chain has ended, and nothing reacts to it.
    void fireNotPerformed(GameObjectId source, TriggerType trigger, AgentInterface* p1_agent,
                          const std::function<void(GameState&)>& after,
                          GameObjectId subject = kInvalidId) {
        int priority_decisions = 0, finalized = 0;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) { ++priority_decisions; return i; }
            return legal.front();
        });
        auto conn = events.on_chain_item_finalized.connect(
            [&](const ChainItemFinalizedEvent&) { ++finalized; });
        withEngine([&](GameEngine& e) {
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            e.testHook_runChain();
            EXPECT_FALSE(e.mutableState().chain.exists());
            after(e.mutableState());
            e.testHook_runChain();
        }, p1_agent, &p2);
        conn.disconnect();
        EXPECT_EQ(finalized, 0) << "the ability was finalized (CR 403: it is removed instead)";
        EXPECT_EQ(priority_decisions, 0) << "the opponent got priority for an ability that left the chain";
    }
    /// Execute `intent` for P1; `probe` is handed the live state the first
    /// time the opponent gets priority (the spell or ability is on the chain,
    /// finalized, not resolved).
    void executeAndProbe(const Intent& intent, AgentInterface* p1_agent,
                         const std::function<void(GameState&)>& probe) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(intent);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    static void removeFromBoard(GameState& live, GameObjectId id) {
        auto& x = live.getObject(id);
        x.location = std::nullopt;
        x.zone = ZoneType::Trash;
        live.player(x.owner).trash.push_back(id);
    }
    /// A battlefield card of `def_id` in slot `bf`, controlled by `controller`.
    GameObjectId battlefieldCard(int bf, CardDefId def_id, PlayerId controller = P1) {
        auto id = state.createObject();
        auto& b = o(id);
        b.card_def_id = def_id;
        b.card_type = CardType::Battlefield;
        b.name = card_db.get(def_id).name;
        b.owner = b.controller = controller;
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        state.battlefields[bf].controller = controller;
        return id;
    }
    GameObjectId unitInHand(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        o(id).base_might = card_db.get(def_id).might;
        o(id).current_might = o(id).base_might;
        return id;
    }
    /// `p` controls battlefield slot `bf` and has a unit there (so a cleanup
    /// does not take the control away again).
    void holdBattlefield(int bf, PlayerId p = P1) {
        state.battlefields[bf].controller = p;
        addUnit(p, kInvalidId, 1, bf);
    }
    /// Picks `id` wherever it is offered; answers every other prompt with the
    /// next of `values` (matched against chosen_value), then the first option.
    static EnginePathAgent::Picker pickingThen(GameObjectId id, std::vector<int> values) {
        auto n = std::make_shared<size_t>(0);
        return [id, values = std::move(values), n](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            if (*n < values.size()) {
                const int want = values[(*n)++];
                for (const auto& i : legal)
                    if (i.chosen_value.has_value() && *i.chosen_value == want) return i;
            }
            return legal.front();
        };
    }
    /// True if `agent` was asked Void Hatchling's look (EffectExecutor::
    /// lookBeforeReveal): two int answers, 0 and 1, labelled "Void Hatchling:
    /// leave / recycle the top card ..." (the labels came with plans/09 item
    /// 9.OO2; before, it was the only prompt made of two unlabelled answers).
    static bool askedHatchlingLook(const EnginePathAgent& agent) {
        for (const auto& p : agent.prompts) {
            if (p.size() != 2) continue;
            bool look = true;
            for (const auto& i : p)
                if (!i.chosen_value.has_value() || !i.chosen_objects.empty() ||
                    i.choice_label.rfind("Void Hatchling: ", 0) != 0) look = false;
            if (look) return true;
        }
        return false;
    }
    /// A Void Hatchling for `p`, with its "look before you reveal" switched on.
    void withVoidHatchling(PlayerId p = P1) {
        addUnit(p, kVoidHatchling, 0);
        cleanup();   // the aura pass sets PlayerState::has_reveal_peek
        ASSERT_TRUE(ps(p).has_reveal_peek);
    }
    /// Put a card that is in `owner`'s trash into banishment, recorded as
    /// banished with `gear` (The Zero Drive, Cursed Sarcophagus).
    void banishWith(GameObjectId gear, GameObjectId card) {
        auto& owner = ps(o(card).owner);
        owner.trash.erase(std::remove(owner.trash.begin(), owner.trash.end(), card),
                          owner.trash.end());
        o(card).zone = ZoneType::Banishment;
        owner.banishment.push_back(card);
        o(gear).tracked_objects.push_back(card);
    }

    /// 9.O1 scenario: Mystic Vortex 947 in battlefield slot 0, a unit of each
    /// player there, a P1 unit at battlefield 1, P1's main phase. Returns an
    /// Inferna ([Ambush], [2], no Power) in P1's hand: with the Vortex's tax she
    /// costs [2] + [A].
    GameObjectId vortexScenario() {
        battlefieldCard(0, kMysticVortex, P2);
        addUnit(P1, kInvalidId, 1, 0);
        addUnit(P2, kInvalidId, 1, 0);
        addUnit(P1, kInvalidId, 1, 1);
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = P1;
        return addToHand(P1, kInferna);
    }
    /// A Non-Combat Showdown open at slot `bf`, P1 with Focus (what
    /// GameEngine::runShowdown + advanceShowdown set).
    void openShowdownAt(GameState& st, int bf) {
        st.turn.ns_state = NeutralShowdownState::Showdown;
        st.turn.oc_state = OpenClosedState::Open;
        st.turn.focus_holder = P1;
        st.turn.priority_holder = P1;
        st.battlefields[bf].showdown_in_progress = true;
    }
    /// A combat at slot `bf` in step `phase`, P1 attacking (what
    /// GameEngine::runCombat sets: it never sets showdown_in_progress).
    void openCombatAt(GameState& st, int bf, CombatPhase phase) {
        st.turn.ns_state = NeutralShowdownState::Showdown;
        st.turn.oc_state = OpenClosedState::Open;
        st.turn.focus_holder = P1;
        st.turn.priority_holder = P1;
        auto& b = st.battlefields[bf];
        b.showdown_in_progress = false;
        b.combat_in_progress = true;
        b.combat_phase = phase;
        b.attacker = P1;
        b.defender = P2;
    }
    /// Whether P1 can afford `card` with `energy` + `any_power` floating, in the
    /// engine's own order: the aura pass runs in the cleanup BEFORE a showdown
    /// or combat opens (advanceMainPhase -> runShowdown / runCombat set their
    /// flags and ask the first decision with no aura pass in between), then
    /// `open` sets the live flags.
    bool affordableAfter(GameObjectId card, int energy, int any_power,
                         const std::function<void(GameState&)>& open) {
        ps(P1).rune_pool.energy = energy;
        ps(P1).rune_pool.universal_power = any_power;
        bool ok = false;
        withEngine([&](GameEngine& e) {
            e.testHook_recalculateAuras();
            open(e.mutableState());
            ok = e.testHook_canAfford(P1, card);
        });
        return ok;
    }
    /// The generator's [Ambush] play of `card` to battlefield slot `bf`, if it
    /// is offered in the fixture's current state.
    std::optional<Intent> ambushOffer(GameObjectId card, int bf) {
        std::optional<Intent> found;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.card == card && i.play_location &&
                    *i.play_location ==
                        LocationId{BattlefieldLocation{BattlefieldId::fromRaw(bf)}})
                    found = i;
        });
        return found;
    }

    /// Answers the n-th MakeChoice prompt with the object order[n]; kInvalidId
    /// stands for the option that carries a value and no object ("Stop here").
    /// A wanted option that is not on offer is a test failure. First option
    /// once the list runs out.
    static EnginePathAgent::Picker pickingInOrder(std::vector<GameObjectId> order) {
        auto n = std::make_shared<size_t>(0);
        return [order = std::move(order), n](const std::vector<Intent>& legal) {
            if (*n >= order.size()) return legal.front();
            const GameObjectId want = order[(*n)++];
            for (const auto& i : legal) {
                if (want == kInvalidId
                        ? (i.chosen_objects.empty() && i.chosen_value.has_value())
                        : (!i.chosen_objects.empty() && i.chosen_objects.front() == want))
                    return i;
            }
            ADD_FAILURE() << "prompt " << *n << ": the wanted option was not offered";
            return legal.front();
        };
    }
    /// The objects prompt `n` offered, ascending.
    static std::vector<GameObjectId> promptObjects(const EnginePathAgent& agent, size_t n) {
        std::vector<GameObjectId> out;
        if (n >= agent.prompts.size()) return out;
        for (const auto& i : agent.prompts[n])
            if (!i.chosen_objects.empty()) out.push_back(i.chosen_objects.front());
        std::sort(out.begin(), out.end());
        return out;
    }
    /// Every play of `card` the generator offers its controller right now,
    /// with the printed cost covered.
    std::vector<Intent> playsOffered(GameObjectId card) {
        const PlayerId player = o(card).controller;
        const auto& def = card_db.get(o(card).card_def_id);
        const auto pool = ps(player).rune_pool;
        ps(player).rune_pool.energy += def.energy_cost;
        ps(player).rune_pool.universal_power += def.power_cost;
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.card == card && (i.type == IntentType::PlayCard ||
                                       i.type == IntentType::PlayActionCard ||
                                       i.type == IntentType::PlayReaction))
                    out.push_back(i);
        }, nullptr, nullptr, player);
        ps(player).rune_pool = pool;
        return out;
    }
    /// True if one of `plays` has exactly `targets` (in any order).
    static bool offersTargets(const std::vector<Intent>& plays, std::vector<GameObjectId> targets) {
        std::sort(targets.begin(), targets.end());
        for (const auto& p : plays) {
            auto t = p.targets;
            std::sort(t.begin(), t.end());
            if (t == targets) return true;
        }
        return false;
    }
    /// A unit token of `p` with no card behind it.
    GameObjectId addTokenUnit(PlayerId p, int might, int at_bf = -1) {
        auto id = addUnit(p, kInvalidId, might, at_bf);
        o(id).is_token = true;
        o(id).super_type = SuperType::Token;
        return id;
    }
    /// A rune of `p` in their rune deck (for "channel" effects).
    GameObjectId addRuneToRuneDeck(PlayerId p, Domain d) {
        auto rune = addRune(p, d);
        o(rune).location = std::nullopt;
        o(rune).zone = ZoneType::RuneDeck;
        ps(p).rune_deck.push_back(rune);
        return rune;
    }
    /// True if `agent` was asked a prompt with an option whose label contains `text`.
    static bool askedLabel(const EnginePathAgent& agent, const std::string& text) {
        for (const auto& p : agent.prompts)
            for (const auto& i : p)
                if (i.choice_label.find(text) != std::string::npos) return true;
        return false;
    }

    /// fireAndProbe for a trigger that is easier to cause than to name:
    /// `cause` is run on the live state with the engine's event bus listening,
    /// then the chain is run. `probe` is handed the live state the first time
    /// the opponent gets priority.
    void causeAndProbe(const std::function<void(GameState&)>& cause, AgentInterface* p1_agent,
                       const std::function<void(GameState&)>& probe) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            cause(*live);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }

    /// fireAndProbe for a trigger whose condition may keep it off the chain
    /// (Card::triggerCondition): returns whether the opponent got priority,
    /// i.e. whether an ability was queued at all, instead of failing when not.
    bool fireAndProbeIfQueued(GameObjectId source, TriggerType trigger, AgentInterface* p1_agent,
                              const std::function<void(GameState&)>& probe,
                              GameObjectId subject = kInvalidId) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        return probed;
    }
    /// Put an object onto the board in response — a unit moved, or a card
    /// taken out of its owner's hand or trash: at battlefield slot `at_bf`, or
    /// in its controller's base (-1).
    static void putOnBoard(GameState& live, GameObjectId id, int at_bf = -1) {
        auto& x = live.getObject(id);
        auto& p = live.player(x.owner);
        p.hand.erase(std::remove(p.hand.begin(), p.hand.end(), id), p.hand.end());
        p.trash.erase(std::remove(p.trash.begin(), p.trash.end(), id), p.trash.end());
        if (at_bf >= 0) {
            x.zone = ZoneType::BattlefieldZone;
            x.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            x.zone = ZoneType::Base;
            x.location = BaseLocation{x.controller};
        }
    }
    int countNamed(PlayerId owner, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == owner && obj.location.has_value() && obj.name == name) ++n;
        return n;
    }
    GameObjectId firstNamed(PlayerId owner, const std::string& name) {
        GameObjectId best = kInvalidId;
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == owner && obj.location.has_value() && obj.name == name &&
                (best == kInvalidId || id < best)) best = id;
        return best;
    }
    /// True if prompt `n` of `agent` offers the int-coded answer `value`.
    static bool promptOffersValue(const EnginePathAgent& agent, size_t n, int value) {
        if (n >= agent.prompts.size()) return false;
        for (const auto& i : agent.prompts[n])
            if (i.chosen_value.has_value() && *i.chosen_value == value) return true;
        return false;
    }
    std::vector<std::string> tagsOf(CardDefId id) { return card_db.get(id).tags; }
};

using Tags = std::vector<std::string>;

// ═══ Work item 5: Spectral Matron 226 / Rumble 349 use pickEffectPlayLocation ═

TEST_F(VerifyCardsTest, SpectralMatron_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    // CR 355.2.a through the shared helper: a controlled battlefield that
    // says units can't be played there (Rockfall Path) is not a choice.
    auto matron = addToHand(P1, kSpectralMatron);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);   // held: control without a unit there is lost at the next Cleanup (CR 323.5)
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(cheap.raw()), 2}));
    playThroughEngine(matron, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(cheap));
    EXPECT_EQ(bfOf(cheap), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, RumbleHotheaded_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto rumble = addUnit(P1, kRumbleHotheaded, 0, 0);
    auto fodder = addUnit(P1, kShipyardSkulker, 7);          // 7 [M]: the Mech is free
    auto mech = addToTrash(P1, kMegaMech);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder.raw()), static_cast<int>(mech.raw()), 2}));
    fireTriggerThroughEngine(rumble, TriggerType::WhenIConquer, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    const size_t last = agent.prompts.size() - 1;
    EXPECT_FALSE(promptOffersValue(agent, last, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, last, 2));
    ASSERT_TRUE(onBoard(mech));
    EXPECT_EQ(bfOf(mech), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// ═══ Work item 3: fixes merged without a test ═══════════════════════════════

// ── Wild Claw 876 (batch-09 "playPayingCost result ignored") ──

TEST_F(VerifyCardsTest, WildClaw_ThroughTheEngine_PlaysTheChosenUnitForFiveLessAndEmpowersIt) {
    addToDeck(P1, kMeditation);                       // sixth from the top: not looked at
    auto mech = addToDeck(P1, kMegaMech);             // [7]: [2] after the reduction
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    ps(P1).rune_pool.energy = 2;
    EnginePathAgent agent(EnginePathAgent::choosing(mech, EnginePathAgent::May::Yes));   // the unit, then "yes"
    playThroughEngine(spell, &agent);
    ASSERT_TRUE(onBoard(mech));
    EXPECT_TRUE(o(mech).isEmpowered()) << "Then you may do this: Empower it.";
    EXPECT_FALSE(inBanishment(P1, mech)) << "played out of banishment";
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "only [5] of its [7] is taken off";
    EXPECT_EQ(deckSize(P1), 5) << "the other four were recycled";
}

TEST_F(VerifyCardsTest, WildClaw_APaymentThatFailsLeavesTheCardBanished) {
    // The card is chosen (and banished) at one prompt and paid for after the
    // next ("where to play it"). Nothing in the engine can change what the
    // controller can pay between the two — only the controller is asked, no
    // priority is given and no trigger resolves — so the failing payment is
    // forced here by emptying the pool while that second prompt is open.
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;            // so the location is asked
    ps(P1).rune_pool.energy = 2;

    GameEngine* live = nullptr;
    int prompt = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (prompt++ == 0) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == mech) return i;
            ADD_FAILURE() << "Mega Mech was not offered";
        } else if (live) {
            live->mutableState().player(P1).rune_pool.energy = 0;
        }
        return legal.front();
    });
    const auto& def = card_db.get(kWildClaw);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executeIntent(play);
        live = nullptr;
    }, &agent);

    EXPECT_EQ(agent.prompts.size(), 2u) << "no Empower prompt: nothing was played";
    EXPECT_FALSE(onBoard(mech));
    EXPECT_TRUE(inBanishment(P1, mech)) << "banished, and still listed there: not in no zone";
    EXPECT_EQ(o(mech).zone, ZoneType::Banishment);
    EXPECT_FALSE(inDeck(P1, mech));
}

// ── On-board guards: Vayne 537, Vex 617, Hwei 642 (07 item 7.26, batch-10 S2) ──

TEST_F(VerifyCardsTest, Vayne537_OffTheBoardIsNotOfferedTheReturnAndPaysNothing) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    sendToTrash(vayne);                               // died before the trigger was finalized
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "she is not on the board: nothing to return";
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "the [1] is not taken for nothing";
    EXPECT_FALSE(inHand(P1, vayne));
    EXPECT_TRUE(inTrash(P1, vayne));
}

TEST_F(VerifyCardsTest, Vayne537_KilledInResponseStaysInTheTrash) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(vayne, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.energy, 0) << "paid as the trigger was finalized";
        auto& v = live.getObject(vayne);              // ...and she is killed in response
        v.location = std::nullopt;
        v.zone = ZoneType::Trash;
        live.player(P1).trash.push_back(vayne);
    });
    EXPECT_FALSE(inHand(P1, vayne)) << "a card in the trash is not returned to hand";
    EXPECT_TRUE(inTrash(P1, vayne));
    EXPECT_EQ(o(vayne).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, Vayne537_AcceptingPaysOneAndReturnsHerToHand) {
    auto vayne = addUnit(P1, kVayne537, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inHand(P1, vayne));
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
}

TEST_F(VerifyCardsTest, VexMocking_OffTheBoardIsNotOfferedTheMove) {
    auto vex = addUnit(P1, kVexMocking, 0);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 1);
    o(vex).card_counters["__stunned_unit_id"] = static_cast<int>(enemy.raw());
    sendToTrash(vex);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vex, TriggerType::WhenYouStun, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "she left the board: the may is not offered";
    EXPECT_FALSE(onBoard(vex));
    EXPECT_TRUE(inTrash(P1, vex));
}

TEST_F(VerifyCardsTest, VexMocking_AcceptingMovesHerToTheStunnedUnitsBattlefield) {
    auto vex = addUnit(P1, kVexMocking, 0);
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.stunUnitBy(enemy, vex);
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(bfOf(vex), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, Hwei_OffTheBoardStillDiscardsButGetsNoMight) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);                       // the card drawn
    auto unit_card = addToHand(P1, kShipyardSkulker);
    sendToTrash(hwei);
    EnginePathAgent agent(EnginePathAgent::choosing(unit_card));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    EXPECT_TRUE(inTrash(P1, unit_card)) << "draw 1, then discard 1 still happen";
    EXPECT_EQ(o(hwei).temp_might_bonus, 0) << "\"Give me +3\": I am not on the board";
}

TEST_F(VerifyCardsTest, Hwei_OnTheBoardDiscardingAUnitGivesPlusThree) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit_card = addToHand(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(unit_card));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    EXPECT_TRUE(inTrash(P1, unit_card));
    EXPECT_EQ(o(hwei).temp_might_bonus, 3);
}

// ═══ Work item 2: pins for NOT A DEFECT closures ════════════════════════════

// 03 item 3.17: Seal of Strength 163 was never the wrong one (542 was).
TEST_F(VerifyCardsTest, Pin_3_17_SealOfStrength163_AddsBodyPower) {
    auto seal = addGear(P1, kSealOfStrength163);
    activate(seal);
    EXPECT_EQ(power(P1, Domain::Body), 1) << "[Add] [O]: Body";
    EXPECT_EQ(power(P1, Domain::Order), 0);
    EXPECT_TRUE(o(seal).is_exhausted);
}

// 03 item 3.18: Plundering Poro 392 already made its Gold exhausted (778 did not).
TEST_F(VerifyCardsTest, Pin_3_18_PlunderingPoro392_TheGoldEntersExhausted) {
    auto poro = addUnit(P1, kPlunderingPoro392, 0, 0);
    fireTriggerThroughEngine(poro, TriggerType::WhenIConquer);
    ASSERT_EQ(countNamed(P1, "Gold"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Gold")).is_exhausted) << "play a Gold gear token exhausted";
}

// 03 item 3.21 (and batch-06 548 / 547 / 543 / data): each printing carries
// the tags printed on it — the Spiritforged reprints print fewer region tags
// than the Origins cards (cards/raw/gallery_raw.json).
TEST_F(VerifyCardsTest, Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting) {
    EXPECT_EQ(tagsOf(kAhri119), (Tags{"Ahri", "Ionia"}));                 // OGN-119
    EXPECT_EQ(tagsOf(kAhri539), (Tags{"Ahri"}));                          // SFD-227
    EXPECT_EQ(tagsOf(kSett164), (Tags{"Sett", "Ionia"}));                 // OGN-164
    EXPECT_EQ(tagsOf(kSett543), (Tags{"Sett"}));                          // SFD-232
    EXPECT_EQ(tagsOf(kYasuo205), (Tags{"Yasuo", "Ionia"}));               // OGN-205
    EXPECT_EQ(tagsOf(kYasuo546), (Tags{"Yasuo"}));                        // SFD-235
    EXPECT_EQ(tagsOf(kVayne035), (Tags{"Vayne", "Demacia"}));             // OGN-035
    EXPECT_EQ(tagsOf(kVayne537), (Tags{"Vayne", "Demacia", "Sentinel"})); // SFD-223
    EXPECT_EQ(tagsOf(kDarius243), (Tags{"Trifarian", "Darius", "Noxus"})); // OGN-243
    EXPECT_EQ(tagsOf(kDarius547), (Tags{"Trifarian", "Darius"}));         // SFD-236
    EXPECT_EQ(tagsOf(kKarma235), (Tags{"Karma", "Ionia"}));               // OGN-235
    EXPECT_EQ(tagsOf(kKarma548), (Tags{"Karma"}));                        // SFD-237
    // Whatever its region tags, each is a legal Chosen Champion for its name.
    for (auto [id, name] : {std::pair<CardDefId, const char*>{kAhri539, "Ahri"},
                            {kSett543, "Sett"}, {kYasuo546, "Yasuo"},
                            {kVayne537, "Vayne"}, {kDarius547, "Darius"},
                            {kKarma548, "Karma"}}) {
        const auto t = tagsOf(id);
        EXPECT_NE(std::find(t.begin(), t.end(), name), t.end()) << name;
        EXPECT_EQ(card_db.get(id).super_type, SuperType::Champion) << name;
    }
}

// 03 item 3.21: Veteran Poro 779 — [Weaponmaster] is its only keyword.
TEST_F(VerifyCardsTest, Pin_3_21_VeteranPoro779_HasWeaponmasterAndNoEquip) {
    const auto& def = card_db.get(kVeteranPoro779);
    EXPECT_TRUE(def.keywords.has(Keyword::Weaponmaster));
    EXPECT_FALSE(def.keywords.has(Keyword::Equip))
        << "a Weaponmaster unit has no Equip ability of its own";
    bool offered = false;
    auto poro = addUnit(P1, kVeteranPoro779, 0);
    ps(P1).rune_pool.energy = 5;
    ps(P1).rune_pool.universal_power = 5;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.ability_source == poro) offered = true;
    });
    EXPECT_FALSE(offered) << "nothing to activate on it";
}

// batch-01 Systemic S5: Dark Child 315 "ready up to 2 runes" — the controller
// chooses which, and may ready fewer (CR 355.13; the earlier "asks nothing"
// closure was wrong: which rune is ready matters when one is recycled for Power).
TEST_F(VerifyCardsTest, Pin_Batch01_DarkChild315_ReadiesUpToTwoRunesItsControllerPicks) {
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, true);
    auto r2 = addRune(P1, Domain::Chaos, true);
    auto r3 = addRune(P1, Domain::Mind, true);
    auto theirs = addRune(P2, Domain::Fury, true);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r1.raw()), static_cast<int>(r2.raw())}));
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "one prompt per rune, no third";
    EXPECT_TRUE(agent.wasOffered(theirs)) << "\"runes\": any rune on the board (no \"friendly\" in the text)";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_TRUE(o(r3).is_exhausted);
    EXPECT_EQ(readyRuneCount(P2), 0);
}

// Carry-over (engine-auras-costs): Shurelya's Requiem 509 grants [Ganking]
// "here" — the sentence is the Equipment's effect_text, not its ability_text.
TEST_F(VerifyCardsTest, Pin_Shurelya509_UnitsWhereTheBearerIsHaveGanking) {
    const auto& def = card_db.get(kShurelyasRequiem);
    EXPECT_NE(def.effect_text.find("Your units here have [Ganking]"), std::string::npos);
    EXPECT_EQ(def.ability_text.find("Ganking"), std::string::npos);
    auto bearer = addUnit(P1, kShipyardSkulker, 0, 0);
    auto here = addUnit(P1, kShipyardSkulker, 0, 0);
    auto elsewhere = addUnit(P1, kShipyardSkulker, 0, 1);
    auto enemy_here = addUnit(P2, kShipyardSkulker, 0, 0);
    auto requiem = addGear(P1, kShurelyasRequiem);
    attach(requiem, bearer);
    cleanup();
    EXPECT_TRUE(o(bearer).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(o(here).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(elsewhere).hasKeyword(Keyword::Ganking));
    EXPECT_FALSE(o(enemy_here).hasKeyword(Keyword::Ganking));
}

// batch-03 758 Void Assault: "(If they both move to a battlefield you don't
// control, you're the attacker.)" needs no code in the card.
TEST_F(VerifyCardsTest, Pin_Batch03_VoidAssault758_TheCasterContestsAndIsTheAttacker) {
    auto spell = addToHand(P1, kVoidAssault);
    auto mine = addUnit(P1, kInvalidId, 3);          // base: may go to bf 0 or bf 1
    auto theirs = addUnit(P2, kInvalidId, 3, 0);     // bf 0: may go to base or bf 1
    // Options are the pairs (mine x theirs): index 3 = mine -> bf 1, theirs -> bf 1.
    EnginePathAgent agent(EnginePathAgent::script({3}));
    playAt(spell, {mine, theirs}, &agent, nullptr,
           [](GameEngine& e) { e.testHook_cleanup(); });
    ASSERT_EQ(bfOf(mine), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    ASSERT_EQ(bfOf(theirs), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(state.battlefields[1].contested_by, P1)
        << "CR 190.3.a: the turn player — the caster — applies Contested, and the "
           "player who applied it is the attacker";
}

// batch-03 368 Not So Fast / 668 Repulse: a triggered ability is never given
// targets; its event's subject is not something it chose.
TEST_F(VerifyCardsTest, Pin_Batch03_NotSoFastAndRepulse_ATriggersSubjectIsNotAChoice) {
    auto mine = addUnit(P1, kShipyardSkulker, 0, 0);
    auto their_poro = addUnit(P2, kPlunderingPoro392, 0, 0);
    Card* nsf = card_registry.get(kNotSoFast);
    Card* repulse = card_registry.get(kRepulse);
    ASSERT_NE(nsf, nullptr);
    ASSERT_NE(repulse, nullptr);
    withEngine([&](GameEngine& e) {
        // An enemy trigger whose event is about my unit, queued by TriggerManager.
        e.testHook_executor().queueTriggeredAbility(their_poro, P2, TriggerType::WhenIConquer, mine);
        auto& st = e.mutableState();
        ASSERT_FALSE(st.chain.items.empty());
        auto& top = st.chain.items.back();
        EXPECT_EQ(top.triggering_subject, mine);
        EXPECT_TRUE(top.targets.empty()) << "the engine never hands a trigger targets";
        EXPECT_FALSE(nsf->hasLegalTargets(st, P1)) << "it chose nothing";
        EXPECT_FALSE(repulse->hasLegalTargets(st, P1));
        // Control: once the ability really chooses my unit — and is finalized:
        // a counter chooses finalized items only (CR 355.9.a.2; testing phase
        // D21) — both can answer it.
        top.chosen_objects = {mine};
        top.status = ChainItemStatus::Finalized;
        EXPECT_TRUE(nsf->hasLegalTargets(st, P1));
        EXPECT_TRUE(repulse->hasLegalTargets(st, P1));
        st.chain.items.clear();
    });
}

// batch-10 728 Stalking Wolf, the Ambush half: the unit to be killed is still
// at its battlefield while play locations are offered.
TEST_F(VerifyCardsTest, Pin_Batch10_StalkingWolf728_CanBePlayedToItsVictimsBattlefield) {
    auto wolf = addToHand(P1, kStalkingWolf);
    auto poro = addUnit(P1, kInvalidId, 1, 0);       // my only unit at battlefield #0
    o(poro).tags = {"Poro"};
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    bool at_bf0 = false, at_bf1 = false;
    // [Ambush] is a play location at Reaction timing (CR 822.1.b): ask what
    // P1 may do while holding priority in a Closed State.
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.priority_holder = P1;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions()) {
            if (i.card != wolf || !i.play_location) continue;
            if (*i.play_location == LocationId{BattlefieldLocation{0}}) at_bf0 = true;
            if (*i.play_location == LocationId{BattlefieldLocation{1}}) at_bf1 = true;
        }
    });
    state.turn.oc_state = OpenClosedState::Open;
    state.turn.priority_holder = std::nullopt;
    EXPECT_TRUE(at_bf0) << "[Ambush] to its battlefield";
    EXPECT_FALSE(at_bf1) << "no unit of mine there";
    EnginePathAgent agent(EnginePathAgent::choosing(poro));
    playThroughEngine(wolf, &agent, LocationId{BattlefieldLocation{0}}, nullptr, /*fund=*/false);
    EXPECT_EQ(bfOf(wolf), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "even if you don't have other units there";
    EXPECT_FALSE(onBoard(poro));
}

// batch-10 Systemic S3: two Ezreal, Dashing — "I don't deal combat damage"
// is applied once to each.
TEST_F(VerifyCardsTest, Pin_Batch10_EzrealDashing404_TwoCopiesEachSuppressedOnce) {
    auto a = addUnit(P1, kEzrealDashing, 0, 0);
    auto b = addUnit(P1, kEzrealDashing, 0, 1);
    auto other = addUnit(P1, kShipyardSkulker, 0, 0);
    cleanup();
    auto suppressions = [&](GameObjectId id) {
        int n = 0;
        for (const auto& ae : o(id).aura_effects) if (ae.suppress_combat_damage) ++n;
        return n;
    };
    EXPECT_EQ(suppressions(a), 1);
    EXPECT_EQ(suppressions(b), 1);
    EXPECT_EQ(suppressions(other), 0);
}

// batch-08 671 / 783 / 274: Matriarch of War 940 was the correct comparison —
// "Ready a unit." is any unit.
TEST_F(VerifyCardsTest, Pin_Batch08_MatriarchOfWar940_ReadyAUnitOffersEitherPlayers) {
    auto legend = addLegend(P1, kMatriarchOfWar);
    o(legend).empowered = 1;
    auto mine = addUnit(P1, kShipyardSkulker, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 0);
    o(mine).is_exhausted = true;
    o(theirs).is_exhausted = true;
    ps(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    activate(legend, &agent);
    EXPECT_TRUE(agent.wasOffered(mine));
    EXPECT_TRUE(agent.wasOffered(theirs));
    EXPECT_FALSE(o(theirs).is_exhausted) << "the enemy unit was readied";
    EXPECT_TRUE(o(mine).is_exhausted);
    EXPECT_FALSE(o(legend).isEmpowered()) << "Disempower me";
}

// 07 item 7.35: Noxian Demolitionist 867 was named as the guarded example —
// a gear token with no card behind it costs [0] and is looked up nowhere.
TEST_F(VerifyCardsTest, Pin_7_35_NoxianDemolitionist867_AGearTokenWithoutACardCostsNothing) {
    auto demo = addUnit(P1, kNoxianDemolitionist, 1, 0);
    auto token = addGear(P2, kInvalidId);
    o(token).is_token = true;
    auto dear = addGear(P2, kShurelyasRequiem);      // [4]: more than 1 [M]
    EnginePathAgent agent(EnginePathAgent::choosing(token, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(demo, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(token));
    EXPECT_FALSE(agent.wasOffered(dear)) << "Energy cost no more than my Might";
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(dear));
}

// 02 item 2.20: Forgotten Monument 523 — before a player's third turn there
// is no score at all: no ScoreEvent, no point, no conquer trigger.
TEST_F(VerifyCardsTest, Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger) {
    BattlefieldCard* monument = dynamic_cast<BattlefieldCard*>(card_registry.get(kForgottenMonument));
    ASSERT_NE(monument, nullptr);
    state.battlefields[0].min_turn_to_score = monument->minTurnToScore();
    addUnit(P1, kPlunderingPoro392, 0, 0);            // "When I conquer, play a Gold..."
    int scores = 0;
    auto conn = events.on_score.connect([&](const ScoreEvent&) { ++scores; });
    auto conquer = [&]() {
        withEngine([&](GameEngine& e) {
            e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0));
            e.testHook_runChain();
        });
    };
    ps(P1).turns_taken = 2;
    conquer();
    EXPECT_EQ(scores, 0) << "Players can't score here until their third turn";
    EXPECT_EQ(ps(P1).score, 0);
    EXPECT_EQ(countNamed(P1, "Gold"), 0) << "no conquer happened, so nothing triggers";

    ps(P1).turns_taken = 3;
    conquer();
    conn.disconnect();
    EXPECT_EQ(scores, 1);
    EXPECT_EQ(countNamed(P1, "Gold"), 1);
}

// ═══ Work items 1 and 4: missing accept paths and tests that could not fail ═

// 03 item 3.7: Guards! 475 — the "may" re-enters the callback after the token
// exists; one token, readied, [Y] spent.
TEST_F(VerifyCardsTest, Guards_AcceptingPaysOrderPowerAndReadiesTheOneSandSoldier) {
    auto spell = addToHand(P1, kGuards);
    power(P1, Domain::Order) = 1;
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));   // to base; yes, pay
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "where, then the may";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1) << "one token however often it re-enters";
    EXPECT_FALSE(o(firstNamed(P1, "Sand Soldier")).is_exhausted);
    EXPECT_EQ(power(P1, Domain::Order), 0);
}

TEST_F(VerifyCardsTest, Guards_WithoutOrderPowerTheMayIsNotOffered) {
    auto spell = addToHand(P1, kGuards);
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "where to play it; the may is not offered";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Sand Soldier")).is_exhausted);
}

// 03 item 3.12 / 07 item 7.5: the cost is paid as the trigger is finalized —
// seen from the opponent's first priority, before anything resolves.
TEST_F(VerifyCardsTest, FaePorter_ThePowerIsGoneBeforeTheOpponentCanRespond) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, EnginePathAgent::May::Yes));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
        EXPECT_TRUE(live.getObject(ally).isAtBase()) << "not moved yet";
    });
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(VerifyCardsTest, RumbleHotheaded_TheUnitIsRecycledBeforeTheOpponentCanRespond) {
    auto rumble = addUnit(P1, kRumbleHotheaded, 0, 0);
    auto fodder = addUnit(P1, kShipyardSkulker, 7);
    auto mech = addToTrash(P1, kMegaMech);
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(fodder.raw()), static_cast<int>(mech.raw())}));
    fireAndProbe(rumble, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(fodder).location.has_value()) << "recycled as the cost";
        EXPECT_FALSE(live.getObject(mech).location.has_value()) << "not played yet";
    });
    EXPECT_TRUE(inDeck(P1, fodder));
    EXPECT_TRUE(onBoard(mech));
}

TEST_F(VerifyCardsTest, OverzealousFan_IsDeadBeforeTheOpponentCanRespond) {
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 0, 0);
    auto& b = state.battlefields[0];
    b.combat_in_progress = true;
    b.attacker = P2;
    b.defender = P1;
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker.raw());
    EnginePathAgent agent(EnginePathAgent::choosing(attacker, EnginePathAgent::May::Yes));
    fireAndProbe(fan, TriggerType::WhenIDefend, &agent, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(fan).location.has_value()) << "\"kill me\" is the cost";
        EXPECT_FALSE(live.getObject(attacker).isAtBase()) << "not moved yet";
    });
    EXPECT_TRUE(o(attacker).isAtBase());
}

// 03 item 3.14: a source that left the board is not moved back.
TEST_F(VerifyCardsTest, Blitzcrank_HoldReturnsHimToHandOnlyFromTheBoard) {
    auto blitz = addUnit(P1, kBlitzcrank, 0, 0);
    fireTriggerThroughEngine(blitz, TriggerType::WhenIHold);
    EXPECT_TRUE(inHand(P1, blitz));

    auto dead = addUnit(P1, kBlitzcrank, 0, 0);
    sendToTrash(dead);
    fireTriggerThroughEngine(dead, TriggerType::WhenIHold);
    EXPECT_FALSE(inHand(P1, dead)) << "he was in the trash: not also put into hand";
    EXPECT_TRUE(inTrash(P1, dead));
    EXPECT_EQ(o(dead).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, EzrealDashing_ActivatedThenKilledIsNotMovedToBase) {
    auto ezreal = addUnit(P1, kEzrealDashing, 0, 0);
    power(P1, Domain::Mind) = 1;
    GameState* live = nullptr;
    bool killed = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!killed && live) {                        // in response: he dies
            killed = true;
            auto& e = live->getObject(ezreal);
            e.location = std::nullopt;
            e.zone = ZoneType::Trash;
            live->player(P1).trash.push_back(ezreal);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = ezreal;
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(act);
        live = nullptr;
    }, nullptr, &p2);
    ASSERT_TRUE(killed) << "the opponent never got priority";
    EXPECT_EQ(power(P1, Domain::Mind), 0) << "the ability was activated and paid for";
    EXPECT_FALSE(onBoard(ezreal)) << "a card in the trash is not moved to base";
    EXPECT_EQ(o(ezreal).zone, ZoneType::Trash);
}

TEST_F(VerifyCardsTest, EzrealDashing_ActivatedMovesHimToBase) {
    auto ezreal = addUnit(P1, kEzrealDashing, 0, 0);
    power(P1, Domain::Mind) = 1;
    activate(ezreal);
    EXPECT_TRUE(o(ezreal).isAtBase());
}

// 03 item 3.16: through the engine — the played copy does not count itself,
// another copy does.
TEST_F(VerifyCardsTest, Direwing_ThroughTheEngine_EntersReadyOnlyNextToAnotherDragon) {
    auto first = addToHand(P1, kDirewing);
    playThroughEngine(first);
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted) << "alone: it is not \"another Dragon\" for itself";
    auto second = addToHand(P1, kDirewing);
    playThroughEngine(second);
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted) << "another Direwing is another Dragon";
}

TEST_F(VerifyCardsTest, BreakneckMech_ThroughTheEngine_EntersReadyOnlyNextToAnotherMech) {
    auto first = addToHand(P1, kBreakneckMech);
    playThroughEngine(first);
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted);
    auto second = addToHand(P1, kBreakneckMech);
    playThroughEngine(second);
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted);
}

// 07 item 7.15: Glasc Mixologist 486 — the accept path.
TEST_F(VerifyCardsTest, GlascMixologist_AcceptingPlaysTheChosenCheapUnitFromTheTrash) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    auto dear = addToTrash(P1, kMegaMech);            // [7]
    EnginePathAgent agent(EnginePathAgent::choosing(cheap, EnginePathAgent::May::Yes));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(dear)) << "no more than [3]";
    EXPECT_TRUE(onBoard(cheap));
    EXPECT_FALSE(inTrash(P1, cheap));
}

TEST_F(VerifyCardsTest, GlascMixologist_DecliningLeavesTheUnitInTheTrash) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inTrash(P1, cheap));
}

// 07 item 7.24: the pick is neither the first nor the last of three, so an
// implementation that prompts and then takes its own favourite fails.
TEST_F(VerifyCardsTest, FizzTrickster_PlaysTheMiddleOfThreeSpells) {
    auto fizz = addToHand(P1, kFizzTrickster);
    auto oldest = addToTrash(P1, kMeditation);
    auto middle = addToTrash(P1, kMobilize);
    auto newest = addToTrash(P1, kMeditation);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.universal_power = 3;
    EnginePathAgent agent(EnginePathAgent::choosing(middle, EnginePathAgent::May::Yes));
    playThroughEngine(fizz, &agent);
    EXPECT_TRUE(agent.wasOffered(oldest));
    EXPECT_TRUE(agent.wasOffered(newest));
    EXPECT_TRUE(inTrash(P1, oldest));
    EXPECT_TRUE(inTrash(P1, newest));
    EXPECT_FALSE(inTrash(P1, middle));
    EXPECT_TRUE(inDeck(P1, middle)) << "Then recycle it.";
}

TEST_F(VerifyCardsTest, KaiSaEvolutionary_PlaysTheMiddleOfThreeSpells) {
    auto kaisa = addUnit(P1, kKaiSaEvolutionary, 0, 0);
    setScore(P1, 5);
    auto oldest = addToTrash(P1, kMeditation);
    auto middle = addToTrash(P1, kMobilize);
    auto newest = addToTrash(P1, kMeditation);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.universal_power = 3;
    EnginePathAgent agent(EnginePathAgent::choosing(middle, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(kaisa, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.wasOffered(oldest));
    EXPECT_TRUE(agent.wasOffered(newest));
    EXPECT_TRUE(inTrash(P1, oldest));
    EXPECT_TRUE(inTrash(P1, newest));
    EXPECT_FALSE(inTrash(P1, middle));
    EXPECT_TRUE(inDeck(P1, middle)) << "Then recycle it.";
}

// Work item 4: Loyal Pup 447 and Shadow Clone 955 — accept and decline.
TEST_F(VerifyCardsTest, LoyalPup_AcceptingMovesItToTheDefendedBattlefield) {
    auto pup = addUnit(P1, kLoyalPup, 0);
    o(pup).card_counters["__defend_bf"] = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(pup, TriggerType::WhenYouDefendAtABattlefield, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_EQ(bfOf(pup), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, LoyalPup_DecliningLeavesItInBase) {
    auto pup = addUnit(P1, kLoyalPup, 0);
    o(pup).card_counters["__defend_bf"] = 1;
    EnginePathAgent no(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(pup, TriggerType::WhenYouDefendAtABattlefield, &no);
    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_TRUE(o(pup).isAtBase());
}

TEST_F(VerifyCardsTest, ShadowClone_AcceptingBanishesTheChosenUnitAndGivesAssault) {
    auto clone = addUnit(P1, kShadowClone, 0, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(corpse, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(clone, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(inBanishment(P1, corpse));
    EXPECT_FALSE(inTrash(P1, corpse));
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
    EXPECT_TRUE(o(clone).hasKeyword(Keyword::Assault));
}

TEST_F(VerifyCardsTest, ShadowClone_DecliningBanishesNothingAndGivesNoAssault) {
    auto clone = addUnit(P1, kShadowClone, 0, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(clone, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inTrash(P1, corpse));
    EXPECT_FALSE(o(clone).hasKeyword(Keyword::Assault));
}

// ═══ Round 2: tests that could not fail, and accept paths never taken ═══════

// 03 item 3.14: Azir, Ascendant — he left the board in response: nobody moves.
TEST_F(VerifyCardsTest, AzirAscendant_KilledInResponseSwapsNobody) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    executeAndProbe(act, nullptr, [&](GameState& live) { removeFromBoard(live, azir); });
    EXPECT_EQ(power(P1, Domain::Calm), 0) << "activated and paid for";
    EXPECT_FALSE(onBoard(azir));
    EXPECT_EQ(o(azir).zone, ZoneType::Trash);
    EXPECT_EQ(bfOf(other), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)}) << "it stays where it was";
}

TEST_F(VerifyCardsTest, AzirAscendant_SwapsPlacesWithTheChosenUnit) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    executeAndProbe(act, nullptr, [](GameState&) {});
    EXPECT_EQ(bfOf(azir), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_TRUE(o(other).isAtBase());
}

// 03 item 3.15: Pirate's Haven — the readied unit must still be on the board.
TEST_F(VerifyCardsTest, PiratesHaven_AUnitThatLeftTheBoardGetsNoMight) {
    auto haven = addGear(P1, kPiratesHaven);
    auto here = addUnit(P1, kShipyardSkulker, 0);
    fireTriggerThroughEngine(haven, TriggerType::WhenYouReadyAFriendlyUnit, nullptr, here);
    EXPECT_EQ(o(here).temp_might_bonus, 1);
    auto gone = addUnit(P1, kShipyardSkulker, 0);
    sendToTrash(gone);
    fireTriggerThroughEngine(haven, TriggerType::WhenYouReadyAFriendlyUnit, nullptr, gone);
    EXPECT_EQ(o(gone).temp_might_bonus, 0);
}

// 03 item 3.24: the shared splitDamageAmong, through the engine.
TEST_F(VerifyCardsTest, AlphaStrike_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt) {
    auto striker = addUnit(P1, kInvalidId, 4);
    auto e1 = addUnit(P2, kInvalidId, 3, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 1);
    auto in_base = addUnit(P2, kInvalidId, 2);
    auto spell = addToHand(P1, kAlphaStrike);
    EnginePathAgent agent(EnginePathAgent::choosing(e1));
    playAt(spell, {striker}, &agent);
    EXPECT_FALSE(agent.wasOffered(in_base)) << "enemy units at battlefields";
    EXPECT_FALSE(onBoard(e1)) << "all 4 on the 3 [M] unit kills it";
    EXPECT_EQ(o(e2).damage_marked, 0);
    EXPECT_EQ(ps(P1).xp, 1);
}

TEST_F(VerifyCardsTest, VolibearFurious_ThroughTheEngine_AllTheDamageWhereTheControllerPutsIt) {
    auto voli = addUnit(P1, kVolibearFurious, 0, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    auto small = addUnit(P2, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 2, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(big));
    fireTriggerThroughEngine(voli, TriggerType::WhenIAttack, &agent);
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "here";
    EXPECT_EQ(o(big).damage_marked, 5);
    EXPECT_EQ(o(small).damage_marked, 0);
}

// 07 item 7.5: Ravenbloom Prefect — "banish me" is paid as the trigger is
// finalized: there is a moment when she is banished and the gear is not yet.
TEST_F(VerifyCardsTest, RavenbloomPrefect_IsBanishedBeforeTheGearIs) {
    auto prefect = addUnit(P2, kRavenbloomPrefect, 0, 0);
    auto gear = addToHand(P1, kSealOfStrength163);
    GameEngine* live = nullptr;
    bool seen_cost_paid_first = false;
    PickByPredicateAgent me([&](int, const std::vector<Intent>& legal) {
        if (live) {
            const auto& st = live->mutableState();
            if (!st.getObject(prefect).location.has_value() &&
                st.getObject(gear).location.has_value()) seen_cost_paid_first = true;
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    EnginePathAgent them(EnginePathAgent::script({1}));
    const auto& def = card_db.get(kSealOfStrength163);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = gear;
    play.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executeIntent(play);
        live = nullptr;
    }, &me, &them);
    ASSERT_EQ(them.prompts.size(), 1u);
    EXPECT_TRUE(seen_cost_paid_first) << "the cost is paid before anyone can respond";
    EXPECT_TRUE(inBanishment(P2, prefect));
    EXPECT_TRUE(inBanishment(P1, gear));
}

// 07 item 7.8: Relentless Storm — the Mighty check is made when the unit is
// played; losing the unit in response does not cost the rune.
TEST_F(VerifyCardsTest, RelentlessStorm_StillChannelsWhenTheMightyUnitLeavesInResponse) {
    auto voli = addLegend(P1, kRelentlessStorm);
    auto rune = addRune(P1, Domain::Fury);
    o(rune).location = std::nullopt;
    o(rune).zone = ZoneType::RuneDeck;
    ps(P1).rune_deck.push_back(rune);
    auto mighty = unitInHand(P1, kMegaMech);
    ps(P1).rune_pool.energy += card_db.get(kMegaMech).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = mighty;
    play.play_location = LocationId{BaseLocation{P1}};
    EnginePathAgent yes(EnginePathAgent::script({1}));
    executeAndProbe(play, &yes, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(voli).is_exhausted) << "exhausted as the cost";
        removeFromBoard(live, mighty);
    });
    EXPECT_TRUE(ps(P1).rune_deck.empty()) << "the rune is channeled all the same";
    EXPECT_TRUE(onBoard(rune));
}

// 07 item 7.25: Scryer's Bloom — "Kill this" is a cost: paid before the
// opponent can respond, before any card is looked at.
TEST_F(VerifyCardsTest, ScryersBloom_IsKilledBeforeTheOpponentCanRespond) {
    auto bloom = addGear(P1, kScryersBloom);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kInvalidId);
    ps(P1).rune_pool.energy = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = bloom;
    executeAndProbe(act, nullptr, [&](GameState& live) {
        EXPECT_FALSE(live.getObject(bloom).location.has_value()) << "Kill this: the cost";
        EXPECT_TRUE(live.player(P1).hand.empty()) << "nothing drawn yet";
        EXPECT_EQ(live.player(P1).xp, 0);
    });
    EXPECT_EQ(handSize(P1), 1);
}

// 07 item 7.26: Vayne, Hunter 35 — as reprint 537.
TEST_F(VerifyCardsTest, Vayne035_OffTheBoardIsNotOfferedTheReturnAndPaysNothing) {
    auto vayne = addUnit(P1, kVayne035, 0, 0);
    sendToTrash(vayne);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(vayne, TriggerType::WhenIConquer, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_FALSE(inHand(P1, vayne));
}

TEST_F(VerifyCardsTest, Vayne035_KilledInResponseStaysInTheTrash) {
    auto vayne = addUnit(P1, kVayne035, 0, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(vayne, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.energy, 0);
        removeFromBoard(live, vayne);
    });
    EXPECT_FALSE(inHand(P1, vayne));
    EXPECT_EQ(o(vayne).zone, ZoneType::Trash);
}

// 07 item 7.27: Carnivorous Snapvine — a Snapvine that went back to hand
// neither damages nor kills. (The enemy has more Might than the Snapvine, so
// "damage 0" cannot be a killed-and-reset unit.)
TEST_F(VerifyCardsTest, Snapvine_ReturnedToHandInResponseFightsNobody) {
    auto enemy = addUnit(P2, kInvalidId, 9, 0);
    auto vine = addUnit(P1, kSnapvine);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(vine, P1, TriggerType::WhenYouPlayMe);
        e.testHook_executor().bounceToHand(vine);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_EQ(o(enemy).damage_marked, 0);
    EXPECT_TRUE(inHand(P1, vine));
    EXPECT_FALSE(inTrash(P1, vine));
}

// 07 item 7.28: Falling Star — both hits on one token: it dies once.
TEST_F(VerifyCardsTest, FallingStar_BothHitsOnOneTokenKillItOnce) {
    auto spell = addToHand(P1, kFallingStar);
    auto token = addUnit(P2, kInvalidId, 3, 0);
    o(token).is_token = true;
    o(token).super_type = SuperType::Token;
    int deaths = 0;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    playThroughEngine(spell, &agent);
    conn.disconnect();
    EXPECT_EQ(agent.prompts.size(), 2u) << "two picks";
    EXPECT_FALSE(onBoard(token));
    EXPECT_EQ(deaths, 1) << "the second hit finds nothing on the board";
    EXPECT_FALSE(inTrash(P2, token)) << "a token ceases to exist";
}

// 07 item 7.31: Void Burrower — the banish goes through the executor.
TEST_F(VerifyCardsTest, VoidBurrower_ThroughTheEngine_TheBanishIsCountedAndAnnounced) {
    auto legend = addLegend(P1, kVoidBurrower);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    o(top).base_might = o(top).current_might = 3;
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    int announced = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "banished_owned") ++announced;
    });
    EnginePathAgent agent(EnginePathAgent::choosing(top, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    conn.disconnect();
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
    EXPECT_EQ(announced, 1);
    EXPECT_TRUE(onBoard(top)) << "banish one, then play it";
    EXPECT_FALSE(inBanishment(P1, top));
    EXPECT_TRUE(inDeck(P1, below)) << "recycle the rest";
}

// 07 item 7.36: Elder Dragon — every chosen unit is damaged before any dies.
TEST_F(VerifyCardsTest, ElderDragon_DamagesEveryChosenUnitBeforeAnyDies) {
    auto dragon = addUnit(P1, kElderDragon, 0, 0);
    auto in_base = addUnit(P2, kInvalidId, 1);
    auto at_bf = addUnit(P2, kInvalidId, 1, 0);
    GameEngine* live = nullptr;
    int deaths = 0;
    bool all_damaged_at_first_death = false;
    auto conn = events.on_unit_died.connect([&](const UnitDiedEvent&) {
        if (++deaths != 1 || !live) return;
        const auto& st = live->mutableState();
        all_damaged_at_first_death =
            (!st.getObject(in_base).location.has_value() || st.getObject(in_base).damage_marked >= 1) &&
            (!st.getObject(at_bf).location.has_value() || st.getObject(at_bf).damage_marked >= 1);
    });
    // One prompt per location: take the unit offered each time.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent);
    conn.disconnect();
    EXPECT_EQ(deaths, 2);
    EXPECT_FALSE(onBoard(in_base));
    EXPECT_FALSE(onBoard(at_bf));
    EXPECT_TRUE(all_damaged_at_first_death) << "damage first, kills in a second pass";
}

// 07 item 7.6: Stalking Wolf — the engine consults the gate for a unit play.
TEST_F(VerifyCardsTest, StalkingWolf_TheEngineRefusesThePlayWithNothingToKill) {
    auto wolf = addToHand(P1, kStalkingWolf);
    addUnit(P1, kShipyardSkulker, 0);                 // a Pirate: not a Bird, Cat, Dog or Poro
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    auto offered = [&]() {
        bool found = false;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.type == IntentType::PlayCard && i.card == wolf) found = true;
            EXPECT_EQ(e.testHook_canAfford(P1, wolf), found);
        });
        return found;
    };
    EXPECT_FALSE(offered()) << "the additional cost can't be paid";
    auto poro = addUnit(P1, kInvalidId, 1);
    o(poro).tags = {"Poro"};
    EXPECT_TRUE(offered());
}

// ── Accept paths the stage 2 tests never took ──

// 513 Arise!: "Ready up to two of them."
TEST_F(VerifyCardsTest, Arise_ReadyingTwoOfThreeSandSoldiers) {
    auto spell = addToHand(P1, kArise);
    for (int i = 0; i < 3; ++i) {
        auto g = addGear(P1, kSerratedDirk);
        ASSERT_TRUE(isEquipment(o(g)));
    }
    // Take a Sand Soldier whenever one is offered.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    int ready = 0, exhausted = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Sand Soldier" || !obj.location.has_value()) continue;
        if (obj.is_exhausted) ++exhausted; else ++ready;
    }
    EXPECT_EQ(ready, 2) << "up to two";
    EXPECT_EQ(exhausted, 1);
}

// 224 Salvage: "You may kill up to one gear. Draw 1."
TEST_F(VerifyCardsTest, Salvage_KillingTheChosenGearAndDrawing) {
    auto my_gear = addGear(P1, kSerratedDirk);
    auto other = addGear(P1, kSerratedDirk);
    addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kSalvage);
    playAt(spell, {my_gear});
    EXPECT_FALSE(onBoard(my_gear));
    EXPECT_TRUE(onBoard(other));
    EXPECT_EQ(handSize(P1), 1);
}

// 286 The Candlelit Sanctum: "You may recycle one or both of them."
TEST_F(VerifyCardsTest, CandlelitSanctum_RecyclingOneOfTheTwo) {
    auto sanctum = battlefieldCard(0, kCandlelitSanctum);
    auto third = addToDeck(P1, kRecruit);
    auto second = addToDeck(P1, kRecruit);
    auto top = addToDeck(P1, kRecruit);
    int recycled_events = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "recycled_main") ++recycled_events;
    });
    // Recycle the first card asked about, keep the second.
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const bool recycle = asked++ == 0;
        for (const auto& i : legal)
            if (i.chosen_objects.empty() != recycle) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(sanctum, TriggerType::WhenYouConquerHere, &agent);
    conn.disconnect();
    ASSERT_EQ(deckSize(P1), 3);
    EXPECT_EQ(recycled_events, 1);
    const auto& deck = ps(P1).main_deck;
    const GameObjectId recycled = deck.front();
    EXPECT_TRUE(recycled == top || recycled == second) << "one of the two looked at";
    EXPECT_EQ(deck.back(), recycled == top ? second : top) << "the kept one is back on top";
    EXPECT_EQ(deck[1], third);
}

// 671 Blood Rose: "When you play a unit, you may pay [1] to gain 1 XP."
TEST_F(VerifyCardsTest, BloodRose_PayingOneForAnXpWhenAUnitIsPlayed) {
    addGear(P1, kBloodRose);
    auto unit = unitInHand(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    playThroughEngine(unit, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_EQ(ps(P1).xp, 1);
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);

    auto second = unitInHand(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(second, &no);
    EXPECT_EQ(no.prompts.size(), 1u);
    EXPECT_EQ(ps(P1).xp, 1) << "declined: no XP";
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "and nothing paid";
}

// 07 item 7.34: Forge of the Fluft — the Equipment and the unit are chosen as
// the ability is activated (kFinalizeActivate), so they are on the chain item
// before anyone can respond.
TEST_F(VerifyCardsTest, ForgeOfTheFluft_TheEquipmentAndUnitAreChosenAtActivation) {
    battlefieldCard(0, kForgeOfTheFluft);
    addUnit(P1, kInvalidId, 1, 0);                    // holds the battlefield
    auto lg = addLegend(P1, kPurifier);               // any legend
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    attach(sword, a);
    cleanup();
    ASSERT_EQ(o(lg).granted_abilities.size(), 1u);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = lg;
    act.granted_ability_def = kForgeOfTheFluft;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(sword.raw()), static_cast<int>(b.raw())}));
    executeAndProbe(act, &agent, [&](GameState& live) {
        ASSERT_FALSE(live.chain.items.empty());
        const auto& chosen = live.chain.items.back().chosen_objects;
        EXPECT_NE(std::find(chosen.begin(), chosen.end(), sword), chosen.end());
        EXPECT_NE(std::find(chosen.begin(), chosen.end(), b), chosen.end());
        EXPECT_EQ(live.getObject(sword).attached_to, std::optional<GameObjectId>{a})
            << "not moved until it resolves";
    });
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{b});
}

// 08 S2 / 07 item 7.26: units whose own ability acts on themselves ("give me
// +N [M]", "buff me", "ready me", "move me") do nothing once they have left
// the board — whichever of their triggers is on the chain, and even when the
// controller says yes to everything.
TEST_F(VerifyCardsTest, S2_ASourceThatLeftTheBoardDoesNothingToItself) {
    const CardDefId ids[] = {27, 36, 56, 59, 103, 131, 167, 177, 197, 202, 304, 343,
                             380, 398, 434, 458, 590, 681};
    for (CardDefId def_id : ids) {
        SCOPED_TRACE("card " + toString(def_id) + " " + card_db.get(def_id).name);
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        for (TriggerType t : card->triggerTypes()) {
            if (t == TriggerType::None || t == TriggerType::Activated) continue;
            // A fresh board each time: the source in the trash, things around
            // for it to act on, and the means to pay for anything.
            auto source = addUnit(P1, def_id, 0, 0);
            auto ally = addUnit(P1, kShipyardSkulker, 0, 0);
            auto enemy = addUnit(P2, kShipyardSkulker, 9, 0);
            sendToTrash(source);
            ps(P1).rune_pool.energy = 5;
            ps(P1).rune_pool.universal_power = 5;
            ps(P1).xp = 5;
            const KeywordSet before = o(source).keywords;
            EnginePathAgent yes([](const std::vector<Intent>& legal) {
                for (const auto& i : legal)
                    if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
                return legal.front();
            });
            fireTriggerThroughEngine(source, t, &yes, ally);
            EXPECT_FALSE(onBoard(source)) << "not moved back onto the board";
            EXPECT_EQ(o(source).zone, ZoneType::Trash);
            EXPECT_EQ(o(source).temp_might_bonus, 0);
            EXPECT_EQ(o(source).buff_count, 0);
            EXPECT_TRUE(o(source).keywords == before);
            EXPECT_TRUE(o(source).attachments.empty());
            EXPECT_EQ(o(enemy).damage_marked, 0) << "its Might is not used from the trash";
            EXPECT_TRUE(onBoard(enemy));
            // Clear the board for the next round.
            for (auto id : {source, ally, enemy}) {
                o(id).location = std::nullopt;
                o(id).zone = ZoneType::Banishment;
            }
            ps(P1).trash.clear();
        }
    }
}

// 08 S2: Akshan, Mischievous — once he has left the board nothing is taken,
// and the enemy Equipment is not stripped from its bearer either.
TEST_F(VerifyCardsTest, Akshan_OffTheBoardLeavesTheEnemyEquipmentWhereItIs) {
    auto akshan = addUnit(P1, kAkshan, 0, 0);
    auto bearer = addUnit(P2, kShipyardSkulker, 0, 1);
    auto sword = addGear(P2, kLongSword);
    attach(sword, bearer);
    o(akshan).card_counters["__akshan_paid"] = 1;
    sendToTrash(akshan);
    EnginePathAgent agent(EnginePathAgent::choosing(sword));
    fireTriggerThroughEngine(akshan, TriggerType::AsYouPlayMe, &agent);
    EXPECT_EQ(o(sword).controller, P2);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{bearer})
        << "not detached for a move that does not happen";
    EXPECT_EQ(o(bearer).attachments.size(), 1u);
}

// 08 S5: the pick is not the first option, so a first-match card fails.
TEST_F(VerifyCardsTest, StarSpring_MovesTheLaterOfTwoOtherUnits) {
    auto spring = battlefieldCard(0, kStarSpring);
    auto played = addUnit(P1, kShipyardSkulker, 0, 0);
    auto first = addUnit(P1, kShipyardSkulker, 0, 0);
    auto later = addUnit(P1, kShipyardSkulker, 0, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(later, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &agent, played);
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_TRUE(o(later).isAtBase());
    EXPECT_TRUE(o(first).isAtBattlefield());
    EXPECT_TRUE(o(played).isAtBattlefield());
}

TEST_F(VerifyCardsTest, AssemblyRig_RecyclesTheSecondUnitWhenThatIsThePick) {
    auto rig = addGear(P1, kAssemblyRig);
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Fury) = 1;
    auto u1 = addToTrash(P1, kShipyardSkulker);
    auto u2 = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::choosing(u2));
    activate(rig, &agent);
    EXPECT_TRUE(agent.wasOffered(u1));
    EXPECT_TRUE(inDeck(P1, u2));
    EXPECT_TRUE(inTrash(P1, u1));
    EXPECT_EQ(countNamed(P1, "Mech"), 1);
}


// ═══ Tranche 2, group A ═══════════════════════════════════════════════════════
// ═══ Fixer A: sweep items 191 / 695 / 147 / 765 / 449 / 21 / 680 / 446 / 373 / 855 / 406 ═══

// 191 Maddened Marauder: a unit that can't be moved to base is not a choice.
TEST_F(VerifyCardsTest, MaddenedMarauder_DoesNotOfferAUnitThatCannotBeMovedToBase) {
    auto marauder = addUnit(P1, kMaddenedMarauder);
    state.battlefields[0].blocks_move_to_base = true;    // as Vilemaw's Lair sets it
    auto stuck = addUnit(P2, kShipyardSkulker, 0, 0);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 1);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets
    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent;
    fireTriggerThroughEngine(marauder, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(agent.wasOffered(stuck)) << "units can't move from battlefield #0 to base";
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_TRUE(o(free_unit).isAtBase());
    EXPECT_EQ(bfOf(stuck), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// 695 Blast Cone: an enemy unit enemy abilities can't move is not a choice.
TEST_F(VerifyCardsTest, BlastCone_DoesNotOfferAnEnemyUnitItCannotMove) {
    auto cone = addGear(P1, kBlastCone);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 0);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets
    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 0);
    // Yes, the unit, to its base (mode 0); the stun "may" that follows is declined.
    EnginePathAgent agent(EnginePathAgent::script({
        1,                              // "may move an enemy unit": yes
        static_cast<int>(free_unit.raw()),    // which enemy unit
        0,                              // where: mode 0 = its base (mode 2 = battlefield #1)
        0,                              // the move triggers "you may exhaust this to [Stun] it": no
    }));
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_TRUE(o(free_unit).isAtBase());
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(VerifyCardsTest, BlastCone_WithOnlyAnUnmovableEnemyAsksNothing) {
    auto cone = addGear(P1, kBlastCone);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 0);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;
    EnginePathAgent agent(EnginePathAgent::accepting());
    fireTriggerThroughEngine(cone, TriggerType::WhenYouPlayThis, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "no enemy unit it could move: the \"may\" is not offered";
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// 147 Wildclaw Shaman: spending a unit's buff is paying a cost, not choosing
// the unit — a "when you choose me" ability does not see it.
TEST_F(VerifyCardsTest, WildclawShaman_SpendingABuffDoesNotChooseTheUnit) {
    auto irelia = addUnit(P1, kIreliaFervent);
    o(irelia).buff_count = 1;
    o(irelia).recomputeMight();
    auto plain = addUnit(P1, kShipyardSkulker);
    o(plain).buff_count = 1;
    o(plain).recomputeMight();
    auto shaman = addToHand(P1, kWildclawShaman);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(irelia.raw())}));
    playThroughEngine(shaman, &agent);
    EXPECT_TRUE(agent.wasOffered(plain)) << "the controller picks whose buff is spent";
    EXPECT_EQ(o(irelia).buff_count, 0) << "\"spend a buff\"";
    EXPECT_EQ(o(plain).buff_count, 1);
    EXPECT_EQ(o(irelia).temp_might_bonus, 0)
        << "\"When you choose ... me\" did not trigger: a cost is not a choice";
    EXPECT_EQ(o(shaman).buff_count, 1);
    EXPECT_FALSE(o(shaman).is_exhausted);
}

// 765 Dusk Rose Lab: the kill was replaced as the cost was paid — no draw, and
// the cost is not attempted again as the ability resolves.
TEST_F(VerifyCardsTest, DuskRoseLab_AReplacedKillIsNotTriedAgainAtResolution) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);   // P1 keeps units (and control) here
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).death_replacement_recall_pending = true;     // one-shot: recalled instead of dying
    EnginePathAgent agent(EnginePathAgent::choosing(unit, EnginePathAgent::May::Yes));   // yes, then which unit
    fireAndProbe(lab, TriggerType::AtStartOfBeginning, &agent, [&](GameState& live) {
        auto& u = live.getObject(unit);
        EXPECT_TRUE(u.isAtBase()) << "the death was replaced as the cost was paid";
        EXPECT_FALSE(u.death_replacement_recall_pending);
        // In response it comes back to the Lab: nothing protects it now.
        u.location = BattlefieldLocation{0};
        u.zone = ZoneType::BattlefieldZone;
    });
    EXPECT_TRUE(onBoard(unit)) << "the cost is attempted once, not again at resolution";
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 0) << "no kill, no draw";
}

// 449 Overzealous Fan: "kill me to ..." — the Fan's death was replaced, so the
// cost was not paid and the attacker stays.
TEST_F(VerifyCardsTest, OverzealousFan_SurvivingTheKillMovesNobody) {
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 0, 0);
    auto& b = state.battlefields[0];
    b.combat_in_progress = true;
    b.attacker = P2;
    b.defender = P1;
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker.raw());
    o(fan).death_replacement_recall_pending = true;      // recalled instead of dying
    EnginePathAgent agent(EnginePathAgent::choosing(attacker, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, the attacker";
    EXPECT_TRUE(onBoard(fan)) << "the death was replaced";
    EXPECT_EQ(bfOf(attacker), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "no kill, no move";
}

// 21 Sun Disc: unarmed, playing a unit puts no Sun Disc ability on the chain.
TEST_F(VerifyCardsTest, SunDisc_UnarmedDoesNotTriggerWhenAUnitIsPlayed) {
    auto disc = addGear(P1, kSunDisc);
    // Plays `card` from hand; true if the opponent ever had priority with a
    // Sun Disc ability on the chain.
    auto disc_ability_seen = [&](GameObjectId card) {
        bool saw = false;
        GameState* live = nullptr;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (live)
                for (const auto& ci : live->chain.items)
                    if (ci.source == disc) saw = true;
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        ps(P1).rune_pool.energy += card_db.get(o(card).card_def_id).energy_cost;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = card;
        play.play_location = LocationId{BaseLocation{P1}};
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executeIntent(play);
            e.testHook_runChain();
            live = nullptr;
        }, nullptr, &p2);
        return saw;
    };
    auto first = addToHand(P1, kRecruit);
    EXPECT_FALSE(disc_ability_seen(first)) << "not armed: the ability does not trigger";
    ASSERT_TRUE(onBoard(first));
    EXPECT_TRUE(o(first).is_exhausted);

    // Armed ([Legion]: a card was played this turn): it triggers for the next unit.
    ASSERT_GE(ps(P1).cards_played_this_turn, 1);
    activate(disc);
    auto second = addToHand(P1, kRecruit);
    EXPECT_TRUE(disc_ability_seen(second)) << "armed: the ability is on the chain";
    ASSERT_TRUE(onBoard(second));
    EXPECT_FALSE(o(second).is_exhausted) << "\"enters ready\"";
}

// 680 Elder Dragon: [Deflect] is owed for every unit chosen — one payable [A]
// buys one Deflect unit, not one per location.
TEST_F(VerifyCardsTest, ElderDragon_DeflectIsCountedAcrossLocations) {
    auto dragon = addUnit(P1, kElderDragon, 0, 1);
    auto in_base = addUnit(P2, kIreliaFervent);           // [Deflect]: [A] to choose
    auto at_bf = addUnit(P2, kIreliaFervent, 0, 0);
    power(P1, Domain::Body) = 1;                          // all P1 can pay
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(in_base.raw()), static_cast<int>(at_bf.raw())}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "after the first pick nothing is left to pay with";
    EXPECT_TRUE(agent.wasOffered(in_base));
    EXPECT_FALSE(agent.wasOffered(at_bf)) << "its [A] can't be paid on top of the first pick's";
    EXPECT_EQ(power(P1, Domain::Body), 0) << "the first pick's Deflect was paid";
    EXPECT_TRUE(!onBoard(in_base) || o(in_base).damage_marked >= 1) << "\"Deal 1 to them\"";
    ASSERT_TRUE(onBoard(at_bf));
    EXPECT_EQ(o(at_bf).damage_marked, 0) << "not chosen: its Deflect was never paid";
}

// 446 Fae Porter — NOT A DEFECT: "the same battlefield" is read off the Porter.
// Once it has left the board that information is gone and the move is ignored

// 373 Azir, Ascendant: "Use only once per turn" — an activation is a use even
// if the chosen unit is gone when it resolves.
TEST_F(VerifyCardsTest, AzirAscendant_AnActivationThatDoesNothingIsStillTheUseForTheTurn) {
    auto azir = addUnit(P1, kAzirAscendant, 0);
    auto other = addUnit(P1, kShipyardSkulker, 0, 1);
    auto spare = addUnit(P1, kShipyardSkulker, 0, 0);     // still there to swap with
    power(P1, Domain::Calm) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = azir;
    EnginePathAgent agent(EnginePathAgent::choosing(other));
    executeAndProbe(act, &agent, [&](GameState& live) {
        auto it = live.getObject(azir).card_counters.find("azir_used_turn");
        ASSERT_TRUE(it != live.getObject(azir).card_counters.end())
            << "used as the ability goes on the chain";
        EXPECT_EQ(it->second, live.turn.turn_number + 1);
        removeFromBoard(live, other);                     // in response: the chosen unit leaves
    });
    EXPECT_TRUE(o(azir).isAtBase()) << "nothing to swap with";
    EXPECT_EQ(bfOf(spare), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(azir).card_counters["azir_used_turn"], state.turn.turn_number + 1);
    const Card* card = card_registry.get(kAzirAscendant);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, azir, 0))
        << "\"Use only once per turn\": not offered again this turn";
}

// 855 Jayce, Brilliant Inventor: "ready something besides me that's exhausted"
// — not only his controller's things.
TEST_F(VerifyCardsTest, JayceBrilliantInventor_MayReadyAnEnemyObject) {
    auto jayce = addUnit(P1, kJayceBrilliantInventor);
    auto theirs = addUnit(P2, kShipyardSkulker, 0, 0);
    o(theirs).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::choosing(theirs, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then what to ready";
    EXPECT_TRUE(agent.wasOffered(theirs)) << "\"something\": no \"friendly\" in the text";
    EXPECT_FALSE(o(theirs).is_exhausted);
}

// 855: the gear trigger's choices are made again after "make new choices for
// it" (GameEngine::makeNewChoicesFor, reached as Rebuttal reaches it:
// EffectExecutor::makeNewChoices), and the ability is not lost because its
// "first gear this turn" stamp was already written when it went on the chain.
TEST_F(VerifyCardsTest, JayceBrilliantInventor_IsAskedAgainAfterNewChoicesAndStillResolves) {
    auto jayce = addUnit(P1, kJayceBrilliantInventor);
    auto first = addUnit(P1, kShipyardSkulker, 0);
    o(first).is_exhausted = true;
    auto second = addUnit(P1, kShipyardSkulker, 0);
    o(second).is_exhausted = true;
    auto gear = addGear(P1, kSerratedDirk);
    // Finalization: yes, `first`. After the new choices: yes, `second`.
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(first.raw()), 1, static_cast<int>(second.raw())}));
    GameEngine* live = nullptr;
    bool rechosen = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!rechosen && live && !live->mutableState().chain.items.empty()) {
            rechosen = true;
            auto& st = live->mutableState();
            live->testHook_executor().makeNewChoices(st.chain.items.back().id);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    // The play is announced by hand: count it as the announcement would
    // (testing phase B6: the condition is the turn's first non-token gear
    // play, PlayerState::nontoken_gear_plays_this_turn; the old test checked
    // a stamp of Jayce's own, which is gone).
    state.player(P1).nontoken_gear_plays_this_turn = 1;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(jayce, P1, TriggerType::WhenYouPlayACard, gear);
        e.testHook_runChain();
        live = nullptr;
    }, &agent, &p2);
    ASSERT_TRUE(rechosen) << "the opponent never got priority";
    EXPECT_EQ(agent.prompts.size(), 4u) << "may + what, asked at finalization and again";
    EXPECT_TRUE(o(first).is_exhausted) << "the first answer was forgotten";
    EXPECT_FALSE(o(second).is_exhausted) << "the re-chosen ability still resolved";

    // A second non-token gear this turn: not "the first time", nothing triggers.
    o(second).is_exhausted = true;
    auto gear2 = addGear(P1, kSerratedDirk);
    state.player(P1).nontoken_gear_plays_this_turn = 2;
    EnginePathAgent again(EnginePathAgent::accepting());
    fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayACard, &again, gear2);
    EXPECT_TRUE(again.prompts.empty()) << "the second gear this turn does not trigger";
    EXPECT_TRUE(o(second).is_exhausted);
}

// 406 Jayce, Man of Progress: "ignoring its Energy cost. (You must still pay
// its Power cost.)" — Sun Disc is [2][R].
TEST_F(VerifyCardsTest, JayceManOfProgress_TheGrantedGearStillCostsItsPower) {
    auto jayce = addToHand(P1, kJayceManOfProgress);
    auto kill = addGear(P1, kSerratedDirk);
    auto disc = addToHand(P1, kSunDisc);
    EnginePathAgent agent(EnginePathAgent::choosing(kill, EnginePathAgent::May::Yes));
    playThroughEngine(jayce, &agent);
    ASSERT_FALSE(onBoard(kill));
    ASSERT_EQ(ps(P1).cost_modifiers.size(), 1u) << "the permission";

    // No Power: the Energy is ignored, the [R] is not.
    ps(P1).rune_pool.energy = 0;
    ps(P1).rune_pool.universal_power = 0;
    power(P1, Domain::Fury) = 0;
    withEngine([&](GameEngine& e) {
        EXPECT_FALSE(e.testHook_canAfford(P1, disc)) << "[R] must still be paid";
    });
    // With the Power: affordable, and playing it spends the Power and no Energy.
    power(P1, Domain::Fury) = 1;
    ps(P1).rune_pool.energy = 3;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_canAfford(P1, disc));
    });
    playThroughEngine(disc, nullptr, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_TRUE(onBoard(disc));
    EXPECT_EQ(power(P1, Domain::Fury), 0) << "its Power cost was paid";
    EXPECT_EQ(ps(P1).rune_pool.energy, 3) << "its Energy cost was ignored";
    EXPECT_TRUE(ps(P1).cost_modifiers.empty()) << "\"a gear\": the permission is used up";
}


// ═══ Tranche 2, group B ═══════════════════════════════════════════════════════
// (already defined in the file and used below: kArise, kVoidBurrower, kGuards,
//  kGlascMixologist, kWildClaw, kMegaMech, kShipyardSkulker, kMeditation,
//  kSerratedDirk, kForgottenMonument)

// ═══ Sweep B: items 1-7 ═════════════════════════════════════════════════════

// ── 1. Icathian Rain 247: a play-time unit that is gone loses its "Deal 2" ──

TEST_F(VerifyCardsTest, IcathianRain_AUnitGoneBeforeItResolvesLosesOneOfTheSix) {
    // CR 359.3.e.5: the first "Deal 2 to a unit" was aimed as the spell was
    // played. Its unit leaves in response: that instance is lost, the other
    // five are still chosen — not six fresh ones.
    auto spell = addToHand(P1, kIcathianRain);
    auto first = addUnit(P2, kInvalidId, 3, 0);
    auto tough = addUnit(P2, kInvalidId, 30, 1);
    const auto& def = card_db.get(kIcathianRain);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {first};
    EnginePathAgent agent(EnginePathAgent::choosing(tough));
    executeAndProbe(play, &agent, [&](GameState& live) { removeFromBoard(live, first); });
    EXPECT_EQ(agent.prompts.size(), 5u) << "five units left to choose, not six";
    EXPECT_EQ(o(tough).damage_marked, 10) << "5 x 2";
    EXPECT_FALSE(onBoard(first));
}

// ── 2. Bullet Time 263: a battlefield is chosen, not a unit; X is capped ──

TEST_F(VerifyCardsTest, BulletTime_ChoosesABattlefieldNotAUnitThere) {
    // "...to all enemy units at a battlefield": no unit is chosen, so nothing
    // is "chosen" (no Deflect, no "chose an enemy unit this turn") and any
    // battlefield may be named, the empty one included.
    auto spell = addToHand(P1, kBulletTime);
    auto e1 = addUnit(P2, kInvalidId, 9, 1);
    auto e2 = addUnit(P2, kInvalidId, 9, 1);
    ps(P1).rune_pool.universal_power = 2;
    EnginePathAgent agent(EnginePathAgent::script({1, 2}));   // battlefield slot 1; X = 2
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the battlefield, then X";
    for (const auto& i : agent.prompts[0])
        EXPECT_TRUE(i.chosen_objects.empty()) << "a battlefield is offered, not a unit";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "a battlefield with no unit may be named";
    EXPECT_TRUE(promptOffersValue(agent, 0, 1));
    EXPECT_FALSE(ps(P1).chose_enemy_unit_this_turn) << "no unit was chosen";
    EXPECT_EQ(o(e1).damage_marked, 2);
    EXPECT_EQ(o(e2).damage_marked, 2);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0);
}

TEST_F(VerifyCardsTest, BulletTime_TheBattlefieldIsNamedBeforeTheOpponentCanRespond) {
    // CR 355: chosen as the spell is played; X is asked as it resolves.
    auto spell = addToHand(P1, kBulletTime);
    auto enemy = addUnit(P2, kInvalidId, 9, 1);
    ps(P1).rune_pool.universal_power = 1;
    ps(P1).rune_pool.energy += card_db.get(kBulletTime).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    EnginePathAgent agent(EnginePathAgent::script({1, 1}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 1u) << "the battlefield is already chosen, X is not";
        EXPECT_EQ(live.player(P1).rune_pool.universal_power, 1) << "nothing paid yet";
        EXPECT_EQ(live.getObject(enemy).damage_marked, 0);
    });
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(o(enemy).damage_marked, 1);
}

TEST_F(VerifyCardsTest, BulletTime_XIsCappedAtTheFifteenTheActionVocabularyEncodes) {
    // pickXAmount publishes one int-coded choice per X; the action vocabulary
    // has 16 int slots (openspiel::kNumIntChoices): 0..15.
    auto spell = addToHand(P1, kBulletTime);
    auto enemy = addUnit(P2, kInvalidId, 99, 0);
    ps(P1).rune_pool.universal_power = 20;
    EnginePathAgent agent(EnginePathAgent::script({0, 15}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(agent.prompts[1].size(), 16u) << "X = 0..15";
    EXPECT_TRUE(promptOffersValue(agent, 1, 15));
    EXPECT_FALSE(promptOffersValue(agent, 1, 16));
    EXPECT_EQ(o(enemy).damage_marked, 15);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 5);
}

// ── 3. Promising Future 115: only one of the top five can be banished ──

// An answer naming an object that was never on offer does not reach the card:
// the engine refuses it (GameEngine::askAgent, plans/09 item 9.G4). The card's
// own fallback (banish the top card) is still there for a direct call. This
// test used to assert that fallback through the engine, before the engine
// checked answers.
TEST_F(VerifyCardsTest, PromisingFuture_AnAnswerThatIsNotOneOfTheTopFiveIsRefused) {
    auto bystander = addUnit(P1, kShipyardSkulker, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kPromisingFuture);
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        Intent wrong = legal.front();
        wrong.chosen_objects = {bystander};
        return wrong;
    });
    EXPECT_THROW(playThroughEngine(spell, &agent), std::invalid_argument);
    ASSERT_EQ(agent.prompts.size(), 1u) << "P2 has no deck: only P1 is asked";
    EXPECT_FALSE(agent.wasOffered(bystander));
    EXPECT_TRUE(onBoard(bystander)) << "a unit on the board is not one of the top 5";
    EXPECT_TRUE(o(bystander).isAtBase()) << "it never moved";
    EXPECT_TRUE(inDeck(P1, top));
}

// ── 4. Keeper's Verdict 760: a token's owner has nothing to decide ──

TEST_F(VerifyCardsTest, KeepersVerdict_ATokensOwnerIsNotAskedTopOrBottom) {
    // CR 183.1: a token that leaves the board ceases to exist.
    auto spell = addToHand(P1, kKeepersVerdict);
    auto token = addUnit(P2, kInvalidId, 3, 0);
    o(token).is_token = true;
    o(token).super_type = SuperType::Token;
    addToDeck(P2, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(token));
    EnginePathAgent owner;
    playThroughEngine(spell, &agent, std::nullopt, &owner);
    EXPECT_TRUE(owner.prompts.empty()) << "top or bottom of a deck it never reaches";
    EXPECT_FALSE(onBoard(token));
    EXPECT_FALSE(inDeck(P2, token));
    EXPECT_EQ(deckSize(P2), 1);
    EXPECT_FALSE(inBanishment(P2, token));
}

// ── 5 / 6. Void Burrower 553: "may" and "exhaust me" at finalization; a
//           printed reveal is shown to both players ──

TEST_F(VerifyCardsTest, VoidBurrower_IsExhaustedBeforeTheOpponentCanRespondAndRevealsToBoth) {
    auto legend = addLegend(P1, kVoidBurrower);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    int shown = 0, to_all = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card != top && e.card != below) return;
        ++shown;
        if (e.revealed_to_all) ++to_all;
    });
    EnginePathAgent agent(EnginePathAgent::script({
        1,                         // "may exhaust me to reveal 2": yes
        0,                         // "You may banish one": banish none (value 0; the cards are objects)
    }));
    fireAndProbe(legend, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 1u) << "the may was decided at finalization";
        EXPECT_TRUE(live.getObject(legend).is_exhausted) << "\"exhaust me\" is the cost (CR 383.3.b)";
        EXPECT_EQ(shown, 0) << "nothing is revealed until it resolves";
        EXPECT_EQ(live.player(P1).main_deck.size(), 2u);
    });
    conn.disconnect();
    EXPECT_EQ(agent.prompts.size(), 2u) << "\"You may banish one\" is asked on resolution";
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(to_all, 2) << "\"reveal\": to every player";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_EQ(deckSize(P1), 2) << "none banished: both recycled";
}

TEST_F(VerifyCardsTest, VoidBurrower_DecliningAtFinalizationCostsNothing) {
    auto legend = addLegend(P1, kVoidBurrower);
    addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_EQ(ps(P1).main_deck.back(), top) << "the deck is untouched";
}

// ── 5. Deceiver 756: "may", discard and exhaust at finalization (and its
//       first engine-path test) ──

TEST_F(VerifyCardsTest, Deceiver_ThroughTheEngine_TheCostIsPaidBeforeTheOpponentCanRespond) {
    auto legend = addLegend(P1, kDeceiver);
    auto bf_card = battlefieldCard(0, kForgottenMonument);   // the battlefield held
    auto model = addUnit(P1, kShipyardSkulker, 0, 0);
    auto elsewhere = addUnit(P1, kShipyardSkulker, 0, 1);
    auto keep = addToHand(P1, kMeditation);
    auto discard = addToHand(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(discard.raw()), static_cast<int>(model.raw())}));
    fireAndProbe(legend, TriggerType::WhenIConquerOrHold, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 2u) << "the may and the discard: at finalization";
        EXPECT_TRUE(live.getObject(legend).is_exhausted) << "\"exhaust me\" (CR 383.3.b)";
        const auto& trash = live.player(P1).trash;
        EXPECT_NE(std::find(trash.begin(), trash.end(), discard), trash.end()) << "\"discard 1\"";
        for (const auto& [id, obj] : live.objects)
            EXPECT_FALSE(obj.isToken()) << "the Reflection is not played yet";
    }, bf_card);
    // "Then do this: It becomes a copy of another unit there." is a reflexive
    // trigger (CR 388): its unit is chosen as the ability resolves.
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(agent.wasOffered(model));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "another unit THERE";
    EXPECT_TRUE(inTrash(P1, discard));
    EXPECT_TRUE(inHand(P1, keep));
    GameObjectId token = kInvalidId;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.isUnit() && obj.location.has_value()) token = id;
    ASSERT_NE(token, kInvalidId);
    EXPECT_EQ(bfOf(token), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "there";
    EXPECT_EQ(o(token).card_def_id, kShipyardSkulker) << "a copy of the chosen unit";
    EXPECT_TRUE(o(token).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(o(token).is_exhausted) << "a READY Reflection";
}

TEST_F(VerifyCardsTest, Deceiver_ThroughTheEngine_DecliningPaysNothingAndPlaysNothing) {
    auto legend = addLegend(P1, kDeceiver);
    auto bf_card = battlefieldCard(0, kForgottenMonument);
    addUnit(P1, kShipyardSkulker, 0, 0);
    auto card = addToHand(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquerOrHold, &agent, bf_card);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_TRUE(inHand(P1, card));
    for (const auto& [id, obj] : state.objects) EXPECT_FALSE(obj.isToken());
}

// ── 5 / 7. Rek'Sai, Swarm Queen 491: the first "may" at finalization; "you
//           may play it here" is one more place the unit can go ──

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheRevealIsDecidedBeforeTheOpponentCanRespond) {
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);
    addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    int shown = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent&) { ++shown; });
    EnginePathAgent agent(EnginePathAgent::script({
        1,                       // "may reveal the top 2 cards": yes
        // "You may banish one" (the two cards, then "banish none" = value 0):
        // the top card. Meditation is a [2] spell P1 has nothing to pay for,
        // so it stays banished and nothing more is asked. (Declining:
        // RekSaiSwarmQueen_BanishNoneRecyclesBothRevealedCards.)
        static_cast<int>(top.raw()),
    }));
    fireAndProbe(reksai, TriggerType::WhenIAttack, &agent, [&](GameState&) {
        EXPECT_EQ(agent.prompts.size(), 1u)
            << "CR 383.3.a: a \"you may\" that opens the effect is decided at finalization";
        EXPECT_EQ(shown, 0) << "nothing is revealed until it resolves";
    });
    conn.disconnect();
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(agent.prompts.size(), 2u) << "\"You may banish one\": on resolution";
}

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheUnitMayGoToBaseAndNotWhereUnitsCannotBePlayed) {
    // "If it is a unit, you may play it here": a permission (CR 355.2.b), on
    // top of the base and the battlefields its controller controls.
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);       // "here" = battlefield #0
    state.battlefields[1].controller = P1;
    state.battlefields[1].blocks_unit_play = true;
    addUnit(P1, kInvalidId, 1, 1);                            // holds battlefield #1
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);              // top
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(unit.raw()), 0}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the card, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "here";
    EXPECT_FALSE(promptOffersValue(agent, 2, 2)) << "units can't be played to battlefield #1";
    ASSERT_TRUE(onBoard(unit));
    EXPECT_TRUE(o(unit).isAtBase()) << "\"you MAY play it here\"";
    EXPECT_EQ(deckSize(P1), 1) << "the rest was recycled";
}

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_TheUnitCanBePlayedHere) {
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(unit.raw()), 1}));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_FALSE(inBanishment(P1, unit));
}

// ── 6. Teemo, Strategist 121: "reveal the top 5" is shown to both players ──

TEST_F(VerifyCardsTest, TeemoStrategist_TheRevealIsShownToBothPlayers) {
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    addUnit(P2, kInvalidId, 3, 0);   // the enemy unit he chooses (CR 403.4: with none, no ability)
    addToDeck(P1, kMeditation);
    addToDeck(P1, kMeditation);
    int shown = 0, to_all = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        ++shown;
        if (e.revealed_to_all) ++to_all;
    });
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend);
    conn.disconnect();
    EXPECT_EQ(shown, 2);
    EXPECT_EQ(to_all, 2) << "\"reveal\": to every player, as the deck bookkeeping records";
}

// ── 5. Forgotten Relic 895: its first engine-path tests ──

TEST_F(VerifyCardsTest, ForgottenRelic_ThroughTheEngine_BurningAUnitGivesItsMightToTheChosenUnit) {
    auto relic = addGear(P1, kForgottenRelic);
    auto first = addUnit(P1, kShipyardSkulker, 0);
    auto second = addUnit(P1, kShipyardSkulker, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 0);
    addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMegaMech);
    EnginePathAgent agent(EnginePathAgent::choosing(second));
    fireTriggerThroughEngine(relic, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_TRUE(inTrash(P1, top)) << "[Burn 1]";
    EXPECT_EQ(deckSize(P1), 1) << "burned once, however often the callback is re-entered";
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_FALSE(agent.wasOffered(theirs)) << "a FRIENDLY unit";
    EXPECT_EQ(o(second).temp_might_bonus, card_db.get(kMegaMech).might)
        << "+[M] equal to the burned card's Might";
    EXPECT_EQ(o(first).temp_might_bonus, 0);
}

TEST_F(VerifyCardsTest, ForgottenRelic_ThroughTheEngine_PlayingItBurnsOneAndANonUnitGivesNothing) {
    auto relic = addToHand(P1, kForgottenRelic);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    addToDeck(P1, kMegaMech);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent;
    playThroughEngine(relic, &agent);
    ASSERT_TRUE(onBoard(relic));
    EXPECT_TRUE(inTrash(P1, top)) << "When you play this, [Burn 1]";
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_TRUE(agent.prompts.empty()) << "not a unit: nobody gets Might";
    EXPECT_EQ(o(unit).temp_might_bonus, 0);
}

// ── 7. Effect plays respect "units can't be played here" (CR 355.2.a) ──

TEST_F(VerifyCardsTest, GlascMixologist_ChoosesWhereAndIsNotOfferedABattlefieldUnitsCannotBePlayedTo) {
    auto glasc = addUnit(P1, kGlascMixologist, 0);
    auto cheap = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(cheap.raw()), 2}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(glasc); },
                         &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(cheap));
    EXPECT_EQ(bfOf(cheap), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, HeedlessResurrection_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kHeedlessResurrection);
    auto victim = addUnit(P1, kShipyardSkulker, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    // The unit to kill, the unit to play, where (mode 0 = base, then the
    // battlefields units can be played to, in order).
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(victim.raw()), static_cast<int>(corpse.raw()), 1}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    // Options are numbered over what is offered (playLocations leaves the
    // blocked battlefield out — plans/09 item 9.G1): base, then battlefield #1.
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "battlefield #1";
    EXPECT_FALSE(promptOffersValue(agent, 2, 2)) << "units can't be played to battlefield #0";
    EXPECT_EQ(agent.prompts[2].size(), 2u);
    ASSERT_TRUE(onBoard(corpse));
    EXPECT_EQ(bfOf(corpse), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_TRUE(inTrash(P1, victim));
}

TEST_F(VerifyCardsTest, ArcaneShift_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kArcaneShift);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    // Batch M1: both units are targets chosen as the spell is played, as a
    // pair (CR 355.5, 355.8): the friendly unit, then "an enemy unit at a
    // battlefield", which the test now provides; the location prompt follows
    // as it resolves (it was the only prompt, the unit named in the intent).
    auto enemy = addUnit(P2, kInvalidId, 9, 1);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(unit.raw()), static_cast<int>(enemy.raw()), 1}));
    playAt(spell, {}, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    // Options are numbered over what is offered (playLocations leaves the
    // blocked battlefield out — plans/09 item 9.G1): base, then battlefield #1.
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "battlefield #1";
    EXPECT_FALSE(promptOffersValue(agent, 2, 2)) << "units can't be played to battlefield #0";
    EXPECT_EQ(agent.prompts[2].size(), 2u);
    ASSERT_TRUE(onBoard(unit));
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, Arise_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kArise);
    addGear(P1, kSerratedDirk);                               // one Equipment: one token
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({
        1,   // where the one Sand Soldier goes: option 1 = battlefield #1 (0 = base)
        0,   // "Ready up to two of them" (the new token, then "ready no more" = 0):
             // ready no more — readying is not what this test is about
    }));
    playThroughEngine(spell, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    // Options are numbered over what is offered (playLocations leaves the
    // blocked battlefield out — plans/09 item 9.G1): base, then battlefield #1.
    EXPECT_TRUE(promptOffersValue(agent, 0, 1)) << "battlefield #1";
    EXPECT_FALSE(promptOffersValue(agent, 0, 2)) << "units can't be played to battlefield #0";
    EXPECT_EQ(agent.prompts[0].size(), 2u);
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, Guards_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kGuards);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "where; no [Y] for the may";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    // Options are numbered over what is offered (playLocations leaves the
    // blocked battlefield out — plans/09 item 9.G1): base, then battlefield #1.
    EXPECT_TRUE(promptOffersValue(agent, 0, 1)) << "battlefield #1";
    EXPECT_FALSE(promptOffersValue(agent, 0, 2)) << "units can't be played to battlefield #0";
    EXPECT_EQ(agent.prompts[0].size(), 2u);
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(VerifyCardsTest, Guards_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    // Mageseeker Warden: "While I'm at a battlefield, opponents can only play
    // units to their base." (The flag is rebuilt from the Warden by every
    // cleanup; it is also set here so the test does not depend on one running.)
    auto spell = addToHand(P1, kGuards);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    // One prompt only: the [3] is paid from the pool, so there is no [Y] left
    // and the "pay [Y] to ready it" may is not offered.
    EnginePathAgent agent(EnginePathAgent::script({
        0,   // where the Sand Soldier goes: mode 0 = base, the only place on offer
    }));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 1u) << "the base, nothing else";
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1);
    EXPECT_TRUE(o(firstNamed(P1, "Sand Soldier")).isAtBase());
}

TEST_F(VerifyCardsTest, WildClaw_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    ps(P1).rune_pool.energy = 2;
    // The card, where (0 = base, 1 + i = battlefield i), then "yes" to Empower.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mech.raw()), 2, 1}));
    playThroughEngine(spell, &agent);
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 1, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 1, 2));
    ASSERT_TRUE(onBoard(mech));
    EXPECT_EQ(bfOf(mech), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}


// ═══ Tranche 2, group C ═══════════════════════════════════════════════════════
// (already defined in the file and used below: kSett164, kSett543, kRecruit,
//  kShipyardSkulker.)

// ═══ Sweep C, item 1: option lists are offered in object-id order ═══════════

// Thirteen cards built the list they hand to pickTarget / pickTargetPair by
// walking GameState::objects (an unordered_map) and offered it as it came:
// the order of the options — what an agent sees at each index, and the
// default first pick — was hash order. Each now sorts its list.
//
// NOTE: this pins the sort; it cannot force the old code to fail. Hash order
// is not under a test's control, so the unsorted code passes whenever the map
// happens to iterate in ascending id order. To make that unlikely each board
// has nine or more candidates-and-bystanders created interleaved (enemy,
// friend, gear, enemy, ...), every round adds to the same map (so it rehashes
// along the way), and thirteen cards are checked. (libstdc++ walks freshly
// inserted integer keys most-recent-first, so in practice the old code fails
// every round — but that is an implementation detail, not a guarantee.)
TEST_F(VerifyCardsTest, OptionLists_AreOfferedInAscendingObjectIdOrder) {
    enum class Kind { Unit, Battlefield, Gear };
    struct Case { CardDefId id; TriggerType trigger; Kind kind; };
    const Case cases[] = {
        {234, TriggerType::WhenYouPlayMe, Kind::Unit},           // Harnessed Dragon
        {238, TriggerType::WhenIAttack, Kind::Unit},             // Leona, Determined
        {330, TriggerType::WhenYouPlayMe, Kind::Unit},           // Gem Jammer
        {361, TriggerType::WhenIMoveToFB, Kind::Unit},           // Ribbon Dancer
        {453, TriggerType::WhenYouPlayMe, Kind::Unit},           // Beast Below (a pair)
        {479, TriggerType::WhenYouPlayMe, Kind::Unit},           // Sandshifter
        {539, TriggerType::WhenIAttackOrDefend, Kind::Unit},     // Ahri, Inquisitive
        {589, TriggerType::WhenIConquer, Kind::Unit},            // Inviolus Vox
        {618, TriggerType::WhenIAttackOrDefend, Kind::Unit},     // Yuumi, Magical Cat
        {278, TriggerType::WhenYouHoldHere, Kind::Battlefield},  // Navori Fighting Pit
        {535, TriggerType::WhenYouConquerHere, Kind::Battlefield},  // Veiled Temple (gear)
        {763, TriggerType::WhenYouHoldHere, Kind::Battlefield},  // Amateur Recital
        {63, TriggerType::WhenYouPlayThis, Kind::Gear},          // Spirit's Refuge
    };
    for (const Case& c : cases) {
        SCOPED_TRACE("card " + toString(c.id) + " " + card_db.get(c.id).name);
        // Everything at battlefield #0 (the gear in base), interleaved, with
        // the source in the middle. Shipyard Skulker: 3 [M], no text.
        std::vector<GameObjectId> made;
        GameObjectId source = kInvalidId;
        for (int i = 0; i < 4; ++i) {
            made.push_back(addUnit(P2, kShipyardSkulker, 0, 0));
            made.push_back(addUnit(P1, kShipyardSkulker, 0, 0));
            if (i < 3) made.push_back(addGear(P1, kInvalidId));
            if (i == 1) {
                if (c.kind == Kind::Unit) source = addUnit(P1, c.id, 0, 0);
                else if (c.kind == Kind::Gear) source = addGear(P1, c.id);
                else source = battlefieldCard(0, c.id, P1);
                made.push_back(source);
            }
        }

        AcceptingEnginePathAgent agent;               // yes to a "may", then the first option
        fireTriggerThroughEngine(source, c.trigger, &agent);

        int object_prompts = 0;
        for (const auto& prompt : agent.prompts) {
            if (prompt.empty() || prompt.front().chosen_objects.empty()) continue;   // a "may"
            ++object_prompts;
            EXPECT_GE(prompt.size(), 3u) << "at least three candidates";
            for (size_t i = 1; i < prompt.size(); ++i) {
                ASSERT_FALSE(prompt[i].chosen_objects.empty());
                EXPECT_LT(prompt[i - 1].chosen_objects.front(), prompt[i].chosen_objects.front())
                    << "option " << i << " is out of object-id order";
            }
        }
        EXPECT_GE(object_prompts, 1) << "the card never asked which object";

        // Clear the board for the next card (the objects stay in the map).
        for (auto id : made) {
            o(id).location = std::nullopt;
            o(id).zone = ZoneType::Banishment;
        }
        for (auto p : {P1, P2}) {
            ps(p).hand.clear();
            ps(p).trash.clear();
        }
        state.battlefields[0].card_object_id = kInvalidId;
        state.battlefields[0].controller = std::nullopt;
    }
}

// ═══ Sweep C, item 2: Soul Sword 601 — the Level 3 bonus needs both on board ═

TEST_F(VerifyCardsTest, SoulSword_OffTheBoardGearOrBearerGivesNoLevelBonus) {
    ps(P1).xp = 3;                                    // [Level 3][>] is on
    // Control: attached and both on the board — +1 [M] printed, +1 [M] more.
    auto bearer = addUnit(P1, kShipyardSkulker, 2);
    auto sword = addGear(P1, kSoulSword);
    attach(sword, bearer);
    // A Soul Sword that left the board with `attached_to` still set.
    auto kept = addUnit(P1, kShipyardSkulker, 2);
    auto gone_sword = addGear(P1, kSoulSword);
    o(gone_sword).attached_to = kept;
    sendToTrash(gone_sword);
    // A Soul Sword on the board whose bearer left it.
    auto gone_bearer = addUnit(P1, kShipyardSkulker, 2);
    auto orphan = addGear(P1, kSoulSword);
    o(orphan).attached_to = gone_bearer;
    sendToTrash(gone_bearer);

    cleanup();

    EXPECT_EQ(o(bearer).current_might, 4) << "2 + the Equipment's +1 + the Level 3 +1";
    EXPECT_EQ(o(kept).aura_might_bonus, 0) << "a Soul Sword in the trash gives nothing";
    EXPECT_EQ(o(kept).current_might, 2);
    EXPECT_TRUE(o(gone_bearer).aura_effects.empty()) << "a card in the trash is not a bearer";
    EXPECT_EQ(o(gone_bearer).aura_might_bonus, 0);
}

// ═══ Sweep C, item 3: Spirit's Refuge 63 next to Allay 603 ══════════════════

// "Friendly buffed units have [Deflect] if they didn't already." The Refuge's
// hook cannot see a [Deflect] another card's hook adds after it ran (Allay:
// "your other units here have [Deflect]"), so its grant is "only if absent"
// (auraKeywordIfAbsent) and the engine drops it once every hook has run
// (plans/09 item 9.A19). This test asserted "1 or 2 by hook order" while that
// was open; it now asserts the 1 [A] the card text says. Both creation orders:
// VerifyEngineRefugeTest.SpiritsRefuge_NextToAllayCostsOne_*.
TEST_F(VerifyCardsTest, SpiritsRefuge_NextToAllayTheDeflectCostDependsOnHookOrder) {
    state.battlefields[0].controller = P1;
    auto refuge = addGear(P1, kSpiritsRefuge);
    auto allay = addUnit(P1, kAllay, 0, 0);
    auto both = addUnit(P1, kShipyardSkulker, 2, 0);  // buffed, next to Allay
    o(both).buff_count = 1;
    auto plain = addUnit(P1, kShipyardSkulker, 2, 0); // next to Allay, no buff
    auto solo = addUnit(P1, kShipyardSkulker, 2);     // buffed, in base: the Refuge only
    o(solo).buff_count = 1;

    cleanup();

    auto deflectEffects = [&](GameObjectId id, GameObjectId from) {
        int n = 0;
        for (const auto& ae : o(id).aura_effects)
            if (ae.keyword == Keyword::Deflect && (from == kInvalidId || ae.source == from)) ++n;
        return n;
    };
    // Order-independent parts.
    EXPECT_EQ(deflectEffects(solo, kInvalidId), 1);
    EXPECT_EQ(deflectEffects(solo, refuge), 1);
    EXPECT_EQ(deflectCostFor(state, P2, solo), 1);
    EXPECT_EQ(deflectEffects(plain, kInvalidId), 1);
    EXPECT_EQ(deflectEffects(plain, allay), 1);
    EXPECT_EQ(deflectCostFor(state, P2, plain), 1);
    EXPECT_EQ(deflectCostFor(state, P2, allay), 1) << "printed; the Refuge adds nothing";
    EXPECT_EQ(deflectEffects(allay, kInvalidId), 0);

    // The unit both cards reach: Allay's [Deflect], and none from the Refuge
    // ("if they didn't already"), whichever hook ran first.
    EXPECT_EQ(deflectEffects(both, allay), 1);
    EXPECT_EQ(deflectEffects(both, refuge), 0);
    EXPECT_EQ(deflectEffects(both, kInvalidId), 1);
    EXPECT_EQ(deflectCostFor(state, P2, both), 1);
}

// ═══ Sweep C, item 4: what a card does once its object has left the board ═══

// 242 Baited Hook: the unit to kill left the board in response — it is not
// killed, so there is no Might to dig against: no look, no unit played.
TEST_F(VerifyCardsTest, BaitedHook_TheVictimLeftInResponse_NoDigAndNoUnitPlayed) {
    auto hook = addGear(P1, kBaitedHook);
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Order) = 1;
    auto victim = addUnit(P1, kInvalidId, 1);
    addToDeck(P1, kInvalidId);
    auto eligible = addToDeck(P1, kRecruit);
    o(eligible).base_might = 1;
    addToDeck(P1, kInvalidId);
    const auto deck_before = ps(P1).main_deck;

    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = hook;
    act.targets = {victim};
    EnginePathAgent agent(EnginePathAgent::choosing(eligible));
    executeAndProbe(act, &agent, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(hook).is_exhausted) << "activated and paid for";
        removeFromBoard(live, victim);
    });

    EXPECT_EQ(power(P1, Domain::Order), 0);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing is looked at, nothing is offered";
    EXPECT_FALSE(agent.wasOffered(eligible));
    EXPECT_TRUE(inDeck(P1, eligible));
    EXPECT_FALSE(onBoard(eligible));
    EXPECT_EQ(ps(P1).main_deck, deck_before) << "nothing recycled either";
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 0);
    EXPECT_EQ(std::count(ps(P1).trash.begin(), ps(P1).trash.end(), victim), 1)
        << "not killed a second time";
}

// 490 Altar of Memories: the Altar is gone when its trigger is finalized —
// "you may exhaust me" cannot be paid, so the may is not offered.
TEST_F(VerifyCardsTest, AltarOfMemories_OffTheBoardIsNotOfferedAndDrawsNothing) {
    auto altar = addGear(P1, kAltarOfMemories);
    auto dead = addUnit(P1, kInvalidId, 1);
    sendToTrash(dead);
    auto in_hand = addToHand(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    sendToTrash(altar);                               // left the board with the unit

    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(altar, TriggerType::WhenAFriendlyUnitDies, &yes, dead);

    EXPECT_TRUE(yes.prompts.empty()) << "an Altar in the trash cannot be exhausted";
    EXPECT_FALSE(o(altar).is_exhausted);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(inHand(P1, in_hand));
    EXPECT_EQ(deckSize(P1), 2) << "nothing drawn, nothing put back";
}

// 864 Tools of Empire: the chosen unit left the board in response.
TEST_F(VerifyCardsTest, ToolsOfEmpire_AUnitThatLeftInResponseGetsNoMight) {
    auto tools = addGear(P1, kToolsOfEmpire);
    auto unit = addUnit(P1, kShipyardSkulker, 2);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = tools;
    act.targets = {unit};
    executeAndProbe(act, nullptr, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(tools).is_exhausted) << "activated";
        EXPECT_EQ(live.getObject(unit).temp_might_bonus, 0) << "not resolved yet";
        removeFromBoard(live, unit);
    });
    EXPECT_EQ(o(unit).temp_might_bonus, 0) << "a card in the trash gets no Might";
    EXPECT_EQ(o(unit).current_might, 2);

    // Control: the same activation with the unit still there gives +2 [M].
    auto tools2 = addGear(P1, kToolsOfEmpire);
    auto unit2 = addUnit(P1, kShipyardSkulker, 2);
    act.ability_source = tools2;
    act.targets = {unit2};
    executeAndProbe(act, nullptr, [](GameState&) {});
    EXPECT_EQ(o(unit2).temp_might_bonus, 2);
}

// 157 Udyr, Wildman: "Spend my buff" is the cost, and it is Udyr's own buff.
// An Udyr that is not on the board (a stale or hand-built activation; the
// generator never offers one) pays nothing, and then nothing happens — the
// chosen mode does not resolve for free.
TEST_F(VerifyCardsTest, Udyr_OffTheBoardSpendsNoBuffAndDoesNothing) {
    auto udyr = addUnit(P1, kUdyrWildman, 0, 0);
    o(udyr).buff_count = 1;
    auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);
    sendToTrash(udyr);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = udyr;
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(enemy.raw())}));   // Deal 2
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); }, &agent);
    EXPECT_EQ(o(udyr).buff_count, 1) << "the buff of a card in the trash is not spent";
    EXPECT_EQ(o(udyr).card_counters.count("__udyr_modes_mask"), 0u) << "no mode used up";
    EXPECT_EQ(o(enemy).damage_marked, 0) << "no cost paid: no damage";
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_FALSE(onBoard(udyr));
}

// ...and an Udyr that leaves in response, after the buff was spent: "Ready
// me" and "Give me [Ganking]" do nothing to the card in the trash.
TEST_F(VerifyCardsTest, Udyr_LeftInResponse_IsNeitherReadiedNorGivenGanking) {
    for (int mode : {2, 3}) {                         // Ready me / Give me [Ganking]
        SCOPED_TRACE("mode " + std::to_string(mode));
        auto udyr = addUnit(P1, kUdyrWildman);
        o(udyr).buff_count = 1;
        o(udyr).is_exhausted = true;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = udyr;
        EnginePathAgent agent(EnginePathAgent::script({mode}));
        executeAndProbe(act, &agent, [&](GameState& live) {
            EXPECT_EQ(live.getObject(udyr).buff_count, 0) << "spent as the ability was activated";
            removeFromBoard(live, udyr);
        });
        ASSERT_EQ(agent.prompts.size(), 1u) << "the mode";
        EXPECT_TRUE(o(udyr).is_exhausted) << "a card in the trash is not readied";
        EXPECT_FALSE(o(udyr).keywords.has(Keyword::Ganking));
        EXPECT_FALSE(o(udyr).temp_keywords.has(Keyword::Ganking));
        EXPECT_FALSE(onBoard(udyr));
        EXPECT_EQ(o(udyr).zone, ZoneType::Trash);
    }
}

// 164 / 543 Sett, Brawler: activated (the buff is spent), then he leaves the
// board in response — the card in the trash gets no +4 [M].
TEST_F(VerifyCardsTest, SettBrawler_LeftInResponse_GetsNoMightInTheTrash) {
    for (CardDefId def_id : {kSett164, kSett543}) {
        SCOPED_TRACE("card " + toString(def_id));
        auto sett = addUnit(P1, def_id);
        o(sett).buff_count = 1;
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = sett;
        executeAndProbe(act, nullptr, [&](GameState& live) {
            EXPECT_EQ(live.getObject(sett).buff_count, 0) << "spent as the ability was activated";
            EXPECT_EQ(live.getObject(sett).temp_might_bonus, 0) << "not resolved yet";
            removeFromBoard(live, sett);
        });
        EXPECT_EQ(o(sett).temp_might_bonus, 0) << "no +4 [M] on a card in the trash";
        EXPECT_FALSE(onBoard(sett));
        EXPECT_EQ(o(sett).zone, ZoneType::Trash);

        // Control: he stays, he gets the +4 [M].
        auto stays = addUnit(P1, def_id);
        o(stays).buff_count = 1;
        act.ability_source = stays;
        executeAndProbe(act, nullptr, [](GameState&) {});
        EXPECT_EQ(o(stays).temp_might_bonus, 4);
        EXPECT_EQ(o(stays).buff_count, 0);
        o(stays).location = std::nullopt;             // off the board for the next printing
        o(stays).zone = ZoneType::Banishment;
    }
}

// ═══ Sweep C, item 5: 108 Convergent Mutation — what keeps it off the trash ═

// The chosen unit leaves the board in response. EffectExecutor::
// giveTemporaryMight is NOT what protects the card in the trash (it only
// checks objectExists and would add the Might): GameEngine::resolveSpell
// re-validates the play-time target against Card::enumerateLegalTargets (a
// friendly unit ON THE BOARD) and drops it, so onResolve gets no target and
// returns before it asks which other unit to copy.
TEST_F(VerifyCardsTest, Pin_Batch08_ConvergentMutation108_AUnitThatLeftInResponseGetsNoMight) {
    auto chosen = addUnit(P1, kShipyardSkulker, 1);
    auto big = addUnit(P1, kShipyardSkulker, 4);
    auto spell = addToHand(P1, kConvergentMutation);
    const auto& def = card_db.get(kConvergentMutation);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {chosen};
    EnginePathAgent agent;
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(live.getObject(chosen).temp_might_bonus, 0) << "not resolved yet";
        removeFromBoard(live, chosen);
    });
    EXPECT_FALSE(inHand(P1, spell)) << "the spell was played";
    // Batch M1: the other unit is the spell's second target, chosen as it is
    // played (CR 355.5), before the response — one prompt then, none as it
    // resolves (was: no prompt at all, asked only as it resolved).
    EXPECT_EQ(agent.prompts.size(), 1u) << "the other unit, asked as the spell was played";
    EXPECT_EQ(o(chosen).temp_might_bonus, 0) << "a card in the trash gets no Might";
    EXPECT_EQ(o(chosen).current_might, 1);
    EXPECT_EQ(o(big).temp_might_bonus, 0);

    // Control: the same play with the unit still there raises it to 4 [M].
    auto chosen2 = addUnit(P1, kShipyardSkulker, 1);
    auto spell2 = addToHand(P1, kConvergentMutation);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    play.card = spell2;
    play.targets = {chosen2};
    executeAndProbe(play, nullptr, [](GameState&) {});
    EXPECT_EQ(o(chosen2).temp_might_bonus, 3);
    EXPECT_EQ(o(chosen2).current_might, 4);
}

// ═══ Tranche 3: "opponents can only play units to their base" (Mageseeker
// Warden) — the other half of the play-location filter in 704, 515 and 876.
// The flag is rebuilt from the Warden by every cleanup; it is also set here
// so the tests do not depend on one running.

TEST_F(VerifyCardsTest, HeedlessResurrection_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    auto spell = addToHand(P1, kHeedlessResurrection);
    auto victim = addUnit(P1, kShipyardSkulker, 0);
    auto corpse = addToTrash(P1, kShipyardSkulker);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(victim.raw()), static_cast<int>(corpse.raw()), EnginePathAgent::kFirst}));   // only "Base" is offered (asserted below)
    playThroughEngine(spell, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(promptOffersValue(agent, agent.prompts.size() - 1, 1))
        << "battlefield #0 is controlled, but units may only go to the base";
    ASSERT_TRUE(onBoard(corpse));
    EXPECT_TRUE(o(corpse).isAtBase());
}

TEST_F(VerifyCardsTest, ArcaneShift_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    auto spell = addToHand(P1, kArcaneShift);
    auto unit = addUnit(P1, kShipyardSkulker, 0);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    auto warden = addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    // Batch M1: the two units are chosen as the spell is played, as a pair
    // (the friendly unit was the play-time target handed to playAt), then
    // the location as it resolves.
    EnginePathAgent agent(EnginePathAgent::script({
        static_cast<int>(unit.raw()),     // the friendly unit
        static_cast<int>(warden.raw()),   // "Deal 3 to an enemy unit at a battlefield": the Warden,
                                    // the only enemy unit
        0,                          // where the banished unit is played: option 0 = base,
                                    // the only place on offer (asserted below)
    }));
    playAt(spell, {}, &agent);
    for (size_t n = 0; n < agent.prompts.size(); ++n)
        for (const auto& i : agent.prompts[n])
            if (i.chosen_objects.empty()) {
                EXPECT_TRUE(!i.chosen_value.has_value() || *i.chosen_value == 0)
                    << "a battlefield was offered as a place to play the unit";
            }
    ASSERT_TRUE(onBoard(unit));
    EXPECT_TRUE(o(unit).isAtBase());
}

TEST_F(VerifyCardsTest, WildClaw_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    addToDeck(P1, kMeditation);
    auto mech = addToDeck(P1, kMegaMech);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kWildClaw);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);
    ps(P1).units_play_base_only = true;
    ps(P1).rune_pool.energy = 2;
    // The card; then "battlefield #0" if a place is asked (it must not be).
    int asked = 0;
    bool bf_offered = false;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        if (asked++ == 0) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == mech) return i;
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.choice_label.find("battlefield") != std::string::npos) { bf_offered = true; return i; }
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(bf_offered) << "units may only go to the base";
    ASSERT_TRUE(onBoard(mech));
    EXPECT_TRUE(o(mech).isAtBase());
}

// ═══ Tranche 3: plans/09 section G ═══════════════════════════════════════════
/* REPLACES AN EXISTING TEST
   Delete VerifyCardsTest.Pin_FaePorter_GoneBeforeResolutionMovesNobodyAndThePowerStaysSpent
   (and its three-line comment): it pins the reading 9.G2 overturns and fails
   on the fixed card. FaePorter_GoneBeforeResolution_... below takes its place.
*/

// ═══ 9.G2: Fae Porter 446 — "the same battlefield" is the trigger's ═════════
// "When I move to a battlefield, you may pay [P] to move a unit you control to
// the same battlefield." The battlefield comes from the trigger condition, so
// it is fixed when the condition is fulfilled (CR 359.3.f.3), not read off the
// Porter as the ability resolves.
TEST_F(VerifyCardsTest, FaePorter_MovedOnInResponse_TheUnitGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, EnginePathAgent::May::Yes));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_TRUE(live.getObject(ally).isAtBase()) << "not moved yet";
        // In response the Porter ends up at the other battlefield.
        live.getObject(porter).location = BattlefieldLocation{1};
    });
    EXPECT_EQ(bfOf(porter), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "the battlefield the Porter moved to, not the one it is at now";
}

TEST_F(VerifyCardsTest, FaePorter_BackAtBaseInResponse_TheUnitStillGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, EnginePathAgent::May::Yes));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        auto& p = live.getObject(porter);
        p.location = BaseLocation{P1};
        p.zone = ZoneType::Base;
    });
    EXPECT_TRUE(o(porter).isAtBase());
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// The CR 359.3.f.3 example: Lillia leaving the board in response "will not
// affect where the Sprite token will be played". Same here.
TEST_F(VerifyCardsTest, FaePorter_GoneBeforeResolution_TheUnitStillGoesToTheBattlefieldOfTheTrigger) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, EnginePathAgent::May::Yes));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        EXPECT_EQ(live.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
        removeFromBoard(live, porter);                    // in response: the Porter dies
    });
    EXPECT_FALSE(onBoard(porter));
    EXPECT_EQ(power(P1, Domain::Chaos), 0);
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "the battlefield was noted when the Porter moved there";
}

// A unit that got to that battlefield by itself in the meantime is no longer
// "a unit to move there": a mistarget, and it is not moved anywhere else.
TEST_F(VerifyCardsTest, FaePorter_TheChosenUnitAlreadyAtThatBattlefieldIsNotMovedToThePortersNewOne) {
    auto porter = addUnit(P1, kFaePorter, 0, 0);
    // The move that triggered it travels with the trigger (ChainItem::triggering_move).
    AnnouncedMove moved(state, LocationId{BaseLocation{P1}}, LocationId{BattlefieldLocation{0}});
    auto ally = addUnit(P1, kShipyardSkulker, 0);
    power(P1, Domain::Chaos) = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, EnginePathAgent::May::Yes));
    fireAndProbe(porter, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        live.getObject(porter).location = BattlefieldLocation{1};
        auto& a = live.getObject(ally);
        a.location = BattlefieldLocation{0};
        a.zone = ZoneType::BattlefieldZone;
    });
    EXPECT_EQ(bfOf(ally), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "not dragged to battlefield #1";
}

// ═══ 9.G3: Guards! 475 from facedown at "Units can't be played here." ═══════
// CR 811.1.d restricts the unit to the battlefield the spell was hidden at;
// Rockfall Path's text forbids playing units there and a rule does not lift a
// card's "can't" (CR 002, 054.1). No location is left: no token (Vendetta FAQ,
// Perched Grimwyrm at Rockfall Path), and nothing to pay [Y] for.
TEST_F(VerifyCardsTest, Guards_HiddenWhereUnitsCannotBePlayed_PlaysNoSandSoldier) {
    state.turn.turn_number = 3;
    auto hide = [&](int bf) {
        auto spell = addToHand(P1, kGuards);
        auto& h = ps(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        auto& card = o(spell);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = BattlefieldId::fromRaw(bf);
        card.hidden_on_turn = 2;
        state.battlefields[bf].facedown.push_back(spell);
        return spell;
    };
    auto playHidden = [&](GameObjectId spell, AgentInterface* agent) {
        Intent play;
        play.type = IntentType::PlayReaction;
        play.player = P1;
        play.card = spell;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            e.testHook_runChain();
        }, agent);
    };
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;        // Rockfall Path
    state.battlefields[1].controller = P1;

    // The control: hidden at a battlefield units can be played to.
    EnginePathAgent free_agent;
    playHidden(hide(1), &free_agent);
    ASSERT_EQ(countNamed(P1, "Sand Soldier"), 1) << "this setup does play a hidden Guards!";
    EXPECT_EQ(bfOf(firstNamed(P1, "Sand Soldier")), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});

    // Hidden at the battlefield units can't be played to.
    power(P1, Domain::Order) = 1;
    EnginePathAgent agent(EnginePathAgent::script({0, 1}));
    playHidden(hide(0), &agent);
    EXPECT_EQ(countNamed(P1, "Sand Soldier"), 1) << "no second token: not there, not anywhere";
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Sand Soldier" && obj.location.has_value()) {
            EXPECT_NE(obj.battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
                << "\"Units can't be played here.\"";
        }
    // 9.I7, judge ruling 2026-10-11 (confirmed): nothing happens — and the
    // reflexive "Then do this: you may pay [C] to ready it" is not put on the
    // chain: nothing is asked, nothing is paid, nothing is left on the chain.
    EXPECT_TRUE(agent.prompts.empty()) << "no location to pick, no token to ready";
    EXPECT_EQ(power(P1, Domain::Order), 1) << "[Y] not spent on a token that does not exist";
    EXPECT_TRUE(state.chain.items.empty()) << "no reflexive ability was queued";
    EXPECT_FALSE(state.chain.resuming.has_value());
}

// ═══ 9.G6: Udyr, Wildman 157 — NOT A DEFECT ═════════════════════════════════
// "Deal 2 to a unit at a battlefield." / "Stun a unit at a battlefield." read
// nothing off Udyr. The ability is its own chain item (CR 377.3.a.1); only an
// instruction that checks information about a card that has left is ignored
// (CR 359.3.e.12) — which is the other two modes ("Ready me", "Give me
// [Ganking]", pinned by Udyr_LeftInResponse_IsNeitherReadiedNorGivenGanking).
TEST_F(VerifyCardsTest, Pin_Udyr_LeftInResponse_DealTwoAndStunStillResolve) {
    for (int mode : {0, 1}) {                         // Deal 2 / Stun
        SCOPED_TRACE("mode " + std::to_string(mode));
        auto udyr = addUnit(P1, kUdyrWildman);
        o(udyr).buff_count = 1;
        auto enemy = addUnit(P2, kShipyardSkulker, 0, 0);     // 3 [M]: survives 2
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = P1;
        act.ability_source = udyr;
        EnginePathAgent agent(EnginePathAgent::script({mode, static_cast<int>(enemy.raw())}));
        executeAndProbe(act, &agent, [&](GameState& live) {
            EXPECT_EQ(live.getObject(udyr).buff_count, 0) << "spent as the ability was activated";
            EXPECT_EQ(live.getObject(enemy).damage_marked, 0) << "not resolved yet";
            EXPECT_FALSE(live.getObject(enemy).is_stunned) << "not resolved yet";
            removeFromBoard(live, udyr);                      // in response: Udyr leaves
        });
        ASSERT_EQ(agent.prompts.size(), 2u) << "the mode, then its unit";
        EXPECT_FALSE(onBoard(udyr));
        ASSERT_TRUE(onBoard(enemy));
        if (mode == 0) {
            EXPECT_EQ(o(enemy).damage_marked, 2) << "\"Deal 2 to a unit at a battlefield.\"";
            EXPECT_FALSE(o(enemy).is_stunned);
        } else {
            EXPECT_TRUE(o(enemy).is_stunned) << "\"Stun a unit at a battlefield.\"";
            EXPECT_EQ(o(enemy).damage_marked, 0);
        }
    }
}

// ═══ 9.G7: Dusk Rose Lab 765 — banished instead of dying is not killed ══════
// "you may kill a unit you control here to draw 1": the kill is the cost. A
// death replaced by "banish it instead" does not occur (CR 370.1.b,
// 370.1.c.1; the Deathgrip example of CR 359.3.e.14.b), so nothing was paid.
TEST_F(VerifyCardsTest, DuskRoseLab_AUnitBanishedInsteadOfDyingWasNotKilled_NoDraw) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);   // P1 keeps units (and control) here
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).banish_if_would_die_this_turn = true;        // Smite: "banish it instead"
    EnginePathAgent agent(EnginePathAgent::choosing(unit, EnginePathAgent::May::Yes));   // yes, then which unit
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(unit));
    EXPECT_TRUE(inBanishment(P1, unit)) << "the replacement applied";
    EXPECT_FALSE(inTrash(P1, unit));
    EXPECT_TRUE(onBoard(keeper)) << "the cost is attempted once";
    EXPECT_EQ(handSize(P1), 0) << "banished, not killed: no draw";
    EXPECT_EQ(deckSize(P1), 1);
}

// The same for a token, which is in the same place banished or dead (CR 183.1).
TEST_F(VerifyCardsTest, DuskRoseLab_ATokenBanishedInsteadOfDyingWasNotKilled_NoDraw) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);
    auto token = addUnit(P1, kShipyardSkulker, 0, 0);
    o(token).is_token = true;
    o(token).banish_if_would_die_this_turn = true;
    EnginePathAgent agent(EnginePathAgent::choosing(token, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 0) << "banished, not killed: no draw";
}

// ...and a token that does die pays the cost like any unit.
TEST_F(VerifyCardsTest, DuskRoseLab_KillingATokenPaysTheCost) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keeper = addUnit(P1, kShipyardSkulker, 0, 0);
    auto token = addUnit(P1, kShipyardSkulker, 0, 0);
    o(token).is_token = true;
    EnginePathAgent agent(EnginePathAgent::choosing(token, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(lab, TriggerType::AtStartOfBeginning, &agent);
    EXPECT_FALSE(onBoard(token));
    EXPECT_TRUE(onBoard(keeper));
    EXPECT_EQ(handSize(P1), 1) << "killed: draw 1";
}

// ═══ 9.G9: Elder Dragon 680 — its own Deflect filter asks ignoresDeflect() ══
// A pin: Elder Dragon's text has no "ignore [Deflect]", so ignoresDeflect()
// is false and [Deflect] (CR 809) is charged — a unit whose cost can't be paid
// is not offered. (The card's filter now reads the same hook
// Card::offerableChoices does; no card path makes it true for Elder Dragon.)
TEST_F(VerifyCardsTest, Pin_ElderDragon_DoesNotIgnoreDeflect) {
    const Card* card = card_registry.get(kElderDragon);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->ignoresDeflect()) << "nothing in its text says so";
    auto dragon = addUnit(P1, kElderDragon, 0, 1);
    auto plain = addUnit(P2, kShipyardSkulker, 0);        // in P2's base
    auto deflect = addUnit(P2, kIreliaFervent, 0, 0);     // [Deflect]: [A] to choose
    // P1 has no Power and no rune: the [A] can't be paid.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(plain.raw())}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "only the location with a unit that can be chosen";
    EXPECT_TRUE(agent.wasOffered(plain));
    EXPECT_FALSE(agent.wasOffered(deflect)) << "its [Deflect] can't be paid";
    EXPECT_TRUE(!onBoard(plain) || o(plain).damage_marked >= 1) << "\"Deal 1 to them\"";
    ASSERT_TRUE(onBoard(deflect));
    EXPECT_EQ(o(deflect).damage_marked, 0);
}

// ═══ Tranche 4: plans/09 items 9.I4 and 9.I5 ════════════════════════════════

// 9.I4: Back-Alley Bar 272 — "When a unit moves from here, give it +1 [M] this
// turn." A real move out of the Bar, through the move event (the positive
// control for the "a recall is not a move" tests).
TEST_F(VerifyCardsTest, BackAlleyBar_AUnitThatReallyMovesFromHereGetsPlusOne) {
    battlefieldCard(0, kBackAlleyBar);
    auto to_base = addUnit(P1, kShipyardSkulker, 0, 0);
    auto to_other = addUnit(P1, kShipyardSkulker, 0, 0);
    auto stays = addUnit(P1, kShipyardSkulker, 0, 0);
    auto from_elsewhere = addUnit(P1, kShipyardSkulker, 0, 1);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.moveToBase(to_base);
        exec.moveToBattlefield(to_other, BattlefieldId::fromRaw(1));
        exec.moveToBase(from_elsewhere);
    });
    ASSERT_TRUE(o(to_base).isAtBase());
    ASSERT_EQ(bfOf(to_other), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(o(to_base).temp_might_bonus, 1) << "moved from here to base";
    EXPECT_EQ(o(to_other).temp_might_bonus, 1) << "moved from here to another battlefield";
    EXPECT_EQ(o(stays).temp_might_bonus, 0);
    EXPECT_EQ(o(from_elsewhere).temp_might_bonus, 0) << "it did not move from the Bar";
}

// 9.I5: two Sorakas at one battlefield — each has her own "assigned combat
// damage last" once, and a death there is replaced once.
TEST_F(VerifyCardsTest, Soraka_TwoCopiesEachHaveBacklineOnceAndReplaceADeathOnce) {
    auto a = addUnit(P1, kSoraka, 4, 0);
    auto b = addUnit(P1, kSoraka, 4, 0);
    auto unit = addUnit(P1, kShipyardSkulker, 1, 0);
    int recalled = 0, deaths = 0;
    auto c1 = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.object == unit && e.what_changed == "recalled") ++recalled;
    });
    auto c2 = events.on_unit_died.connect([&](const UnitDiedEvent&) { ++deaths; });
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executor().killObject(unit);
        e.testHook_runChain();
    });
    c1.disconnect();
    c2.disconnect();
    EXPECT_TRUE(o(a).hasKeyword(Keyword::Backline));
    EXPECT_TRUE(o(b).hasKeyword(Keyword::Backline));
    EXPECT_EQ(o(a).aura_effects.size(), 1u) << "one effect from her own hook, not one per copy";
    EXPECT_EQ(o(b).aura_effects.size(), 1u);
    EXPECT_EQ(deaths, 0) << "instead heal it, exhaust it, and recall it";
    EXPECT_EQ(recalled, 1) << "replaced once, however many Sorakas are here";
    ASSERT_TRUE(o(unit).isAtBase());
    EXPECT_TRUE(o(unit).is_exhausted);
}


// ═══ Last tranche, group V1 ═══════════════════════════════════════════════════
// ═══ Verifier V1 (last tranche): Aphelios 372, Veteran Poro 421, Weaponmaster ═══

// 03 item 3.13 — 372 Aphelios, Exalted: the mode is claimed when it is chosen
// (as the trigger is finalized). After "make new choices for it"
// (GameEngine::makeNewChoicesFor, reached as Rebuttal reaches it) the SAME
// chain item is asked again, and its own earlier claim must not take its mode
// away from it.
TEST_F(VerifyCardsTest, ApheliosExalted_IsAskedAgainAfterNewChoicesAndIsNotBlockedByItsOwnClaim) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    // Finalization: mode 0 ("Ready 2 runes"). After the new choices: mode 0 again.
    EnginePathAgent agent(EnginePathAgent::script({0, 0}));
    GameEngine* live = nullptr;
    bool rechosen = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!rechosen && live && !live->mutableState().chain.items.empty()) {
            rechosen = true;
            auto& st = live->mutableState();
            EXPECT_EQ(st.getObject(aphelios).card_counters["__aph_mask"], 1)
                << "mode 0 is claimed as the ability is finalized";
            EXPECT_TRUE(st.getObject(r1).is_exhausted) << "nothing readied before it resolves";
            live->testHook_executor().makeNewChoices(st.chain.items.back().id);
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(aphelios, P1,
                                                    TriggerType::WhenEquipmentAttachedToMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent, &p2);
    ASSERT_TRUE(rechosen) << "the opponent never got priority";
    ASSERT_EQ(agent.prompts.size(), 2u) << "the mode, asked at finalization and again";
    EXPECT_EQ(agent.prompts[0].size(), 3u);
    EXPECT_EQ(agent.prompts[1].size(), 3u)
        << "the item's own earlier answer does not count as \"chosen this turn\" against it";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0));
    EXPECT_FALSE(o(r1).is_exhausted) << "the re-chosen ability still resolved: Ready 2 runes";
    EXPECT_FALSE(o(r2).is_exhausted);

    // Another trigger this turn: mode 0 has been chosen, once.
    EnginePathAgent later;
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &later);
    ASSERT_EQ(later.prompts.size(), 1u);
    EXPECT_EQ(later.prompts[0].size(), 2u);
    EXPECT_FALSE(promptOffersValue(later, 0, 0)) << "\"one that hasn't been chosen this turn\"";
}

// 08 S4 — 372 Aphelios, Exalted: "Buff a friendly unit" goes through
// EffectExecutor::buffUnit, so a unit that already has a Buff gets no second.
TEST_F(VerifyCardsTest, ApheliosExalted_BuffModeGivesOneBuffAndNoSecond) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto friendly = addUnit(P1, kShipyardSkulker, 0);
    EnginePathAgent first(EnginePathAgent::script({2, static_cast<int>(friendly.raw())}));
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &first);
    ASSERT_EQ(first.prompts.size(), 2u) << "the mode, then the friendly unit";
    EXPECT_TRUE(first.wasOffered(aphelios)) << "the unit is a choice, not the first found";
    ASSERT_EQ(o(friendly).buff_count, 1);
    EXPECT_EQ(o(aphelios).buff_count, 0);

    // The next turn the mode is free again; the unit still has its Buff.
    state.turn.turn_number += 1;
    EnginePathAgent second(EnginePathAgent::script({2, static_cast<int>(friendly.raw())}));
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &second);
    ASSERT_EQ(second.prompts.size(), 2u);
    EXPECT_EQ(second.prompts[0].size(), 3u) << "a new turn: every mode is offered";
    EXPECT_EQ(o(friendly).buff_count, 1) << "a unit holds one Buff (CR 427.1.b)";
}

// 03 item 3.21 — Veteran Poro 421 carries [Weaponmaster] and, like its
// reprint 779, no [Equip] of its own.
TEST_F(VerifyCardsTest, Pin_3_21_VeteranPoro421_HasWeaponmasterAndNoEquip) {
    const auto& def = card_db.get(kVeteranPoro421);
    EXPECT_TRUE(def.keywords.has(Keyword::Weaponmaster));
    EXPECT_FALSE(def.keywords.has(Keyword::Equip))
        << "a Weaponmaster unit has no Equip ability of its own";
    const auto& reprint = card_db.get(kVeteranPoro779);
    EXPECT_EQ(def.keywords.has(Keyword::Weaponmaster), reprint.keywords.has(Keyword::Weaponmaster));
    EXPECT_EQ(def.keywords.has(Keyword::Equip), reprint.keywords.has(Keyword::Equip));
}

// 07 item 7.23 — 828 Riven, Shattered overrides onTrigger and hands the play
// trigger to the Weaponmaster base. Played through the engine with two
// Equipment on the board: the "may" can be declined, and nothing is charged.
TEST_F(VerifyCardsTest, RivenShattered_WeaponmasterIsAMay_DecliningAttachesAndChargesNothing) {
    auto sword = addGear(P1, kLongSword);            // [Equip] [R]    -> free
    auto skyfall = addGear(P1, kSkyfallOfAreion);    // [Equip] [1][R] -> [1]
    auto riven = addToHand(P1, kRivenShattered);
    ps(P1).rune_pool.energy = 1;                     // beyond Riven's own cost
    EnginePathAgent no(EnginePathAgent::script({0}));
    playThroughEngine(riven, &no);
    ASSERT_TRUE(onBoard(riven));
    ASSERT_EQ(no.prompts.size(), 1u) << "the may is asked, and only the may";
    EXPECT_EQ(no.prompts[0].size(), 2u) << "yes / no";
    EXPECT_FALSE(o(sword).attached_to.has_value());
    EXPECT_FALSE(o(skyfall).attached_to.has_value());
    EXPECT_TRUE(o(riven).attachments.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "no Equip cost for an Equip that did not happen";
}

// ... and accepted, the controller chooses which Equipment (here the second,
// not the first found) and pays its Equip cost with one Power taken off.
TEST_F(VerifyCardsTest, RivenShattered_WeaponmasterEquipsTheChosenEquipmentForItsCostLessOnePower) {
    auto sword = addGear(P1, kLongSword);            // [Equip] [R]    -> free
    auto skyfall = addGear(P1, kSkyfallOfAreion);    // [Equip] [1][R] -> [1]
    auto riven = addToHand(P1, kRivenShattered);
    ps(P1).rune_pool.energy = 1;                     // beyond Riven's own cost: the [1]
    EnginePathAgent yes(EnginePathAgent::choosing(skyfall, EnginePathAgent::May::Yes));
    playThroughEngine(riven, &yes);
    ASSERT_TRUE(onBoard(riven));
    ASSERT_EQ(yes.prompts.size(), 2u) << "the may, then which Equipment";
    EXPECT_TRUE(yes.wasOffered(sword)) << "every Equipment she could equip is a choice";
    EXPECT_TRUE(yes.wasOffered(skyfall))
        << "[1][R] with no Power left: offered only because [A] comes off the cost";
    EXPECT_EQ(o(skyfall).attached_to, std::optional<GameObjectId>{riven});
    EXPECT_FALSE(o(sword).attached_to.has_value()) << "not the first Equipment found";
    EXPECT_EQ(o(riven).attachments.size(), 1u);
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "the [1] of the Equip cost was charged";
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0);
    EXPECT_FALSE(state.chain.exists());
}

// 828 Riven, Shattered — her own branch through the engine: "When I attack,
// choose an enemy unit here. Deal 2 to it for each Equipment attached to me."
TEST_F(VerifyCardsTest, RivenShattered_AttackDealsTwoPerAttachedEquipmentToTheChosenEnemyHere) {
    auto riven = addUnit(P1, kRivenShattered, 0, 0);
    auto sword = addGear(P1, kLongSword, 0);
    auto dirk = addGear(P1, kSerratedDirk, 0);
    auto trinket = addGear(P1, kInvalidId, 0);       // a gear that is not an Equipment
    attach(sword, riven);
    attach(dirk, riven);
    attach(trinket, riven);
    auto first = addUnit(P2, kShipyardSkulker, 9, 0);
    auto chosen = addUnit(P2, kShipyardSkulker, 9, 0);
    auto elsewhere = addUnit(P2, kShipyardSkulker, 9, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(chosen));
    fireTriggerThroughEngine(riven, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the enemy unit here";
    EXPECT_TRUE(agent.wasOffered(first));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "an enemy unit HERE";
    EXPECT_EQ(o(chosen).damage_marked, 4) << "2 for each of the two Equipment";
    EXPECT_EQ(o(first).damage_marked, 0);
    EXPECT_EQ(o(elsewhere).damage_marked, 0);
}

// 09 item 9.B2 (replaces the tail of C5Test.WeaponmasterUnitsUseTheSharedMay,
// which passed no agent and so declined the may whatever the card did) and
// 08 S2 / 07 item 7.26: a Weaponmaster unit that is no longer on the board
// when its play trigger is finalized is not asked anything and gets nothing.
// The agent accepts and would choose the Equipment; its Equip costs nothing
// under Weaponmaster (Long Sword, "[Equip] [R]"), so nothing but the base's own
// on-board check keeps it off the list.
// Corrected (sweep-costs, 9.N5): the Equipment has a card behind it — one with
// none has no Equip cost and can't be equipped this way (CR 821.1.c.4).
TEST_F(VerifyCardsTest, Weaponmaster_AUnitAlreadyOffTheBoardIsNotAskedAndNotEquipped) {
    auto bare = addGear(P1, kInvalidId);
    o(bare).tags = {"Equipment"};
    withEquipmentCard(bare);

    // Control: on the board, the same agent is asked and equips it.
    auto control = addUnit(P1, kVeteranPoro421, 0);
    EnginePathAgent asked(EnginePathAgent::choosing(bare, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(control, TriggerType::WhenYouPlayMe, &asked);
    ASSERT_EQ(asked.prompts.size(), 2u) << "the may, then the Equipment";
    ASSERT_EQ(o(bare).attached_to, std::optional<GameObjectId>{control});

    for (CardDefId def : {kArmedAssailant, kCombatChef, kVeteranPoro421, kMasterBingwen,
                          kOrnnForgeGod, kLucianMerciless, kSentinelAdept, kYoneBlademaster,
                          kRivenShattered, kVeteranPoro779}) {
        auto dead = addUnit(P1, def, 0);
        sendToTrash(dead);
        EnginePathAgent agent(EnginePathAgent::choosing(bare, EnginePathAgent::May::Yes));
        fireTriggerThroughEngine(dead, TriggerType::WhenYouPlayMe, &agent);
        EXPECT_TRUE(agent.prompts.empty()) << def << ": nothing to offer a unit that left the board";
        EXPECT_EQ(o(bare).attached_to, std::optional<GameObjectId>{control})
            << def << ": the Equipment stays where it was (even if it's already attached)";
        EXPECT_TRUE(onBoard(bare)) << def;
        EXPECT_TRUE(o(dead).attachments.empty()) << def;
        EXPECT_EQ(o(control).attachments.size(), 1u) << def;
    }
}

// 08 S2 (324, 414, 421, 448, 407, 435) and 07 item 7.26 (331, 544): the may is
// accepted and the Equipment chosen as the ability is finalized; the unit
// leaves the board in response. As the ability resolves the Equipment is not
// attached to the unit in the trash and its Equip cost is not charged.
TEST_F(VerifyCardsTest, Weaponmaster_AUnitThatLeftInResponseIsNotEquippedAndPaysNothing) {
    // Control: the unit stays, the [1] is paid and the Equipment attached.
    auto control_gear = addGear(P1, kSkyfallOfAreion);   // [Equip] [1][R] -> [1]
    auto control = addUnit(P1, kVeteranPoro421, 0);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent stays(EnginePathAgent::choosing(control_gear, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(control, TriggerType::WhenYouPlayMe, &stays);
    ASSERT_EQ(o(control_gear).attached_to, std::optional<GameObjectId>{control});
    ASSERT_EQ(ps(P1).rune_pool.energy, 0);

    auto skyfall = addGear(P1, kSkyfallOfAreion);
    for (CardDefId def : {kArmedAssailant, kCombatChef, kVeteranPoro421, kMasterBingwen,
                          kOrnnForgeGod, kLucianMerciless, kSentinelAdept, kYoneBlademaster,
                          kRivenShattered, kVeteranPoro779}) {
        auto unit = addUnit(P1, def, 0);
        ps(P1).rune_pool.energy = 1;
        EnginePathAgent agent(EnginePathAgent::choosing(skyfall, EnginePathAgent::May::Yes));
        fireAndProbe(unit, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
            EXPECT_FALSE(live.getObject(skyfall).attached_to.has_value())
                << def << ": attached as the ability resolves, not before";
            removeFromBoard(live, unit);
        });
        ASSERT_EQ(agent.prompts.size(), 2u)
            << def << ": the may was accepted and the Equipment chosen at finalization";
        EXPECT_FALSE(onBoard(unit)) << def;
        EXPECT_FALSE(o(skyfall).attached_to.has_value())
            << def << ": no Equipment follows a unit to the trash";
        EXPECT_TRUE(onBoard(skyfall)) << def;
        EXPECT_TRUE(o(skyfall).isAtBase()) << def << ": it has not moved";
        EXPECT_TRUE(o(unit).attachments.empty()) << def;
        EXPECT_EQ(ps(P1).rune_pool.energy, 1) << def << ": no Equip cost for no Equip";
    }
}


// ═══ Last tranche, group V3 ═══════════════════════════════════════════════════
// ═══ Last tranche (V3): boxes still open in batch-00 / 08 / 09 / 10 ══════════

// ── batch-00 "Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)" ──
// Each gear's declared Equip cost is the printed one: the Equip is not offered
// with every resource except the right Power, nor one Energy short; with
// exactly the printed cost it is offered once, and executing it attaches the
// gear and spends exactly that.
TEST_F(VerifyCardsTest, Equip_EachOfTheTenGearsIsOfferedAndPaidAtItsPrintedCost) {
    struct Row { CardDefId id; int energy; Domain domain; const char* printed; };
    const std::vector<Row> rows = {
        {kSerratedDirk, 0, Domain::Fury, "[R]"},
        {kRecurveBow, 0, Domain::Fury, "[R]"},
        {kLongSword, 0, Domain::Fury, "[R]"},
        {kSkyfallOfAreion, 1, Domain::Fury, "[1][R]"},
        {kDoransShield, 0, Domain::Calm, "[G]"},
        {kBrutalizer, 0, Domain::Calm, "[G]"},
        {kGuardianAngel, 0, Domain::Calm, "[G]"},
        {kSteraksGage, 0, Domain::Calm, "[G]"},
        {kSvellsongur, 1, Domain::Calm, "[1][G]"},
        {kClothArmor, 0, Domain::Mind, "[B]"},
    };
    auto offers = [&](GameObjectId gear, GameObjectId unit) {
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.type == IntentType::ActivateAbility && i.ability_source == gear &&
                    !i.targets.empty() && i.targets.front() == unit)
                    out.push_back(i);
        });
        return out;
    };
    for (const Row& row : rows) {
        SCOPED_TRACE("gear " + toString(row.id) + " [Equip] " + row.printed);
        // The row is the printed cost.
        EXPECT_NE(card_db.get(row.id).ability_text.find(std::string("[Equip] ") + row.printed + " ("),
                  std::string::npos);
        auto unit = addUnit(P1, kShipyardSkulker);
        auto gear = addGear(P1, row.id);

        // Plenty of Energy and one Power of every OTHER domain: not offered.
        ps(P1).rune_pool = RunePool{};
        ps(P1).rune_pool.energy = row.energy + 3;
        for (int d = 0; d < static_cast<int>(Domain::Count); ++d)
            if (d != static_cast<int>(row.domain)) ps(P1).rune_pool.power[d] = 1;
        EXPECT_TRUE(offers(gear, unit).empty()) << "offered without the printed Power";

        // The right Power, one Energy short: not offered.
        if (row.energy > 0) {
            ps(P1).rune_pool = RunePool{};
            power(P1, row.domain) = 1;
            ps(P1).rune_pool.energy = row.energy - 1;
            EXPECT_TRUE(offers(gear, unit).empty()) << "offered one Energy short";
        }

        // Exactly the printed cost: offered once, and paying it attaches.
        ps(P1).rune_pool = RunePool{};
        power(P1, row.domain) = 1;
        ps(P1).rune_pool.energy = row.energy;
        const auto legal = offers(gear, unit);
        ASSERT_EQ(legal.size(), 1u);
        const Intent equip = legal.front();
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(equip); });
        EXPECT_EQ(o(gear).attached_to, std::optional<GameObjectId>{unit});
        EXPECT_EQ(ps(P1).rune_pool.energy, 0);
        EXPECT_EQ(power(P1, row.domain), 0);
        EXPECT_EQ(ps(P1).rune_pool.totalPower(), 0);
    }
}

// ── batch-09 "\"Can't be chosen by enemy spells and abilities\" ignored" ──

// 511 Defiant Dance: the plan named a test that has no unchoosable unit in it.
TEST_F(VerifyCardsTest, DefiantDance_DoesNotOfferAnEnemyUnitThatCannotBeChosen) {
    auto spell = addToHand(P1, kDefiantDance);
    auto mine = addUnit(P1, kInvalidId, 3);
    auto plain = addUnit(P2, kInvalidId, 5, 0);
    auto shrouded = addUnit(P2, kInvalidId, 5, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    // +2 to `mine`; the -2 is tried on the shrouded unit, then on `plain`.
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const GameObjectId want = asked++ == 0 ? mine : shrouded;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == plain) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "a unit, then another unit";
    EXPECT_FALSE(agent.wasOffered(shrouded)) << "can't be chosen by enemy spells";
    EXPECT_TRUE(agent.wasOffered(plain));
    EXPECT_EQ(o(mine).temp_might_bonus, 2);
    EXPECT_EQ(o(plain).temp_might_bonus, -2);
    EXPECT_EQ(o(shrouded).temp_might_bonus, 0);
}

// 539 Ahri, Inquisitive (the reprint of 119, outside the c4 range).
TEST_F(VerifyCardsTest, Ahri539_DoesNotOfferAnEnemyUnitThatCannotBeChosen) {
    auto ahri = addUnit(P1, kAhri539, 0, 0);
    auto open = addUnit(P2, kInvalidId, 4, 0);
    auto shrouded = addUnit(P2, kInvalidId, 4, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    EnginePathAgent agent(EnginePathAgent::choosing(shrouded));
    fireTriggerThroughEngine(ahri, TriggerType::WhenIAttackOrDefend, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_TRUE(agent.wasOffered(open));
    EXPECT_FALSE(agent.wasOffered(shrouded));
    EXPECT_EQ(o(open).temp_might_bonus, -2);
    EXPECT_EQ(o(shrouded).temp_might_bonus, 0);
}

// 200 Twisted Fate, Gambler: both targeted payoffs. The [R] "1 to all other
// enemy units here" is not a choice, so the unchoosable unit still takes it.
TEST_F(VerifyCardsTest, TwistedFateGambler_NeitherPayoffOffersAUnitThatCannotBeChosen) {
    auto tf = addUnit(P1, kTwistedFateGambler, 0, 0);
    auto open = addUnit(P2, kInvalidId, 5, 0);
    auto open2 = addUnit(P2, kInvalidId, 5, 0);
    auto shrouded = addUnit(P2, kInvalidId, 5, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    auto away = addUnit(P2, kInvalidId, 5, 1);
    auto topRune = [&](Domain d) {
        auto r = addRune(P1, d);
        o(r).location = std::nullopt;
        o(r).zone = ZoneType::RuneDeck;
        ps(P1).rune_deck.clear();
        ps(P1).rune_deck.push_back(r);
    };

    // [R] — Deal 2 to an enemy unit here and 1 to all other enemy units here.
    topRune(Domain::Fury);
    EnginePathAgent fury([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == shrouded) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == open) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &fury);
    ASSERT_EQ(fury.prompts.size(), 1u);
    EXPECT_FALSE(fury.wasOffered(shrouded)) << "[R]";
    EXPECT_TRUE(fury.wasOffered(open));
    EXPECT_TRUE(fury.wasOffered(open2));
    EXPECT_FALSE(fury.wasOffered(away)) << "\"an enemy unit here\"";
    EXPECT_EQ(o(open).damage_marked, 2);
    EXPECT_EQ(o(open2).damage_marked, 1);
    EXPECT_EQ(o(shrouded).damage_marked, 1) << "\"all other enemy units here\" chooses nobody";
    EXPECT_EQ(o(away).damage_marked, 0);

    // [Y] — Stun an enemy unit.
    topRune(Domain::Order);
    EnginePathAgent order([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == shrouded) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == away) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &order);
    ASSERT_EQ(order.prompts.size(), 1u);
    EXPECT_FALSE(order.wasOffered(shrouded)) << "[Y]";
    EXPECT_TRUE(order.wasOffered(open));
    EXPECT_TRUE(order.wasOffered(away)) << "\"an enemy unit\": anywhere";
    EXPECT_TRUE(o(away).is_stunned);
    EXPECT_FALSE(o(shrouded).is_stunned);
}

// ── batch-10 "459 / 585 / 674 / 681 / 878 ... can't be chosen" ──

// 459 Windsinger: no test had an unchoosable unit.
TEST_F(VerifyCardsTest, Windsinger_DoesNotOfferAnEnemyUnitThatCannotBeChosen) {
    auto open = addUnit(P2, kInvalidId, 2, 0);
    auto shrouded = addUnit(P2, kInvalidId, 2, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    auto singer = addUnit(P1, kWindsinger);
    // Yes; then the shrouded unit if it is offered, else `open`.
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == shrouded) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == open) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(singer, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(shrouded));
    EXPECT_TRUE(agent.wasOffered(open));
    EXPECT_TRUE(inHand(P2, open));
    EXPECT_TRUE(onBoard(shrouded));

    // Only the unchoosable unit is left at a battlefield: nothing to return,
    // so the "may" is not offered.
    auto second = addUnit(P1, kWindsinger);
    AcceptingEnginePathAgent none;
    fireTriggerThroughEngine(second, TriggerType::WhenYouPlayMe, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(onBoard(shrouded));
}

// 674 Irresistible Faefolk: the plan's test had no unchoosable unit.
TEST_F(VerifyCardsTest, Faefolk_DoesNotOfferAnEnemyUnitThatCannotBeChosen) {
    auto fae = addUnit(P1, kFaefolk, 0, 0);
    auto open = addUnit(P2, kInvalidId, 2, 1);
    auto shrouded = addUnit(P2, kInvalidId, 2, 1);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == shrouded) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == open) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(fae, TriggerType::WhenIMoveToFB, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(shrouded));
    EXPECT_TRUE(agent.wasOffered(open));
    EXPECT_EQ(bfOf(open), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(shrouded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});

    // Only the unchoosable unit is elsewhere now: the "may" is not offered.
    AcceptingEnginePathAgent none;
    fireTriggerThroughEngine(fae, TriggerType::WhenIMoveToFB, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(bfOf(shrouded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// 681 Kha'Zix, Evolving Hunter: no test had an unchoosable unit.
TEST_F(VerifyCardsTest, KhaZix_DoesNotOfferAnEnemyUnitThatCannotBeChosen_AndKeepsTheXp) {
    auto kha = addUnit(P1, kKhaZixHunter, 0, 0);
    auto open = addUnit(P2, kInvalidId, 9, 0);
    auto shrouded = addUnit(P2, kInvalidId, 9, 0);
    o(shrouded).untargetable_by_enemy_this_turn = true;
    o(shrouded).untargetable_by_enemy = true;
    setXp(P1, 3);
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == shrouded) return i;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == open) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    fireTriggerThroughEngine(kha, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(shrouded));
    EXPECT_EQ(o(shrouded).damage_marked, 0);
    EXPECT_EQ(o(open).damage_marked, o(kha).current_might);
    EXPECT_EQ(ps(P1).xp, 0);

    // Only the unchoosable unit is here: nothing to deal damage to, so the
    // 3 XP is not asked for and not spent.
    removeFromBoard(state, open);
    setXp(P1, 3);
    AcceptingEnginePathAgent none;
    fireTriggerThroughEngine(kha, TriggerType::WhenIAttack, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(ps(P1).xp, 3);
    EXPECT_EQ(o(shrouded).damage_marked, 0);
}

// ── batch-09 "Effect-played units always land in base" ──

// 37 Immortal Phoenix: its tests only ever played it to the base (no
// battlefield was controlled), so "always lands in base" passed them.
TEST_F(VerifyCardsTest, ImmortalPhoenix_CanBePlayedToAControlledBattlefield) {
    auto phoenix = addToTrash(P1, kImmortalPhoenix);
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 1);                            // holds battlefield #1
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Fury) = 1;
    // Yes; battlefield #1 (pickEffectPlayLocation: mode 0 = base, 1 + i).
    EnginePathAgent agent(EnginePathAgent::script({1, 2}));
    fireTriggerThroughEngine(phoenix, TriggerType::WhenYouKillAUnitWithASpell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then where";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 1, 1)) << "battlefield #0 is not controlled";
    ASSERT_TRUE(onBoard(phoenix));
    EXPECT_EQ(bfOf(phoenix), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_FALSE(inTrash(P1, phoenix));
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
    EXPECT_EQ(power(P1, Domain::Fury), 0);
}

// 194 Nocturne, Horrifying: builds its own mode list (0 = stay banished,
// 1 = base, 2 + i = battlefield i); its tests only ever chose the base.
TEST_F(VerifyCardsTest, Nocturne_CanBePlayedToAControlledBattlefieldButNotWhereUnitsCannotBePlayed) {
    auto nocturne = addToDeck(P1, kNocturneHorrifying);
    state.battlefields[0].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // a unit holding each
    addUnit(P1, kInvalidId, 1, 1);
    ps(P1).rune_pool.universal_power = 1;
    EnginePathAgent agent(EnginePathAgent::script({1, 3}));   // banish; play to battlefield #1
    // (Testing phase D71, by design: a replacement as he is seen, no longer a
    // triggered ability fired by hand.)
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.seenOnTopOfDeck(P1, nocturne); },
                         &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the banish, then whether and where to play";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "stay banished";
    EXPECT_TRUE(promptOffersValue(agent, 1, 1)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 1, 2)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 1, 3));
    ASSERT_TRUE(onBoard(nocturne));
    EXPECT_EQ(bfOf(nocturne), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_FALSE(inDeck(P1, nocturne));
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "\"play me for [A]\"";
}

// 730 Undying Loyalty. Its tests only ever chose the base. The three tests
// below judge where the unit ends up, not how the modes are numbered: the
// agent takes the trash unit, then any "where" answer that is not the base.
TEST_F(VerifyCardsTest, UndyingLoyalty_TheUnitCanBePlayedToAControlledBattlefield) {
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    state.battlefields[1].controller = P1;
    addUnit(P1, kInvalidId, 1, 1);                            // holds battlefield #1
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == poro) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value != 0) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    ASSERT_TRUE(onBoard(poro));
    EXPECT_EQ(bfOf(poro), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)})
        << "CR 355.2.a: base or a battlefield you control";
    EXPECT_FALSE(inTrash(P1, poro));
}

TEST_F(VerifyCardsTest, UndyingLoyalty_DoesNotOfferABattlefieldUnitsCannotBePlayedTo) {
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    state.battlefields[0].controller = P1;                    // the only one P1 controls
    state.battlefields[0].blocks_unit_play = true;            // Rockfall Path
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == poro) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value != 0) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    for (const auto& prompt : agent.prompts)
        for (const auto& i : prompt)
            EXPECT_FALSE(i.chosen_value.has_value() && *i.chosen_value != 0)
                << "a place other than the base was offered: " << i.choice_label;
    ASSERT_TRUE(onBoard(poro));
    EXPECT_TRUE(o(poro).isAtBase()) << "units can't be played to battlefield #0";
}

TEST_F(VerifyCardsTest, UndyingLoyalty_APlayerWhoMayOnlyPlayUnitsToBaseIsOfferedOnlyTheBase) {
    auto spell = addToHand(P1, kUndyingLoyalty);
    auto poro = addToTrash(P1, kPoutyPoro);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);                            // holds battlefield #0
    addUnit(P2, kMageseekerWarden, 0, 1);                     // "opponents can only play units to their base"
    ps(P1).units_play_base_only = true;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == poro) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value != 0) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    for (const auto& prompt : agent.prompts)
        for (const auto& i : prompt)
            EXPECT_FALSE(i.chosen_value.has_value() && *i.chosen_value != 0)
                << "a place other than the base was offered: " << i.choice_label;
    ASSERT_TRUE(onBoard(poro));
    EXPECT_TRUE(o(poro).isAtBase()) << "battlefield #0 is controlled, but units may only go to the base";
}

// ═══ Last tranche, group V2 ═══════════════════════════════════════════════════
// ═══ Last tranche (after sweep-costs merged) ═════════════════════════════════

// 08 S3: two Ornn, Forge God under one controller — "I have +1 [M] for each
// friendly gear" is applied to each of them once (the hook is called once per
// (card, controller) and visits every instance).
TEST_F(VerifyCardsTest, OrnnForgeGod_TwoCopiesEachCountTheFriendlyGearOnce) {
    auto a = addUnit(P1, kOrnnForgeGod, 0);          // printed 4 [M]
    auto b = addUnit(P1, kOrnnForgeGod, 0);
    addGear(P1, kInvalidId);
    addGear(P1, kInvalidId);
    addGear(P2, kInvalidId);                         // not friendly
    cleanup();
    auto fromSelf = [&](GameObjectId id) {
        int n = 0;
        for (const auto& ae : o(id).aura_effects) if (ae.source == id) n += ae.might_bonus;
        return n;
    };
    EXPECT_EQ(fromSelf(a), 2);
    EXPECT_EQ(fromSelf(b), 2);
    EXPECT_EQ(o(a).aura_effects.size(), 1u) << "one effect from her own hook, not one per copy";
    EXPECT_EQ(o(b).aura_effects.size(), 1u);
    EXPECT_EQ(o(a).current_might, 6) << "4 + two friendly gear";
    EXPECT_EQ(o(b).current_might, 6);
}

// 08 S3: Risen Altar in both battlefield slots, both controlled by one player
// — the hook runs once per slot and each run visits every Altar, yet a unit
// there gets the [Empower] discount once.
TEST_F(VerifyCardsTest, RisenAltar_InBothSlotsTheDiscountIsOnePerUnit) {
    battlefieldCard(0, kRisenAltar, P1);
    battlefieldCard(1, kRisenAltar, P1);
    auto here0 = addUnit(P1, kInvalidId, 2, 0);
    auto here1 = addUnit(P1, kInvalidId, 2, 1);
    auto in_base = addUnit(P1, kInvalidId, 2);
    cleanup();
    ASSERT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
    ASSERT_EQ(state.battlefields[1].controller, std::optional<PlayerId>{P1});
    EXPECT_EQ(state.battlefields[0].empower_cost_reduction[0], 1);
    EXPECT_EQ(state.battlefields[1].empower_cost_reduction[0], 1);
    EXPECT_EQ(state.battlefields[0].empower_cost_reduction[1], 0) << "\"your units here\"";
    EXPECT_EQ(o(here0).empower_cost_reduction, 1) << "[1] or [A] less, once";
    EXPECT_EQ(o(here1).empower_cost_reduction, 1);
    EXPECT_EQ(o(in_base).empower_cost_reduction, 0) << "\"here\"";
    cleanup();                                       // a second pass adds nothing
    EXPECT_EQ(o(here0).empower_cost_reduction, 1);
    EXPECT_EQ(o(here1).empower_cost_reduction, 1);
}

// 08 S5: Bard, Mercurial — which open battlefield and which units are the
// controller's choices: the second of two open battlefields, the second unit,
// and no more.
TEST_F(VerifyCardsTest, BardMercurial_TheSecondOpenBattlefieldAndALaterUnitWhenThoseAreThePicks) {
    auto legend = addLegend(P1, kPurifier);
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto bard = addToHand(P1, kBardMercurial);
    // Yes to the additional cost; battlefield #1 (mode 1); u2; then "None".
    EnginePathAgent agent(EnginePathAgent::script({1, 1, static_cast<int>(u2.raw()), 0}));
    playThroughEngine(bard, &agent);
    ASSERT_TRUE(onBoard(bard));
    EXPECT_TRUE(o(legend).is_exhausted) << "the additional cost";
    ASSERT_GE(agent.prompts.size(), 4u) << "the cost, the battlefield, a unit, a unit or none";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "both open battlefields are offered";
    EXPECT_TRUE(promptOffersValue(agent, 1, 1));
    EXPECT_TRUE(agent.wasOffered(u1));
    EXPECT_EQ(bfOf(u2), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_TRUE(o(u1).isAtBase()) << "any number: not every unit";
    EXPECT_TRUE(o(bard).isAtBase());
}

// 08 S5: the Weaponmaster choice of Equipment (weaponmaster_base.h) — every
// unit that takes the ability from the base asks which Equipment, and takes
// the one picked when it is not the first on offer.
TEST_F(VerifyCardsTest, Weaponmaster_EveryUnitOfTheBaseTakesTheEquipmentThePlayerPicks) {
    for (CardDefId def : {kArmedAssailant, kCombatChef, kVeteranPoro421, kMasterBingwen,
                          kOrnnForgeGod, kLucianMerciless, kVeteranPoro779}) {
        auto first = addGear(P1, kLongSword);        // [Equip] [R]: free, one Power less
        auto second = addGear(P1, kLongSword);
        auto unit = addToHand(P1, def);
        EnginePathAgent agent(EnginePathAgent::choosing(second, EnginePathAgent::May::Yes));
        playThroughEngine(unit, &agent);
        ASSERT_TRUE(onBoard(unit)) << def;
        EXPECT_TRUE(agent.wasOffered(first)) << def << ": a prompt over the player's Equipment";
        EXPECT_EQ(o(second).attached_to, std::optional<GameObjectId>{unit}) << def;
        EXPECT_FALSE(o(first).attached_to.has_value()) << def << ": not the first match";
    }
}

// 08 S5: Sky Crusier — "Discard a gear, [1], [E]": which gear is discarded is
// the player's choice (a non-gear card is not on offer), and so is the unit.
TEST_F(VerifyCardsTest, SkyCrusier_DiscardsTheGearThePlayerPicksAndHitsTheChosenUnit) {
    auto cruiser = addUnit(P1, kSkyCrusier);
    auto g1 = addToHand(P1, kLongSword);
    auto spell = addToHand(P1, kMeditation);
    auto g2 = addToHand(P1, kSerratedDirk);
    auto t1 = addUnit(P2, kInvalidId, 6, 0);
    auto t2 = addUnit(P2, kInvalidId, 6, 0);
    auto in_base = addUnit(P2, kInvalidId, 6);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (GameObjectId want : {g2, t2})
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    activate(cruiser, &agent);
    EXPECT_TRUE(o(cruiser).is_exhausted);
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
    EXPECT_TRUE(agent.wasOffered(g1)) << "a prompt over the gear in hand";
    EXPECT_FALSE(agent.wasOffered(spell)) << "Discard a GEAR";
    EXPECT_TRUE(inTrash(P1, g2));
    EXPECT_TRUE(inHand(P1, g1)) << "not the first gear in hand";
    EXPECT_TRUE(inHand(P1, spell));
    EXPECT_TRUE(agent.wasOffered(t1));
    EXPECT_FALSE(agent.wasOffered(in_base)) << "a unit at a battlefield";
    EXPECT_EQ(o(t2).damage_marked, 4);
    EXPECT_EQ(o(t1).damage_marked, 0);
}

// 08 S5 / batch-07 732: Atakhan — which friendly unit is killed as the
// additional cost is the player's choice, and the discount is that unit's
// printed cost.
TEST_F(VerifyCardsTest, Atakhan_TheAdditionalCostKillsTheUnitThePlayerPicks) {
    auto mech = addUnit(P1, kMegaMech);              // printed [7]
    auto skulker = addUnit(P1, kShipyardSkulker);    // printed [3]
    auto atakhan = addToHand(P1, kAtakhan);          // [10] + 3 Power
    ps(P1).rune_pool = RunePool{};
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(skulker.raw())}));
    playAt(atakhan, {}, &agent);                     // funded with exactly [10] + 3
    ASSERT_TRUE(onBoard(atakhan));
    EXPECT_TRUE(agent.wasOffered(mech));
    EXPECT_FALSE(agent.wasOffered(atakhan)) << "chosen before I am on the board";
    EXPECT_FALSE(onBoard(skulker));
    EXPECT_TRUE(onBoard(mech)) << "not the first friendly unit";
    EXPECT_EQ(ps(P1).rune_pool.energy, 3) << "[10] less the Skulker's [3]";
}

// batch-05 462 Irelia, Graceful: "cost [1] or [A] less" — the player picks the
// half when both apply; the Power half takes one Power off and no Energy.
TEST_F(VerifyCardsTest, IreliaGraceful_TheDiscountIsTakenAsPowerOrAsEnergy) {
    auto irelia = addUnit(P1, kIreliaGraceful, 0);
    cleanup();                                       // her continuous effect
    {
        auto spell = addToHand(P1, kPunchFirst);     // [1] + 2 Power
        ps(P1).rune_pool = RunePool{};
        EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(irelia.raw()), 1}));
        playAt(spell, {}, &agent);                   // funded with exactly [1] + 2
        ASSERT_GE(agent.prompts.size(), 2u) << "the target, then [1] or [A]";
        EXPECT_TRUE(promptOffersValue(agent, 1, 0));
        EXPECT_TRUE(promptOffersValue(agent, 1, 1));
        EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "the Energy was paid in full";
        EXPECT_EQ(ps(P1).rune_pool.universal_power, 1) << "[A] less";
        EXPECT_EQ(o(irelia).temp_might_bonus, 5) << "the spell resolved";
    }
    {
        auto spell = addToHand(P1, kPunchFirst);
        ps(P1).rune_pool = RunePool{};
        EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(irelia.raw()), 0}));
        playAt(spell, {}, &agent);
        EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "[1] less";
        EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "the Power was paid in full";
    }
}

// 9.J6 / batch-10 728 Stalking Wolf: the kill is a cost of the play. The
// engine's own [Ambush] offer to the victim's battlefield — where the victim
// is the player's only unit — is executed as offered: the victim is off the
// board when the Wolf enters, and the Wolf still lands there.
TEST_F(VerifyCardsTest, StalkingWolf_AmbushedToItsVictimsBattlefield_TheVictimIsGoneWhenItLands) {
    auto wolf = addToHand(P1, kStalkingWolf);
    auto poro = addUnit(P1, kInvalidId, 1, 0);       // my only unit at battlefield #0
    o(poro).tags = {"Poro"};
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.priority_holder = P1;
    std::optional<Intent> ambush;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.card == wolf && i.play_location &&
                *i.play_location == LocationId{BattlefieldLocation{0}}) ambush = i;
    });
    state.turn.oc_state = OpenClosedState::Open;
    state.turn.priority_holder = std::nullopt;
    ASSERT_TRUE(ambush.has_value()) << "[Ambush] to its battlefield is offered";

    GameState* live = nullptr;
    int entered = 0;
    bool victim_on_board_at_entry = true;
    auto conn = events.on_entered_board.connect([&](const EnteredBoardEvent& e) {
        if (e.object != wolf || !live) return;
        ++entered;
        victim_on_board_at_entry = live->getObject(poro).location.has_value();
    });
    EnginePathAgent agent(EnginePathAgent::choosing(poro));
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(*ambush);
        e.testHook_runChain();
        live = nullptr;
    }, &agent);
    conn.disconnect();
    EXPECT_EQ(entered, 1);
    EXPECT_FALSE(victim_on_board_at_entry) << "killed as I am played, before I enter";
    EXPECT_FALSE(onBoard(poro));
    EXPECT_EQ(bfOf(wolf), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "even if you don't have other units there";
}


// ═══ Tranche 5, group T5A ══════════════════════════════════════════════════════
// Already defined in the file and used below: kBlitzcrank (67), kFaefolk (674),
// kJaggedCutlass (860), kShipyardSkulker (175), kLongSword (345).

// ═══ Tranche 5, group A: 9.M28 / 9.M31 / 9.M33 / 9.M34 / 9.M37 / 9.M38 / 9.N4 ═══

// ─── 9.N4: enemy units that can't be moved are not offered ──────────────────

// 878 Corrupted Dragon: "move ... enemy units here ... to their base".
TEST_F(VerifyCardsTest, CorruptedDragon_DoesNotOfferAnEnemyUnitItCannotMoveToBase) {
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    auto guarded = addUnit(P2, kInvalidId, 2, 0);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets
    auto rooted = addUnit(P2, kInvalidId, 2, 0);
    o(rooted).cant_move_to_base = true;                  // as Determined Sentry sets it
    auto free_unit = addUnit(P2, kInvalidId, 2, 0);
    // Batch M1: the leading "you may" is a prompt of its own (CR 383.3.a):
    // yes, then the unit (was one prompt).
    EnginePathAgent agent(EnginePathAgent::choosing(free_unit, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "once the free unit is chosen nothing is left to offer";
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_FALSE(agent.wasOffered(rooted)) << "can't move to base";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_TRUE(o(free_unit).isAtBase());
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(rooted), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(guarded).moves_this_turn, 0);
    EXPECT_EQ(o(rooted).moves_this_turn, 0);
}

// 67 Blitzcrank, Impassive: "you may move an enemy unit to here".
TEST_F(VerifyCardsTest, Blitzcrank_DoesNotOfferAnEnemyUnitItCannotMove) {
    auto blitz = addUnit(P1, kBlitzcrank, 0, 0);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 1);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets

    // Only the unmovable unit: the "may" is not offered at all.
    EnginePathAgent none(EnginePathAgent::accepting());
    fireTriggerThroughEngine(blitz, TriggerType::WhenYouPlayMe, &none);
    EXPECT_TRUE(none.prompts.empty()) << "no enemy unit he could move";
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});

    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(free_unit, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(blitz, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_TRUE(agent.wasOffered(free_unit));
    EXPECT_EQ(bfOf(free_unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// 674 Irresistible Faefolk: "you may move an enemy unit to that battlefield".
TEST_F(VerifyCardsTest, Faefolk_DoesNotOfferAnEnemyUnitItCannotMove) {
    auto fae = addUnit(P1, kFaefolk, 0, 0);
    auto guarded = addUnit(P2, kShipyardSkulker, 0, 1);
    auto cutlass = addGear(P2, kJaggedCutlass);
    attach(cutlass, guarded);
    o(guarded).cant_be_moved_by_enemy = true;            // what the Cutlass's aura sets

    EnginePathAgent none(EnginePathAgent::accepting());
    fireTriggerThroughEngine(fae, TriggerType::WhenIMoveToFB, &none);
    EXPECT_TRUE(none.prompts.empty()) << "no enemy unit it could move";
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});

    auto free_unit = addUnit(P2, kShipyardSkulker, 0, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(free_unit, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(fae, TriggerType::WhenIMoveToFB, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, then the unit";
    EXPECT_FALSE(agent.wasOffered(guarded)) << "can't be moved by enemy spells and abilities";
    EXPECT_EQ(bfOf(free_unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(guarded), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// ─── 9.M28: Safety Inspector 726 — the 3 XP is an additional cost of the play ─

TEST_F(VerifyCardsTest, SafetyInspector_TheXpIsSpentBeforeItEntersTheBoard) {
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kSafetyInspector);
    setXp(P1, 3);
    const auto& def = card_db.get(kSafetyInspector);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    const int funded = ps(P1).rune_pool.energy;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = card;
    play.play_location = LocationId{BaseLocation{P1}};

    GameState* live = nullptr;
    int asked = 0;
    bool on_board_when_asked = true;
    int energy_when_asked = -1;
    PickByPredicateAgent me([&](int, const std::vector<Intent>& legal) -> Intent {
        if (legal.empty()) return Intent{};
        const bool is_choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice) {
            for (const auto& i : legal) {
                if (i.choice_label.rfind("Yes", 0) != 0) continue;
                if (asked++ == 0 && live) {
                    on_board_when_asked = live->getObject(card).location.has_value();
                    energy_when_asked = live->player(P1).rune_pool.energy;
                }
                return i;
            }
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.front();
    });
    int xp_seen_by_opponent = -1;
    PickByPredicateAgent them([&](int, const std::vector<Intent>& legal) -> Intent {
        if (xp_seen_by_opponent < 0 && live) xp_seen_by_opponent = live->player(P1).xp;
        if (legal.empty()) return Intent{};
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(play);
        e.testHook_runChain();
        e.testHook_cleanup();
        e.testHook_runChain();
        live = nullptr;
    }, &me, &them);

    EXPECT_EQ(asked, 1) << "one yes/no, as the card is played";
    EXPECT_FALSE(on_board_when_asked)
        << "an additional cost is chosen before the unit enters the board (CR 356.2)";
    EXPECT_EQ(energy_when_asked, funded) << "and before its cost is paid";
    EXPECT_EQ(xp_seen_by_opponent, 0) << "spent before anyone can respond to the play trigger";
    EXPECT_EQ(ps(P1).xp, 0) << "paid once";
    EXPECT_TRUE(onBoard(mine)) << "\"you don't kill a unit this way\"";
    EXPECT_TRUE(onBoard(card));
    EXPECT_FALSE(onBoard(theirs));
}

TEST_F(VerifyCardsTest, SafetyInspector_DecliningTheXpKillsYourUnitToo) {
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kSafetyInspector);
    setXp(P1, 3);
    // No to the XP; then which of my units dies (the Inspector is one of them).
    EnginePathAgent me(EnginePathAgent::script({0, static_cast<int>(mine.raw())}));
    playThroughEngine(card, &me);
    EXPECT_EQ(ps(P1).xp, 3);
    EXPECT_FALSE(onBoard(mine));
    EXPECT_FALSE(onBoard(theirs));
    EXPECT_TRUE(onBoard(card));
}

// The "paid" flag is consumed by the play trigger: a later play of the same
// card with no XP does not inherit it.
TEST_F(VerifyCardsTest, SafetyInspector_APaidPlayDoesNotSpareTheNextPlay) {
    auto mine = addUnit(P1, kInvalidId, 2);
    addUnit(P2, kInvalidId, 5, 0);
    auto card = addToHand(P1, kSafetyInspector);
    setXp(P1, 3);
    EnginePathAgent pays(EnginePathAgent::script({1}));
    playThroughEngine(card, &pays);
    ASSERT_EQ(ps(P1).xp, 0);
    ASSERT_TRUE(onBoard(mine));
    ASSERT_TRUE(onBoard(card));

    // Returned to hand and played again, with no XP left.
    o(card).location = std::nullopt;
    o(card).zone = ZoneType::Hand;
    ps(P1).hand.push_back(card);
    addUnit(P2, kInvalidId, 5, 0);
    EnginePathAgent again(EnginePathAgent::choosing(mine));
    playThroughEngine(card, &again);
    EXPECT_FALSE(onBoard(mine)) << "no additional cost was paid for this play";
    EXPECT_TRUE(onBoard(card));
}

// ─── 9.M31: Fox-Fire 253 / Overt Operation 153 — no 12-pick cap ─────────────

TEST_F(VerifyCardsTest, FoxFire_MoreThanTwelveZeroMightUnitsCanAllBeChosen) {
    std::vector<GameObjectId> units;
    for (int i = 0; i < 14; ++i) {
        auto u = addUnit(P2, kInvalidId, 1, 0);
        o(u).base_might = 0;
        o(u).current_might = 0;
        units.push_back(u);
    }
    // Always another unit, never "Stop here".
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    playThroughEngine(addToHand(P1, kFoxFire), &agent);
    EXPECT_EQ(agent.prompts.size(), 14u)
        << "the unit chosen as it is played, then one prompt per further unit";
    for (auto u : units) EXPECT_FALSE(onBoard(u)) << "unit " << u;
}

TEST_F(VerifyCardsTest, OvertOperation_MoreThanTwelveBuffedUnitsAreEachAsked) {
    std::vector<GameObjectId> units;
    for (int i = 0; i < 14; ++i) {
        auto u = addUnit(P1, kInvalidId, 3);
        o(u).buff_count = 1;
        o(u).recomputeMight();
        o(u).is_exhausted = true;
        units.push_back(u);
    }
    // "Spend its buff to ready it" every time.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    playThroughEngine(addToHand(P1, kOvertOperation), &agent);
    EXPECT_EQ(agent.prompts.size(), 14u) << "one question per buffed friendly unit";
    for (auto u : units) {
        EXPECT_FALSE(o(u).is_exhausted) << "unit " << u;
        EXPECT_EQ(o(u).buff_count, 1) << "\"Then buff all friendly units.\"";
    }
}

// ─── 9.M33: Bellows Breath 402 — "at the same location" at resolution ───────
// CR 355.11.b / 355.11.b.1 (the example is this card): when the chosen units no
// longer share a location, the controller chooses which legal subset is
// affected — all of the chosen units at one location.

TEST_F(VerifyCardsTest, BellowsBreath_FirstTargetMoved_TheControllerPicksWhichGroupIsHit) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBellowsBreath);
    ps(P1).rune_pool.energy += card_db.get(kBellowsBreath).energy_cost;
    ps(P1).rune_pool.universal_power += card_db.get(kBellowsBreath).power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    // The three units are chosen as the spell is played (its own prompts
    // since D45, not targets of the intent). Then, at resolution: group 0 is
    // the first-chosen unit's (now alone in base); group 1 the two that stayed.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(a.raw()), static_cast<int>(b.raw()), static_cast<int>(c.raw()), 1}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        live.getObject(a).location = BaseLocation{P2};
        live.getObject(a).zone = ZoneType::Base;
    });
    ASSERT_EQ(agent.prompts.size(), 4u) << "three units, then which group of units at one location";
    EXPECT_EQ(agent.prompts[3].size(), 2u);
    EXPECT_EQ(o(a).damage_marked, 0);
    EXPECT_EQ(o(b).damage_marked, 1) << "the units that stayed together are a legal subset";
    EXPECT_EQ(o(c).damage_marked, 1);
}

TEST_F(VerifyCardsTest, BellowsBreath_UnitsThatMovedTogetherAreStillAGroup) {
    // The CR 355.11.b.1 example: two of the three move to base; either the
    // one left at the battlefield or the two in base — never one of the two.
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBellowsBreath);
    ps(P1).rune_pool.energy += card_db.get(kBellowsBreath).energy_cost;
    ps(P1).rune_pool.universal_power += card_db.get(kBellowsBreath).power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(a.raw()), static_cast<int>(b.raw()), static_cast<int>(c.raw()), 1}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        for (auto id : {b, c}) {
            live.getObject(id).location = BaseLocation{P2};
            live.getObject(id).zone = ZoneType::Base;
        }
    });
    ASSERT_EQ(agent.prompts.size(), 4u);
    EXPECT_EQ(agent.prompts[3].size(), 2u) << "{a} or {b, c}: no option splits the two in base";
    EXPECT_EQ(o(a).damage_marked, 0);
    EXPECT_EQ(o(b).damage_marked, 1);
    EXPECT_EQ(o(c).damage_marked, 1);
}

TEST_F(VerifyCardsTest, BellowsBreath_FirstTargetGone_TheOthersStillTakeOne) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBellowsBreath);
    ps(P1).rune_pool.energy += card_db.get(kBellowsBreath).energy_cost;
    ps(P1).rune_pool.universal_power += card_db.get(kBellowsBreath).power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(a.raw()), static_cast<int>(b.raw()), static_cast<int>(c.raw())}));
    executeAndProbe(play, &agent, [&](GameState& live) { removeFromBoard(live, a); });
    EXPECT_EQ(agent.prompts.size(), 3u) << "the remaining targets share a location: no choice at resolution";
    EXPECT_EQ(o(a).damage_marked, 0) << "an illegal target is unaffected (CR 359.3.e.5)";
    EXPECT_EQ(o(b).damage_marked, 1);
    EXPECT_EQ(o(c).damage_marked, 1);
}

// ─── 9.M34: Angle Shot 334 attaches through EffectExecutor::attachGear ──────

TEST_F(VerifyCardsTest, AngleShot_TheAttachedEquipmentsTextGoesInactive) {
    auto old_bearer = addUnit(P1, kShipyardSkulker);
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    auto sword = addGear(P1, kLongSword);
    attach(sword, old_bearer);
    addToDeck(P1, kShipyardSkulker);                     // "Draw 1."
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(unit.raw()), static_cast<int>(sword.raw())}));
    playThroughEngine(addToHand(P1, kAngleShot), &agent);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{unit});
    EXPECT_TRUE(o(sword).is_rules_text_inactive) << "CR 718.2";
    EXPECT_EQ(o(sword).location, o(unit).location) << "CR 719.3";
    EXPECT_TRUE(o(old_bearer).attachments.empty()) << "it came off its prior bearer";
    ASSERT_EQ(o(unit).attachments.size(), 1u);
    EXPECT_EQ(o(unit).attachments.front(), sword);
    EXPECT_EQ(handSize(P1), 1) << "Draw 1";
}

TEST_F(VerifyCardsTest, AngleShot_AttachesNothingToAUnitThatLeftInResponse) {
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    auto sword = addGear(P1, kLongSword);
    addToDeck(P1, kShipyardSkulker);                     // "Draw 1."
    auto shot = addToHand(P1, kAngleShot);
    ps(P1).rune_pool.energy += card_db.get(kAngleShot).energy_cost;
    ps(P1).rune_pool.universal_power += card_db.get(kAngleShot).power_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = shot;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(unit.raw()), static_cast<int>(sword.raw())}));
    executeAndProbe(play, &agent, [&](GameState& live) { removeFromBoard(live, unit); });
    EXPECT_TRUE(agent.wasOffered(unit));
    EXPECT_TRUE(agent.wasOffered(sword));
    EXPECT_FALSE(o(sword).attached_to.has_value()) << "the unit is not on the board (CR 716)";
    EXPECT_TRUE(o(unit).attachments.empty());
    EXPECT_FALSE(o(sword).is_rules_text_inactive);
    EXPECT_TRUE(o(sword).isAtBase()) << "the Equipment stays where it was";
    EXPECT_EQ(handSize(P1), 1) << "\"Draw 1.\" still happens";
}

// ─── 9.M37: Voidreaver 787 — the second ability is a move, not a recall ─────

TEST_F(VerifyCardsTest, Voidreaver_SecondAbilityMovesTheUnitToBase) {
    auto legend = addLegend(P1, kVoidreaver);
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).is_exhausted = true;
    setXp(P1, 2);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    activate(legend, &agent, 1);
    EXPECT_TRUE(o(unit).isAtBase());
    EXPECT_EQ(o(unit).moves_this_turn, 1) << "a move counts as one; a recall would not (CR 458)";
    EXPECT_EQ(ps(P1).xp, 0) << "Spend 2 XP";
    EXPECT_TRUE(o(legend).is_exhausted);
}

TEST_F(VerifyCardsTest, Voidreaver_SecondAbilityDoesNotMoveAUnitThatCannotMoveToBase) {
    auto legend = addLegend(P1, kVoidreaver);
    auto unit = addUnit(P1, kShipyardSkulker, 0, 0);
    o(unit).is_exhausted = true;
    o(unit).cant_move_to_base = true;                    // as Determined Sentry sets it
    setXp(P1, 2);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    activate(legend, &agent, 1);
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "a move is stopped by \"can't move to base\"; a recall would not be (CR 458.3)";
    EXPECT_EQ(o(unit).moves_this_turn, 0);
}

// ─── 9.M38: Defender of Tomorrow 936 — which printed ability, while Empowered ─

TEST_F(VerifyCardsTest, DefenderOfTomorrow_EmpoweredMayStillReadyJustOneGear) {
    auto legend = addLegend(P1, kDefenderOfTomorrow);
    o(legend).empowered = 1;
    auto mine = addGear(P1, kInvalidId);
    auto theirs = addGear(P2, kInvalidId);
    o(mine).is_exhausted = true;
    o(theirs).is_exhausted = true;
    ps(P1).rune_pool.energy = 1;
    // "[1], [E]: Ready a gear." (mode 0), then the gear.
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(mine.raw())}));
    activate(legend, &agent);
    ASSERT_GE(agent.prompts.size(), 2u) << "which ability, then which gear";
    EXPECT_EQ(agent.prompts[0].size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "Ready a gear";
    EXPECT_TRUE(promptOffersValue(agent, 0, 1)) << "Ready 2 gear";
    EXPECT_FALSE(o(mine).is_exhausted);
    EXPECT_TRUE(o(theirs).is_exhausted) << "the first printed ability readies one gear only";
    EXPECT_TRUE(o(legend).is_exhausted);

    // Still one activation plus the Empower ability, in the same slots.
    const auto abilities = card_registry.get(kDefenderOfTomorrow)->activatedAbilities();
    ASSERT_EQ(abilities.size(), 2u);
    EXPECT_FALSE(abilities[0].is_empower);
    EXPECT_TRUE(abilities[1].is_empower);
}

TEST_F(VerifyCardsTest, DefenderOfTomorrow_EmpoweredReadyTwoGearReadiesTwo) {
    auto legend = addLegend(P1, kDefenderOfTomorrow);
    o(legend).empowered = 1;
    auto g1 = addGear(P1, kInvalidId);
    auto g2 = addGear(P1, kInvalidId);
    auto g3 = addGear(P1, kInvalidId);
    for (auto g : {g1, g2, g3}) o(g).is_exhausted = true;
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(g3.raw()), static_cast<int>(g1.raw())}));
    activate(legend, &agent);
    EXPECT_EQ(agent.prompts.size(), 3u) << "which ability, then the two gear";
    EXPECT_FALSE(o(g3).is_exhausted);
    EXPECT_FALSE(o(g1).is_exhausted);
    EXPECT_TRUE(o(g2).is_exhausted);
}

TEST_F(VerifyCardsTest, DefenderOfTomorrow_NotEmpoweredIsNotAskedWhichAbility) {
    auto legend = addLegend(P1, kDefenderOfTomorrow);
    auto g1 = addGear(P1, kInvalidId);
    auto g2 = addGear(P1, kInvalidId);
    o(g1).is_exhausted = true;
    o(g2).is_exhausted = true;
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(g2));
    activate(legend, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "only the gear: the Empowered ability is not available";
    EXPECT_TRUE(agent.wasOffered(g1));
    EXPECT_FALSE(o(g2).is_exhausted);
    EXPECT_TRUE(o(g1).is_exhausted);
}


// ═══ Tranche 5, group T5B ══════════════════════════════════════════════════════
// ═══ 9.M30: Void Hatchling 341 — cards that reveal from a deck by hand ══════
// "If you would reveal cards from a deck, look at the top card first. You may
//  recycle it. Then reveal those cards." Each test recycles the top card at the
//  look, so the card revealed is the one below it.

TEST_F(VerifyCardsTest, RavenbloomConservatory_VoidHatchlingLooksBeforeTheReveal) {
    withVoidHatchling();
    auto conservatory = battlefieldCard(0, kRavenbloomConservatory);
    auto spell = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    EnginePathAgent agent(EnginePathAgent::script({1}));   // recycle the top card
    fireTriggerThroughEngine(conservatory, TriggerType::WhenYouDefendHere, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the look";
    EXPECT_TRUE(inHand(P1, spell)) << "the spell below was revealed: into the hand";
    EXPECT_TRUE(inDeck(P1, top)) << "recycled at the look";
}

TEST_F(VerifyCardsTest, PakaaProtector_VoidHatchlingLooksBeforeTheReveal) {
    withVoidHatchling();
    auto pakaa = addUnit(P1, kPakaaProtector, 0, 0);
    auto unit_card = addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(pakaa, TriggerType::WhenIMove, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the look";
    EXPECT_TRUE(inHand(P1, unit_card)) << "the unit below was revealed: drawn";
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_FALSE(inTrash(P1, top));
    EXPECT_EQ(o(pakaa).temp_might_bonus, 0) << "a unit was revealed: no +2";
}

TEST_F(VerifyCardsTest, DianaLunari_VoidHatchlingLooksAfterThePredictBeforeTheReveal) {
    withVoidHatchling();
    auto diana = addUnit(P1, kDianaLunari, 0, 0);
    auto spell = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = 1;
    // Prompt 0: pay [1]? yes. Prompt 1: the Predict — keep the card. Prompt 2:
    // the Hatchling's look — recycle it.
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const int at = n++;
        for (const auto& i : legal) {
            if (at == 1) {
                // Corrected (sweep-costs, 9.GG1): "Keep it" is named by value 0.
                if (i.chosen_objects.empty()) return i;
            } else if (i.chosen_value.has_value() && *i.chosen_value == 1) {
                return i;
            }
        }
        return legal.front();
    });
    fireTriggerThroughEngine(diana, TriggerType::WhenAShowdownBeginsHere, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the payment, the Predict, the look";
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
    EXPECT_TRUE(inHand(P1, spell)) << "the spell below was revealed: drawn";
    EXPECT_TRUE(inDeck(P1, top));
}

TEST_F(VerifyCardsTest, TeemoStrategist_VoidHatchlingLooksOnceBeforeTheFiveAreRevealed) {
    withVoidHatchling();
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto hidden = addToDeck(P1, kTeemoStrategist);         // sixth from the top, [Hidden]
    for (int i = 0; i < 4; ++i) addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kMeditation);
    // The enemy unit at the target prompt; "recycle" (1) at the look, which
    // is an unlabelled 0/1 prompt and so is answered here explicitly.
    EnginePathAgent agent([enemy](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == enemy) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        ADD_FAILURE() << "unexpected prompt";
        return legal.front();
    });
    fireTriggerThroughEngine(teemo, TriggerType::WhenIDefend, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "the unit, then one look";
    EXPECT_TRUE(askedHatchlingLook(agent));
    EXPECT_EQ(o(enemy).damage_marked, 1)
        << "the top card was recycled, so the sixth card ([Hidden]) is among the five";
    EXPECT_EQ(deckSize(P1), 6);
    EXPECT_TRUE(inDeck(P1, hidden));
    EXPECT_TRUE(inDeck(P1, top));
}

TEST_F(VerifyCardsTest, VoidRush_VoidHatchlingLooksBeforeTheReveal) {
    withVoidHatchling();
    auto third = addToDeck(P1, kMeditation);
    auto second = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    auto spell = addToHand(P1, kVoidRush);
    EnginePathAgent agent(EnginePathAgent::script({1, 0}));   // recycle the top card; banish none
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(askedHatchlingLook(agent));
    EXPECT_TRUE(inDeck(P1, top)) << "recycled at the look: not one of the two revealed";
    EXPECT_TRUE(inHand(P1, second)) << "Draw any you didn't banish";
    EXPECT_TRUE(inHand(P1, third));
}

TEST_F(VerifyCardsTest, VoidBurrower_VoidHatchlingLooksBeforeTheReveal) {
    withVoidHatchling();
    auto legend = addLegend(P1, kVoidBurrower);
    auto third = addToDeck(P1, kMeditation);
    auto second = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    std::vector<GameObjectId> shown;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.revealed_to_all) shown.push_back(e.card);
    });
    EnginePathAgent agent(EnginePathAgent::script({
        1,                         // "may exhaust me to reveal 2": yes
        1,                         // Void Hatchling's look at the top card: 1 = recycle it (0 = keep)
        0,                         // "You may banish one": banish none (value 0; the cards are objects)
    }));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    conn.disconnect();
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the look, the banish";
    EXPECT_TRUE(askedHatchlingLook(agent));
    EXPECT_EQ(shown, (std::vector<GameObjectId>{second, third}))
        << "the two cards below the recycled one";
    EXPECT_EQ(deckSize(P1), 3);
    EXPECT_TRUE(inDeck(P1, top));
}

// Blind Fury 25: "Each opponent reveals the top card of their Main Deck." The
// player who reveals is the opponent, from the opponent's deck.
TEST_F(VerifyCardsTest, BlindFury_TheOpponentsVoidHatchlingLooksBeforeTheirReveal) {
    withVoidHatchling(P2);
    auto below = addToDeck(P2, kShipyardSkulker);
    auto top = addToDeck(P2, kShipyardSkulker);
    EnginePathAgent caster;
    EnginePathAgent revealer(EnginePathAgent::script({1}));   // recycle the top card
    playThroughEngine(addToHand(P1, kBlindFury), &caster, std::nullopt, &revealer);
    ASSERT_EQ(revealer.prompts.size(), 1u) << "the opponent would reveal: the opponent looks";
    EXPECT_TRUE(caster.prompts.empty());
    EXPECT_TRUE(inDeck(P2, top)) << "recycled at the look";
    ASSERT_TRUE(onBoard(below)) << "the card below was revealed, banished and played";
    EXPECT_EQ(o(below).controller, P1);
}

TEST_F(VerifyCardsTest, Pin_BlindFury_TheCastersVoidHatchlingDoesNotLook) {
    // "If YOU would reveal cards from a deck": the caster reveals nothing.
    withVoidHatchling(P1);
    auto below = addToDeck(P2, kShipyardSkulker);
    auto top = addToDeck(P2, kShipyardSkulker);
    EnginePathAgent caster(EnginePathAgent::script({1}));
    EnginePathAgent revealer(EnginePathAgent::script({1}));
    playThroughEngine(addToHand(P1, kBlindFury), &caster, std::nullopt, &revealer);
    EXPECT_TRUE(caster.prompts.empty());
    EXPECT_TRUE(revealer.prompts.empty());
    ASSERT_TRUE(onBoard(top)) << "the top card, as it was";
    EXPECT_EQ(o(top).controller, P1);
    EXPECT_TRUE(inDeck(P2, below));
}

// "Look at the top N ... You may reveal a <card> from among them and draw it.
//  Recycle the rest." (381, 594, 613, 626, 741): no Hatchling look — the card
// was picked from cards already looked at, and recycling the top one first
// changes no zone.
TEST_F(VerifyCardsTest, Pin_OrnnBlacksmith_RevealingFromAmongLookedAtCardsHasNoHatchlingLook) {
    withVoidHatchling();
    auto ornn = addUnit(P1, kOrnnBlacksmith, 0);
    addToDeck(P1, kMeditation);
    auto gear = addToDeck(P1, kSerratedDirk);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(gear, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(ornn, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_FALSE(askedHatchlingLook(agent));
    EXPECT_TRUE(inHand(P1, gear));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(VerifyCardsTest, Pin_DoubleTrouble_RevealingFromAmongLookedAtCardsHasNoHatchlingLook) {
    withVoidHatchling();
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(unit, EnginePathAgent::May::Yes));
    playThroughEngine(addToHand(P1, kDoubleTrouble), &agent);
    EXPECT_FALSE(askedHatchlingLook(agent));
    EXPECT_TRUE(inHand(P1, unit));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(VerifyCardsTest, Pin_IvernNurturer_RevealingFromAmongLookedAtCardsHasNoHatchlingLook) {
    withVoidHatchling();
    auto ivern = addUnit(P1, kIvernNurturer, 0);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(unit));
    fireTriggerThroughEngine(ivern, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_FALSE(askedHatchlingLook(agent));
    EXPECT_TRUE(inHand(P1, unit));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(VerifyCardsTest, Pin_FateWeaver_RevealingFromAmongLookedAtCardsHasNoHatchlingLook) {
    withVoidHatchling();
    auto weaver = addUnit(P1, kFateWeaver, 0);
    addToDeck(P1, kMeditation);
    auto big_spell = addToDeck(P1, kPromisingFuture);        // a spell costing [5]
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(big_spell, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(weaver, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_FALSE(askedHatchlingLook(agent));
    EXPECT_TRUE(inHand(P1, big_spell));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(deckSize(P1), 2);
}

TEST_F(VerifyCardsTest, Pin_RiftHerald_RevealingFromAmongLookedAtCardsHasNoHatchlingLook) {
    withVoidHatchling();
    auto herald = addUnit(P1, kRiftHerald, 0, 0);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);
    auto top = addToDeck(P1, kMeditation);
    EnginePathAgent agent(EnginePathAgent::choosing(unit, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(herald, TriggerType::WhenIMoveToFB, &agent);
    EXPECT_FALSE(askedHatchlingLook(agent));
    EXPECT_TRUE(inHand(P1, unit));
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_EQ(deckSize(P1), 2);
}

// ═══ 9.M35: a unit played by an effect with no printed location (CR 355.2.a) ═
// Its controller chooses their base or a battlefield they control that units
// may be played to. Answers: 0 = base, 1 + i = battlefield slot i.

TEST_F(VerifyCardsTest, BlindFury_TheRevealedUnitIsPlayedWhereTheCasterChooses) {
    holdBattlefield(1);
    auto top = addToDeck(P2, kShipyardSkulker);
    int shown = 0;
    auto conn = events.on_card_revealed.connect([&](const CardRevealedEvent& e) {
        if (e.card == top) ++shown;
    });
    EnginePathAgent agent(EnginePathAgent::script({2}));
    playThroughEngine(addToHand(P1, kBlindFury), &agent);
    conn.disconnect();
    ASSERT_EQ(agent.prompts.size(), 1u) << "where";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 0, 1)) << "battlefield #0 is not the caster's";
    EXPECT_TRUE(promptOffersValue(agent, 0, 2));
    ASSERT_TRUE(onBoard(top));
    EXPECT_EQ(o(top).controller, P1);
    EXPECT_EQ(bfOf(top), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(shown, 1) << "revealed once, not again when re-entered with the answer";
    EXPECT_FALSE(inBanishment(P2, top));
}

TEST_F(VerifyCardsTest, Pin_BlindFury_ARevealedGearGoesToBaseUnasked) {
    holdBattlefield(1);
    auto top = addToDeck(P2, kSerratedDirk);
    EnginePathAgent agent(EnginePathAgent::script({2}));
    playThroughEngine(addToHand(P1, kBlindFury), &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "gear is played to base";
    ASSERT_TRUE(onBoard(top));
    EXPECT_EQ(o(top).controller, P1);
    EXPECT_TRUE(o(top).isAtBase());
}

TEST_F(VerifyCardsTest, Reinforce_TheUnitIsPlayedWhereItsControllerChooses) {
    holdBattlefield(0);
    state.battlefields[0].blocks_unit_play = true;    // Rockfall Path
    holdBattlefield(1);
    addToDeck(P1, kMeditation);
    auto unit = addToDeck(P1, kShipyardSkulker);       // [3]: free after the [5] reduction
    auto spell = addToHand(P1, kReinforce);
    EnginePathAgent agent(pickingThen(unit, {2}));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 1, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 1, 2));
    ASSERT_TRUE(onBoard(unit));
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_FALSE(inBanishment(P1, unit));
    EXPECT_EQ(deckSize(P1), 1) << "the rest was recycled";
}

TEST_F(VerifyCardsTest, PromisingFuture_EachPlayerChoosesWhereTheirUnitIsPlayed) {
    holdBattlefield(0, P1);
    holdBattlefield(1, P2);
    auto mine = addToDeck(P1, kShipyardSkulker);
    auto theirs = addToDeck(P2, kShipyardSkulker);
    auto spell = addToHand(P1, kPromisingFuture);
    EnginePathAgent me(pickingThen(mine, {1}));
    EnginePathAgent them(pickingThen(theirs, {2}));
    playThroughEngine(spell, &me, std::nullopt, &them);
    ASSERT_EQ(me.prompts.size(), 2u) << "the banish, where";
    ASSERT_EQ(them.prompts.size(), 2u) << "the banish, where";
    EXPECT_TRUE(promptOffersValue(me, 1, 0));
    EXPECT_TRUE(promptOffersValue(me, 1, 1));
    EXPECT_FALSE(promptOffersValue(me, 1, 2)) << "the opponent's battlefield";
    EXPECT_TRUE(promptOffersValue(them, 1, 0));
    EXPECT_FALSE(promptOffersValue(them, 1, 1)) << "the caster's battlefield";
    EXPECT_TRUE(promptOffersValue(them, 1, 2));
    ASSERT_TRUE(onBoard(mine));
    ASSERT_TRUE(onBoard(theirs));
    EXPECT_EQ(bfOf(mine), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(theirs), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(o(theirs).controller, P2);
}

TEST_F(VerifyCardsTest, TheHarrowing_TheUnitIsPlayedWhereItsControllerChooses) {
    holdBattlefield(0);
    auto dead = addToTrash(P1, kShipyardSkulker);
    EnginePathAgent agent(pickingThen(dead, {1}));
    playThroughEngine(addToHand(P1, kTheHarrowing), &agent);
    ASSERT_FALSE(agent.prompts.empty());
    const size_t last = agent.prompts.size() - 1;
    EXPECT_TRUE(promptOffersValue(agent, last, 0)) << "the last prompt is where: base";
    EXPECT_TRUE(promptOffersValue(agent, last, 1));
    ASSERT_TRUE(onBoard(dead));
    EXPECT_EQ(bfOf(dead), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_FALSE(inTrash(P1, dead));
}

TEST_F(VerifyCardsTest, BaitedHook_TheUnitIsPlayedWhereItsControllerChooses) {
    auto hook = addGear(P1, kBaitedHook);
    ps(P1).rune_pool.energy = 1;
    power(P1, Domain::Order) = 1;
    holdBattlefield(0);
    auto victim = addUnit(P1, kInvalidId, 1);
    addToDeck(P1, kInvalidId);
    auto eligible = addToDeck(P1, kRecruit);
    o(eligible).base_might = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = hook;
    act.targets = {victim};
    EnginePathAgent agent(pickingThen(eligible, {1}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        e.testHook_runChain();
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0));
    EXPECT_TRUE(promptOffersValue(agent, 1, 1));
    EXPECT_FALSE(onBoard(victim));
    ASSERT_TRUE(onBoard(eligible));
    EXPECT_EQ(bfOf(eligible), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_FALSE(inBanishment(P1, eligible));
    EXPECT_EQ(deckSize(P1), 1) << "the rest was recycled";
}

TEST_F(VerifyCardsTest, ZeroDrive_EachUnitIsPlayedWhereItsControllerChooses) {
    auto drive = addGear(P1, kZeroDrive);
    holdBattlefield(0);
    state.battlefields[0].blocks_unit_play = true;    // Rockfall Path
    holdBattlefield(1);
    auto first = addToTrash(P1, kShipyardSkulker);
    auto second = addToTrash(P1, kShipyardSkulker);
    banishWith(drive, first);
    banishWith(drive, second);
    ps(P1).rune_pool.energy = 3;
    power(P1, Domain::Mind) = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = drive;
    EnginePathAgent agent(EnginePathAgent::script({2, 0}));   // battlefield #1, then base
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        e.testHook_runChain();
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "one location per unit";
    for (size_t n = 0; n < 2; ++n) {
        EXPECT_TRUE(promptOffersValue(agent, n, 0)) << "base";
        EXPECT_FALSE(promptOffersValue(agent, n, 1)) << "units can't be played to battlefield #0";
        EXPECT_TRUE(promptOffersValue(agent, n, 2));
    }
    ASSERT_TRUE(onBoard(first));
    ASSERT_TRUE(onBoard(second));
    EXPECT_EQ(bfOf(first), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_TRUE(o(second).isAtBase());
    EXPECT_TRUE(ps(P1).banishment == std::vector<GameObjectId>{drive})
        << "\"Banish this\" was paid once; both units left banishment";
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
    EXPECT_EQ(power(P1, Domain::Mind), 0);
}

TEST_F(VerifyCardsTest, LastRites_TheUnitIsPlayedWhereItsControllerChooses) {
    auto bearer = addUnit(P1, kInvalidId, 3, 0);
    auto rites = addGear(P1, kLastRites);
    attach(rites, bearer);
    state.battlefields[0].controller = P1;            // held by the bearer
    holdBattlefield(1);
    auto in_trash = addToTrash(P1, kShipyardSkulker); // costs [3]
    ps(P1).rune_pool.energy = 3;
    EnginePathAgent agent(pickingThen(in_trash, {1, 2}));   // yes; battlefield #1
    withEngine([&](GameEngine& e) {
        e.testHook_scoreHold(P1, e.mutableState().battlefields[0].id);
        e.testHook_runChain();
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "where the bearer held";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2)) << "the other battlefield it controls";
    ASSERT_TRUE(onBoard(in_trash));
    EXPECT_EQ(bfOf(in_trash), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "its [3] was paid";
}

TEST_F(VerifyCardsTest, VoidBurrower_TheBanishedUnitIsPlayedWhereItsControllerChooses) {
    auto legend = addLegend(P1, kVoidBurrower);
    holdBattlefield(0);
    state.battlefields[0].blocks_unit_play = true;    // Rockfall Path
    holdBattlefield(1);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    ps(P1).rune_pool.energy = card_db.get(kShipyardSkulker).energy_cost;
    EnginePathAgent agent(pickingThen(top, {1, 2}));  // yes; the unit; battlefield #1
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the may, the banish, where";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "base";
    EXPECT_FALSE(promptOffersValue(agent, 2, 1)) << "units can't be played to battlefield #0";
    EXPECT_TRUE(promptOffersValue(agent, 2, 2));
    ASSERT_TRUE(onBoard(top));
    EXPECT_EQ(bfOf(top), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_FALSE(inBanishment(P1, top));
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "its cost was paid";
    EXPECT_TRUE(inDeck(P1, below)) << "recycle the rest";
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 1);
}

TEST_F(VerifyCardsTest, Pin_VoidBurrower_AUnitThatCannotBePaidForIsNotAskedWhere) {
    auto legend = addLegend(P1, kVoidBurrower);
    holdBattlefield(1);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);       // [3], nothing to pay it with
    EnginePathAgent agent(pickingThen(top, {1, 2}));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "the may, the banish; no location for a unit not played";
    EXPECT_FALSE(onBoard(top));
    EXPECT_TRUE(inBanishment(P1, top)) << "banished all the same (CR 356.1.b.2: the cost is not waived)";
    EXPECT_TRUE(inDeck(P1, below));
}

TEST_F(VerifyCardsTest, CursedSarcophagus_TheUnitIsPlayedWhereItsControllerChooses) {
    auto coffin = addGear(P1, kCursedSarcophagus);
    holdBattlefield(0);
    auto unit = addToTrash(P1, kShipyardSkulker);     // costs [3]
    banishWith(coffin, unit);
    ps(P1).rune_pool.energy = 3;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = coffin;
    EnginePathAgent agent(pickingThen(unit, {1}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(act);
        e.testHook_runChain();
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the unit, where";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0));
    EXPECT_TRUE(promptOffersValue(agent, 1, 1));
    ASSERT_TRUE(onBoard(unit));
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "its [3] was paid";
    EXPECT_TRUE(o(coffin).tracked_objects.empty());
    EXPECT_TRUE(ps(P1).banishment.empty());
}

// ═══ Tranche 5: 9.O1 Mystic Vortex 947 ═══════════════════════════════════════
// ── 9.O1 Mystic Vortex 947 ──────────────────────────────────────────────────
// "During showdowns here, cards with [Reaction] cost [A] more to play. (Hidden
// cards have [Reaction].)"
// The card publishes the standing amount (BattlefieldState::reaction_tax_power);
// the engine reader (GameEngine::extraAnyPowerCost) decides live whether a
// showdown is on at that battlefield.

// A [Reaction] play HERE during a showdown HERE, from the first decision of the
// showdown: the cost gate, the generator and the payment.
TEST_F(VerifyCardsTest, MysticVortex_TaxesAReactionPlayedHereFromTheFirstDecisionOfAShowdownHere) {
    auto inferna = vortexScenario();
    auto open = [&](GameState& st) { openShowdownAt(st, 0); };
    EXPECT_FALSE(affordableAfter(inferna, 2, 0, open))
        << "[2] alone is one [A] short while a showdown is on at the Vortex";
    EXPECT_FALSE(ambushOffer(inferna, 0).has_value()) << "not offered unpaid";

    EXPECT_TRUE(affordableAfter(inferna, 2, 1, open));
    auto here = ambushOffer(inferna, 0);
    ASSERT_TRUE(here.has_value()) << "[Ambush] to the showdown's battlefield is offered";
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(*here);
        e.testHook_runChain();
    });
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "her [2] and the Vortex's [A]";
    ASSERT_TRUE(bfOf(inferna).has_value());
    EXPECT_EQ(*bfOf(inferna), BattlefieldId::fromRaw(0));
}

// "here" is where the SHOWDOWN is, not where the taxed card goes: the same
// card played to the other battlefield during that showdown costs [A] more too.
TEST_F(VerifyCardsTest, MysticVortex_TaxesAReactionPlayedToAnotherBattlefieldDuringAShowdownHere) {
    auto inferna = vortexScenario();
    auto open = [&](GameState& st) { openShowdownAt(st, 0); };
    EXPECT_FALSE(affordableAfter(inferna, 2, 0, open));
    EXPECT_FALSE(ambushOffer(inferna, 1).has_value())
        << "a play to battlefield 1 is a card with [Reaction] played during a showdown at "
           "the Vortex: taxed like any other";

    EXPECT_TRUE(affordableAfter(inferna, 2, 1, open));
    auto elsewhere = ambushOffer(inferna, 1);
    ASSERT_TRUE(elsewhere.has_value()) << "P1 has a unit at battlefield 1 (CR 822.1.b)";
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(*elsewhere);
        e.testHook_runChain();
    });
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "the [A] was charged for a play elsewhere";
    ASSERT_TRUE(bfOf(inferna).has_value());
    EXPECT_EQ(*bfOf(inferna), BattlefieldId::fromRaw(1));
}

// A showdown at ANOTHER battlefield is not a showdown "here".
TEST_F(VerifyCardsTest, Pin_MysticVortex_AShowdownAtAnotherBattlefieldIsNotTaxed) {
    auto inferna = vortexScenario();
    addUnit(P2, kInvalidId, 1, 1);
    auto open = [&](GameState& st) { openShowdownAt(st, 1); };
    EXPECT_TRUE(affordableAfter(inferna, 2, 0, open)) << "her printed [2]";
    EXPECT_TRUE(ambushOffer(inferna, 1).has_value());
    EXPECT_TRUE(ambushOffer(inferna, 0).has_value())
        << "nor is a play to the Vortex taxed: no showdown is on there";
}

// A combat here opens with a Combat Showdown (CR 316.8.a, 466.2): it is a
// showdown here. runCombat sets combat_in_progress only; combat_phase is None
// while the attack / defend triggers resolve (CR 466.2.d, part of Step 1) and
// ShowdownStep in the focus loop.
TEST_F(VerifyCardsTest, MysticVortex_TaxesDuringTheCombatShowdownStepOfACombatHere) {
    auto inferna = vortexScenario();
    for (CombatPhase phase : {CombatPhase::None, CombatPhase::ShowdownStep}) {
        auto open = [&](GameState& st) { openCombatAt(st, 0, phase); };
        EXPECT_FALSE(affordableAfter(inferna, 2, 0, open))
            << "combat_phase " << static_cast<int>(phase);
        EXPECT_TRUE(affordableAfter(inferna, 2, 1, open))
            << "combat_phase " << static_cast<int>(phase);
    }
    EXPECT_FALSE(affordableAfter(inferna, 2, 0, [&](GameState& st) {
        openCombatAt(st, 0, CombatPhase::ShowdownStep); }));
    EXPECT_FALSE(ambushOffer(inferna, 0).has_value());
}

// DISABLED until sweep-verify-engine changes the reader. CR 467.2: Combat
// Damage is resolved "when the Showdown closes", and the Resolution Step
// (CR 468) follows it — the combat is still in progress, the showdown is not.
// A [Reaction] played in the Resolution Step's chain (CR 468.2: in response to
// a death or recall trigger) is not played "during a showdown here".
// Needed: GameEngine::extraAnyPowerCost must count a battlefield's
// reaction_tax_power only while
//     bf.showdown_in_progress ||
//     (bf.combat_in_progress && bf.combat_phase != CombatPhase::DamageStep &&
//                               bf.combat_phase != CombatPhase::ResolutionStep)
// instead of `bf.showdown_in_progress || bf.combat_in_progress`.
TEST_F(VerifyCardsTest, MysticVortex_NoTaxOnceTheCombatShowdownHasClosed) {
    auto inferna = vortexScenario();
    for (CombatPhase phase : {CombatPhase::DamageStep, CombatPhase::ResolutionStep}) {
        auto open = [&](GameState& st) {
            openCombatAt(st, 0, phase);
            st.turn.oc_state = OpenClosedState::Closed;   // the step's chain (CR 468.2)
        };
        EXPECT_TRUE(affordableAfter(inferna, 2, 0, open))
            << "her printed [2]: the Combat Showdown has closed (combat_phase "
            << static_cast<int>(phase) << ")";
    }
}

// Outside any showdown the standing amount is published and nothing is owed.
TEST_F(VerifyCardsTest, Pin_MysticVortex_NoTaxOutsideAShowdown) {
    auto inferna = vortexScenario();
    EXPECT_TRUE(affordableAfter(inferna, 2, 0, [](GameState&) {})) << "Neutral Open";
    EXPECT_EQ(state.battlefields[0].reaction_tax_power, 1)
        << "the amount for showdowns here stands; the reader applies it only during one";
    EXPECT_EQ(state.battlefields[1].reaction_tax_power, 0);

    // A showdown here that has ended (runShowdown clears the flag with no aura
    // pass after it): nothing is owed either, in an Open or a Closed State.
    auto ended = [&](GameState& st) {
        openShowdownAt(st, 0);
        st.battlefields[0].showdown_in_progress = false;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Closed;
    };
    EXPECT_TRUE(affordableAfter(inferna, 2, 0, ended));
}

// ═══ Tranche 6: 9.R1 Fox-Fire 253 (CR 355.11) ═══════════════════════════════
// ─── 9.R1: Fox-Fire 253 — the group requirement is re-judged as it resolves ──
// "Kill any number of units at a battlefield with total Might 4 or less."
// CR 355.11.a: the whole group is chosen as the spell is finalized.
// CR 355.11.b: if the group no longer fulfils the requirement as the spell
// resolves, its controller chooses a legal subset of the ORIGINAL targets;
// 355.11.b.1: a subset holds as many of them as it can.

// The rule's own example: four 1 [M] units chosen, two of them get +1 [M] in
// response (1, 1, 2, 2 = 6). The controller chooses the two 2 [M] units.
TEST_F(VerifyCardsTest, FoxFire_TotalNoLongerFits_TheControllerChoosesASubsetOfTheChosenUnits) {
    auto r1 = addUnit(P2, kInvalidId, 1, 0);
    auto r2 = addUnit(P2, kInvalidId, 1, 0);
    auto r3 = addUnit(P2, kInvalidId, 1, 0);
    auto r4 = addUnit(P2, kInvalidId, 1, 0);
    auto fox = addToHand(P1, kFoxFire);
    ps(P1).rune_pool.energy += card_db.get(kFoxFire).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = fox;
    // Four picks as it is played, then the new choice: r3, r4.
    EnginePathAgent agent(pickingInOrder({r1, r2, r3, r4, r3, r4}));
    // Added in response too, so it was never one of the chosen units.
    GameObjectId late = kInvalidId;
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 4u)
            << "all four units are chosen before the opponent can respond (CR 355.11.a)";
        for (auto id : {r3, r4}) {
            auto& u = live.getObject(id);
            u.temp_might_bonus += 1;
            u.recomputeMight();
        }
        late = live.createObject();
        auto& x = live.getObject(late);
        x.owner = x.controller = P2;
        x.card_def_id = kInvalidId;
        x.card_type = CardType::Unit;
        x.name = "LateUnit";
        x.base_might = x.current_might = 1;
        x.zone = ZoneType::BattlefieldZone;
        x.location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
    });
    ASSERT_EQ(agent.prompts.size(), 6u) << "4 choices as it is played, 2 as it resolves";
    const std::vector<GameObjectId> originals = {r1, r2, r3, r4};
    EXPECT_EQ(promptObjects(agent, 4), originals)
        << "the new choice is among the original targets only";
    EXPECT_FALSE(promptOffersValue(agent, 4, 0)) << "no 'stop': a subset is as large as it can be";
    const std::vector<GameObjectId> after_r3 = {r1, r2, r4};
    EXPECT_EQ(promptObjects(agent, 5), after_r3);
    EXPECT_FALSE(onBoard(r3));
    EXPECT_FALSE(onBoard(r4));
    EXPECT_TRUE(onBoard(r1)) << "2 + 2 used the whole total";
    EXPECT_TRUE(onBoard(r2));
    ASSERT_NE(late, kInvalidId);
    EXPECT_TRUE(onBoard(late)) << "a unit that was not chosen as the spell was played is never affected";
}

// The other legal subset of the example: the two 1 [M] units plus one 2 [M].
// After 1 + 1 the two 2 [M] units both still fit, so the controller is asked
// which; after that nothing fits and the fourth unit lives.
TEST_F(VerifyCardsTest, FoxFire_TotalNoLongerFits_SubsetHoldsAsManyUnitsAsFit) {
    auto r1 = addUnit(P2, kInvalidId, 1, 0);
    auto r2 = addUnit(P2, kInvalidId, 1, 0);
    auto r3 = addUnit(P2, kInvalidId, 1, 0);
    auto r4 = addUnit(P2, kInvalidId, 1, 0);
    auto fox = addToHand(P1, kFoxFire);
    ps(P1).rune_pool.energy += card_db.get(kFoxFire).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = fox;
    EnginePathAgent agent(pickingInOrder({r1, r2, r3, r4, r1, r2, r4}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        for (auto id : {r3, r4}) {
            auto& u = live.getObject(id);
            u.temp_might_bonus += 1;
            u.recomputeMight();
        }
    });
    ASSERT_EQ(agent.prompts.size(), 7u);
    EXPECT_FALSE(promptOffersValue(agent, 6, 0))
        << "1 + 1 is not a legal subset while a 2 [M] target still fits (CR 355.11.b.1)";
    EXPECT_FALSE(onBoard(r1));
    EXPECT_FALSE(onBoard(r2));
    EXPECT_FALSE(onBoard(r4));
    EXPECT_TRUE(onBoard(r3)) << "1 + 1 + 2 = 4: the other 2 [M] unit no longer fits";
}

// Fails on the old code: the only chosen unit outgrew the total, so the engine
// dropped it, the card started over and killed a unit that was never chosen.
TEST_F(VerifyCardsTest, FoxFire_TheOnlyChosenUnitOutgrowsTheTotal_NothingElseIsKilled) {
    auto big = addUnit(P2, kInvalidId, 4, 0);
    auto small = addUnit(P2, kInvalidId, 1, 0);
    auto fox = addToHand(P1, kFoxFire);
    ps(P1).rune_pool.energy += card_db.get(kFoxFire).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = fox;
    EnginePathAgent agent(pickingInOrder({big}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        auto& u = live.getObject(big);
        u.temp_might_bonus += 1;
        u.recomputeMight();
    });
    EXPECT_EQ(agent.prompts.size(), 1u) << "no legal subset: there is nothing to choose";
    EXPECT_TRUE(onBoard(big)) << "5 [M] is not 'total Might 4 or less'";
    EXPECT_TRUE(onBoard(small)) << "it was not one of the original targets (CR 355.11.b)";
}

// "At a battlefield" is part of the group requirement: chosen units that are no
// longer together are a subset each (the last sentences of the example).
TEST_F(VerifyCardsTest, FoxFire_ChosenUnitsNoLongerAtOneBattlefield_OneGroupIsChosen) {
    auto stays = addUnit(P2, kInvalidId, 2, 0);
    auto moves = addUnit(P2, kInvalidId, 2, 0);
    auto fox = addToHand(P1, kFoxFire);
    ps(P1).rune_pool.energy += card_db.get(kFoxFire).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = fox;
    EnginePathAgent agent(pickingInOrder({stays, moves, moves}));
    executeAndProbe(play, &agent, [&](GameState& live) {
        live.getObject(moves).location = BattlefieldLocation{BattlefieldId::fromRaw(1)};
    });
    ASSERT_EQ(agent.prompts.size(), 3u);
    const std::vector<GameObjectId> both = {stays, moves};
    EXPECT_EQ(promptObjects(agent, 2), both);
    EXPECT_FALSE(onBoard(moves)) << "a chosen unit that left the battlefield can still be affected";
    EXPECT_TRUE(onBoard(stays)) << "the two are no longer at one battlefield";
}

// Nothing changed in response: exactly the chosen units die, all of them
// chosen before the opponent could respond, and nothing is asked again.
TEST_F(VerifyCardsTest, FoxFire_UnchangedBoard_KillsTheChosenUnitsWithNoFurtherPrompt) {
    auto a = addUnit(P2, kInvalidId, 1, 0);
    auto b = addUnit(P2, kInvalidId, 2, 0);
    auto spared = addUnit(P2, kInvalidId, 1, 0);
    auto fox = addToHand(P1, kFoxFire);
    ps(P1).rune_pool.energy += card_db.get(kFoxFire).energy_cost;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = fox;
    EnginePathAgent agent(pickingInOrder({a, b, kInvalidId}));   // a, b, "Stop here"
    executeAndProbe(play, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 3u) << "the choices are made as the spell is played";
        EXPECT_TRUE(live.getObject(a).location.has_value()) << "not resolved yet";
        EXPECT_TRUE(live.getObject(b).location.has_value());
    });
    EXPECT_EQ(agent.prompts.size(), 3u) << "the group still fits: no new choice";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "'Stop here' while a unit still fits";
    EXPECT_FALSE(onBoard(a));
    EXPECT_FALSE(onBoard(b));
    EXPECT_TRUE(onBoard(spared));
}

// ═══ Tranche 7: 9.T3 zero targets (CR 355.13) ════════════════════════════════
// ═══ 9.T3: "any number of" / "up to N" objects may be zero (CR 355.13) ══════
// "If a card specifies that a player chooses 'any number' or 'up to' some
// number of Game Objects to be affected, they may choose any number of
// available targets, including zero. If they choose zero, the spell or ability
// can be played without any targets."

// ─── Fox-Fire 253 (fixed) ───────────────────────────────────────────────────
// Fails on the old code: hasLegalTargets refused the play with no unit at a
// battlefield.
TEST_F(VerifyCardsTest, FoxFire_PlayableFromHandWithNoUnitAtABattlefield) {
    auto in_base = addUnit(P2, kInvalidId, 1);
    auto fox = addToHand(P1, kFoxFire);
    ASSERT_EQ(playsOffered(fox).size(), 1u) << "zero units is a legal choice (CR 355.13)";
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(fox, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to choose from";
    EXPECT_TRUE(inTrash(P1, fox)) << "it was played and resolved";
    EXPECT_TRUE(onBoard(in_base));
}

// Fails on the old code: the first prompt had no "Stop here", so the script's
// 0 matched nothing and the first unit was taken.
TEST_F(VerifyCardsTest, FoxFire_FirstPromptOffersChoosingNoUnit) {
    auto a = addUnit(P2, kInvalidId, 2, 0);
    auto b = addUnit(P2, kInvalidId, 1, 0);
    auto fox = addToHand(P1, kFoxFire);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "Stop here"
    playThroughEngine(fox, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "'Stop here' on the first prompt";
    ASSERT_FALSE(agent.prompts[0].empty());
    EXPECT_TRUE(agent.prompts[0].front().chosen_value.has_value()) << "listed first";
    EXPECT_TRUE(onBoard(a));
    EXPECT_TRUE(onBoard(b));
    EXPECT_TRUE(inTrash(P1, fox));
}

// ─── Flash 309 (fixed) ──────────────────────────────────────────────────────
// Fails on the old code: "No more units" was offered only after the first unit.
TEST_F(VerifyCardsTest, Flash_FirstPromptOffersMovingNoUnit) {
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto flash = addToHand(P1, kFlash);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No more units"
    playThroughEngine(flash, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0));
    ASSERT_FALSE(agent.prompts[0].empty());
    EXPECT_TRUE(agent.prompts[0].front().chosen_value.has_value()) << "listed first";
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)}) << "up to 2: none was chosen";
    EXPECT_TRUE(inTrash(P1, flash));
}

TEST_F(VerifyCardsTest, Pin_Flash_PlayableWithNoFriendlyUnitAtABattlefield) {
    auto in_base = addUnit(P1, kInvalidId, 3);
    auto flash = addToHand(P1, kFlash);
    ASSERT_EQ(playsOffered(flash).size(), 1u);
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(flash, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, flash));
    EXPECT_TRUE(o(in_base).isAtBase());
}

// ─── Singularity 105 (fixed) ────────────────────────────────────────────────
// "Deal 6 to each of up to two units." Fails on the old code: with a unit on
// the board every offered play had at least one target.
TEST_F(VerifyCardsTest, Singularity_MayBePlayedChoosingNoUnitWhileUnitsAreAvailable) {
    auto e1 = addUnit(P2, kInvalidId, 7, 0);
    auto e2 = addUnit(P2, kInvalidId, 7);
    auto spell = addToHand(P1, kSingularity);
    const auto plays = playsOffered(spell);
    EXPECT_TRUE(offersTargets(plays, {})) << "zero of 'up to two' (CR 355.13)";
    EXPECT_TRUE(offersTargets(plays, {e1}));
    EXPECT_TRUE(offersTargets(plays, {e2}));
    EXPECT_TRUE(offersTargets(plays, {e1, e2}));
    EXPECT_EQ(plays.size(), 4u);

    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(spell, &agent);   // the play with no targets
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_EQ(o(e1).damage_marked, 0);
    EXPECT_EQ(o(e2).damage_marked, 0);
}

TEST_F(VerifyCardsTest, Pin_Singularity_PlayableWithNoUnitOnTheBoard) {
    auto spell = addToHand(P1, kSingularity);
    const auto plays = playsOffered(spell);
    ASSERT_EQ(plays.size(), 1u);
    EXPECT_TRUE(plays[0].targets.empty());
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(inTrash(P1, spell));
}

// ─── Bellows Breath 402 (fixed) ─────────────────────────────────────────────
// "Deal 1 to up to three units at the same location."
// (D45, testing phase: the units are prompts of the spell now, not target
// sets of the play intent. Old expectation: four plays offered — none, a, b,
// a+b. New: one play, whose first prompt offers "No more units", a and b.)
TEST_F(VerifyCardsTest, BellowsBreath_MayBePlayedChoosingNoUnitWhileUnitsAreAvailable) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBellowsBreath);
    const auto plays = playsOffered(spell);
    EXPECT_EQ(plays.size(), 1u);

    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No more units"
    playThroughEngine(spell, &agent);   // zero of 'up to three' (CR 355.13)
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 3u) << "none, a, b";
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_EQ(o(a).damage_marked, 0);
    EXPECT_EQ(o(b).damage_marked, 0);
}

// ─── Emperor's Divide 366 (fixed) ───────────────────────────────────────────
// "Move any number of friendly units at a battlefield to their base." Fails on
// the old code: the mandatory play-time target refused the play from hand with
// no friendly unit at a battlefield.
TEST_F(VerifyCardsTest, EmperorsDivide_PlayableFromHandWithNoFriendlyUnitAtABattlefield) {
    auto in_base = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kEmperorsDivide);
    const auto plays = playsOffered(spell);
    ASSERT_EQ(plays.size(), 1u) << "zero units is a legal choice (CR 355.13)";
    EXPECT_TRUE(plays[0].targets.empty());
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_TRUE(o(in_base).isAtBase());
    EXPECT_EQ(bfOf(enemy), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// With units to choose, "done" is on the first prompt: none has to move.
TEST_F(VerifyCardsTest, Pin_EmperorsDivide_FirstPromptOffersMovingNoUnit) {
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kEmperorsDivide);
    // One play: the units are chosen as it is played (testing phase G17; the
    // play intent used to name a unit standing for the battlefield).
    ASSERT_EQ(playsOffered(spell).size(), 1u);
    EXPECT_TRUE(playsOffered(spell).front().targets.empty());
    // "done": the option with no object.
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_objects.empty()) return i;
        ADD_FAILURE() << "no 'done' option";
        return legal.front();
    });
    playAt(spell, {}, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(bfOf(u1), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(u2), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// ─── Disposal Order 665 (fixed) ─────────────────────────────────────────────
// "Choose one — Choose up to 3 cards from opponents' trashes. Their owners
// recycle them. / Draw 1." Fails on the old code: the first mode was masked out
// while the opposing trash was empty, so only "Draw 1" was offered.
TEST_F(VerifyCardsTest, DisposalOrder_RecycleModeCanBeChosenWithAnEmptyOpposingTrash) {
    ASSERT_TRUE(ps(P2).trash.empty());
    addToDeck(P1, kMeditation);
    auto spell = addToHand(P1, kDisposalOrder);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // mode 0: recycle up to 3
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the mode; then there is no card to ask about";
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "up to 3 includes none (CR 355.13)";
    EXPECT_TRUE(promptOffersValue(agent, 0, 1));
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_EQ(handSize(P1), 0) << "the other mode's 'Draw 1' did not happen";
    EXPECT_EQ(deckSize(P1), 1);
}

TEST_F(VerifyCardsTest, Pin_DisposalOrder_DoneOnTheFirstCardPromptRecyclesNothing) {
    auto theirs = addToTrash(P2, kShipyardSkulker);
    auto spell = addToHand(P1, kDisposalOrder);
    EnginePathAgent agent(EnginePathAgent::script({0, 0}));   // mode 0, then "Done"
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "'Done' on the first card prompt";
    EXPECT_TRUE(inTrash(P2, theirs));
}

// ─── Not a defect: playable with nothing to choose, and "none" is on offer ──

// 224 Salvage: "You may kill up to one gear. Draw 1."
TEST_F(VerifyCardsTest, Pin_Salvage_NoGearChosen_StillDraws) {
    addToDeck(P1, kMeditation);
    addToDeck(P1, kMeditation);
    auto first = addToHand(P1, kSalvage);
    auto plays = playsOffered(first);
    ASSERT_EQ(plays.size(), 1u) << "no gear on the board";
    EXPECT_TRUE(plays[0].targets.empty());
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(first, &agent);
    EXPECT_TRUE(inTrash(P1, first));
    EXPECT_EQ(handSize(P1), 1) << "Draw 1 does not depend on the gear";

    // With a gear on the board, killing none is still a play.
    auto gear = addGear(P2, kSerratedDirk);
    auto second = addToHand(P1, kSalvage);
    plays = playsOffered(second);
    EXPECT_TRUE(offersTargets(plays, {}));
    EXPECT_TRUE(offersTargets(plays, {gear}));
    EnginePathAgent agent2(EnginePathAgent::script({}));
    playThroughEngine(second, &agent2);
    EXPECT_TRUE(onBoard(gear));
    EXPECT_EQ(handSize(P1), 2);
}

// 346 Piercing Light: "Deal 2 to a unit at a battlefield, then deal 2 to up to
// one other unit." The first unit is mandatory; the other may be left out.
TEST_F(VerifyCardsTest, Pin_PiercingLight_NeedsTheFirstUnit_TheOtherIsOptional) {
    auto spell = addToHand(P1, kPiercingLight);
    auto in_base = addUnit(P2, kInvalidId, 5);
    EXPECT_TRUE(playsOffered(spell).empty()) << "'a unit at a battlefield' is not 'up to'";
    auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    const auto plays = playsOffered(spell);
    EXPECT_TRUE(offersTargets(plays, {at_bf})) << "no other unit chosen";
    EXPECT_TRUE(offersTargets(plays, {at_bf, in_base}));
    EXPECT_FALSE(offersTargets(plays, {}));
    EnginePathAgent agent(EnginePathAgent::script({}));
    playAt(spell, {at_bf}, &agent);
    EXPECT_EQ(o(at_bf).damage_marked, 2);
    EXPECT_EQ(o(in_base).damage_marked, 0);
}

// 513 Arise!: "Then do this: Ready up to two of them."
TEST_F(VerifyCardsTest, Pin_Arise_ReadyingNoneOfTheSandSoldiers) {
    auto spell = addToHand(P1, kArise);
    for (int i = 0; i < 2; ++i) ASSERT_TRUE(isEquipment(o(addGear(P1, kSerratedDirk))));
    // "None" on the first ready prompt (the reflexive trigger's "up to
    // two", asked as it is finalized); the first location otherwise.
    int stops = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.choice_label.rfind("None — Arise!", 0) == 0) { ++stops; return i; }
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_EQ(stops, 1) << "asked once, stopped at once";
    int soldiers = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Sand Soldier" || !obj.location.has_value()) continue;
        ++soldiers;
        EXPECT_TRUE(obj.is_exhausted) << "none was readied";
    }
    EXPECT_EQ(soldiers, 2) << "the tokens are played all the same";
}

// 894 Decree of Discord: "Return any number of enemy Order units with total
// Might 5 or less to their owners' hands."
TEST_F(VerifyCardsTest, Pin_DecreeOfDiscord_PlayableWithNoneAndMayChooseNone) {
    auto first = addToHand(P1, kDecreeOfDiscord);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "no enemy Order unit";
    EnginePathAgent none(EnginePathAgent::script({}));
    playThroughEngine(first, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P1, first));

    auto order_unit = addUnit(P2, kInvalidId, 2, 0);
    o(order_unit).domains = {Domain::Order};
    auto second = addToHand(P1, kDecreeOfDiscord);
    EnginePathAgent done(EnginePathAgent::script({0}));   // "Done"
    playThroughEngine(second, &done);
    ASSERT_EQ(done.prompts.size(), 1u);
    EXPECT_TRUE(done.wasOffered(order_unit));
    EXPECT_TRUE(promptOffersValue(done, 0, 0));
    EXPECT_TRUE(onBoard(order_unit));
}

// 260 Guerilla Warfare: "Return up to two cards with [Hidden] from your trash
// to your hand. You can hide cards ignoring costs this turn."
TEST_F(VerifyCardsTest, Pin_GuerillaWarfare_NoCardReturned_StillHidesIgnoringCosts) {
    auto first = addToHand(P1, kGuerillaWarfare);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "no [Hidden] card in the trash";
    EnginePathAgent none(EnginePathAgent::script({}));
    playThroughEngine(first, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(ps(P1).hide_ignoring_costs_this_turn) << "the second sentence stands alone";

    ps(P1).hide_ignoring_costs_this_turn = false;
    auto hidden_card = addToTrash(P1, kFoxFire);
    auto second = addToHand(P1, kGuerillaWarfare);
    EnginePathAgent stop(EnginePathAgent::script({0}));   // "Return no more"
    playThroughEngine(second, &stop);
    ASSERT_EQ(stop.prompts.size(), 1u);
    EXPECT_TRUE(stop.wasOffered(hidden_card));
    EXPECT_TRUE(inTrash(P1, hidden_card));
    EXPECT_TRUE(ps(P1).hide_ignoring_costs_this_turn);
}

// 755 Moonfall: "Choose a battlefield where you have units. You may move up to
// one enemy unit to that battlefield. Then give enemy units there -2 [M] this
// turn." The battlefield is mandatory; the enemy unit is not.
TEST_F(VerifyCardsTest, Pin_Moonfall_NoEnemyUnitMoved_StillGivesMinusTwo) {
    addUnit(P1, kInvalidId, 3, 0);
    auto there = addUnit(P2, kInvalidId, 6, 0);
    auto first = addToHand(P1, kMoonfall);
    ASSERT_EQ(playsOffered(first).size(), 1u);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // battlefield slot 0
    playThroughEngine(first, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "no enemy unit elsewhere: the move is not asked";
    EXPECT_EQ(o(there).current_might, 4) << "the last sentence does not depend on a move";

    // An enemy unit elsewhere may be left where it is.
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    auto second = addToHand(P1, kMoonfall);
    EnginePathAgent decline(EnginePathAgent::script({0, 0}));   // slot 0; no move
    playThroughEngine(second, &decline);
    EXPECT_EQ(decline.prompts.size(), 2u);
    EXPECT_EQ(bfOf(elsewhere), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_EQ(o(elsewhere).current_might, 3);
}

// 616 Tricksy Tentacles: "Move any number of enemy units with the same
// controller and a total Might of 8 or less to a single location."
TEST_F(VerifyCardsTest, Pin_TricksyTentacles_PlayableWithNoneAndMayChooseNone) {
    auto first = addToHand(P1, kTricksyTentacles);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "no enemy unit";
    EnginePathAgent none(EnginePathAgent::script({}));
    playThroughEngine(first, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P1, first));

    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto second = addToHand(P1, kTricksyTentacles);
    EnginePathAgent done(EnginePathAgent::script({0}));   // "Done"
    playThroughEngine(second, &done);
    EXPECT_EQ(done.prompts.size(), 1u) << "nothing chosen: no location is asked";
    EXPECT_TRUE(done.wasOffered(enemy));
    EXPECT_EQ(bfOf(enemy), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// 890 Shadows of the Past: "Return up to 2 units from trashes to their owners'
// hands."
TEST_F(VerifyCardsTest, Pin_ShadowsOfThePast_PlayableWithNoneAndMayChooseNone) {
    auto first = addToHand(P1, kShadowsOfThePast);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "no unit in a trash";
    EnginePathAgent none(EnginePathAgent::script({}));
    playThroughEngine(first, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P1, first));

    auto dead = addToTrash(P2, kShipyardSkulker);
    auto second = addToHand(P1, kShadowsOfThePast);
    EnginePathAgent done(EnginePathAgent::script({0}));   // "Done"
    playThroughEngine(second, &done);
    ASSERT_EQ(done.prompts.size(), 1u);
    EXPECT_TRUE(done.wasOffered(dead));
    EXPECT_TRUE(inTrash(P2, dead));
}

// 503 Relentless Pursuit: "Move a friendly unit. You may attach up to one
// Equipment with the same controller to it. This turn, that unit has ..."
// The unit is mandatory; the Equipment is not.
TEST_F(VerifyCardsTest, Pin_RelentlessPursuit_NoEquipment_StillMovesAndGrants) {
    auto spell = addToHand(P1, kRelentlessPursuit);
    EXPECT_TRUE(playsOffered(spell).empty()) << "'a friendly unit' is not 'up to'";
    auto unit = addUnit(P1, kInvalidId, 3);
    const auto plays = playsOffered(spell);
    ASSERT_EQ(plays.size(), 1u);
    // One play; the unit is chosen in its finalize pass as it is played
    // (testing phase D3 — it was a target of the play intent).
    EXPECT_TRUE(plays.front().targets.empty());
    const size_t delayed_before = state.delayed_abilities.size();
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(unit.raw()), 1}));   // the unit; destination 1 = battlefield #0
    playAt(spell, {}, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "no Equipment: the Equipment is not asked";
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(state.delayed_abilities.size(), delayed_before + 1)
        << "\"When I conquer, you may move me to my base.\"";
}

// 927 Shuriken Flip: "Deal 2 to up to one enemy unit at a battlefield, then
// move a friendly unit."
TEST_F(VerifyCardsTest, Pin_ShurikenFlip_NoEnemyUnitChosen_StillMovesAFriendlyUnit) {
    state.battlefields[0].controller = P1;
    auto mine = addUnit(P1, kInvalidId, 3);
    auto first = addToHand(P1, kShurikenFlip);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "no enemy unit at a battlefield";
    // The friendly unit, then destination 1 = battlefield #0.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mine.raw()), 1}));
    playThroughEngine(first, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u) << "the damage is not asked";
    EXPECT_EQ(bfOf(mine), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});

    // With an enemy unit at a battlefield, "No damage" is on that prompt.
    auto enemy = addUnit(P2, kInvalidId, 5, 1);
    auto other = addUnit(P1, kInvalidId, 3);
    auto second = addToHand(P1, kShurikenFlip);
    // "No damage"; the friendly unit; destination 1 = battlefield #0.
    EnginePathAgent decline(EnginePathAgent::script({0, static_cast<int>(other.raw()), 1}));
    playThroughEngine(second, &decline);
    ASSERT_EQ(decline.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(decline, 0, 0));
    EXPECT_EQ(o(enemy).damage_marked, 0);
    EXPECT_EQ(bfOf(other), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// 937 Acceleration Gate: "Ready up to 4 units, gear, and/or runes."
TEST_F(VerifyCardsTest, Pin_AccelerationGate_PlayableWithNoneAndMayChooseNone) {
    auto first = addToHand(P1, kAccelerationGate);
    ASSERT_EQ(playsOffered(first).size(), 1u) << "nothing is exhausted";
    EnginePathAgent none(EnginePathAgent::script({}));
    playThroughEngine(first, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P1, first));

    auto tired = addUnit(P1, kInvalidId, 3);
    o(tired).is_exhausted = true;
    auto second = addToHand(P1, kAccelerationGate);
    EnginePathAgent done(EnginePathAgent::script({0}));   // "Done"
    playThroughEngine(second, &done);
    ASSERT_EQ(done.prompts.size(), 1u);
    EXPECT_TRUE(done.wasOffered(tired));
    EXPECT_TRUE(o(tired).is_exhausted);
}


// ═══ Tranche 8: 9.W1 zero objects for non-spell cards (CR 355.13), group T8A ═══
// ═══ 9.W1: "any number of" / "up to N" on units (CR 355.13), part A ═════════

// ─── Sona, Harmonious 73 (fixed) ────────────────────────────────────────────
// "At the end of your turn, if I'm at a battlefield, ready up to 4 friendly
// runes." Fails on the old code: no prompt at all — four exhausted runes were
// readied in map order.
TEST_F(VerifyCardsTest, Sona_TheControllerPicksWhichRunesAndMayStopBeforeFour) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, true);
    auto r3 = addRune(P1, Domain::Fury, true);
    auto r4 = addRune(P1, Domain::Calm, true);
    auto r5 = addRune(P1, Domain::Fury, true);
    auto theirs = addRune(P2, Domain::Calm, true);
    EnginePathAgent agent(pickingInOrder({r5, r3, kInvalidId}));   // two, then "None"
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_FALSE(agent.wasOffered(theirs)) << "friendly runes";
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{r1, r2, r3, r4}))
        << "a rune already named is not offered again";
    EXPECT_FALSE(o(r5).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
    EXPECT_TRUE(o(r1).is_exhausted) << "up to 4: the controller stopped at two";
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_TRUE(o(r4).is_exhausted);
    EXPECT_TRUE(o(theirs).is_exhausted);
}

// Fails on the old code: both runes were readied without asking.
TEST_F(VerifyCardsTest, Sona_FirstPromptOffersReadyingNoRune) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, true);
    auto r2 = addRune(P1, Domain::Calm, true);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "None"
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "zero of 'up to 4' (CR 355.13)";
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
}

TEST_F(VerifyCardsTest, Pin_Sona_AtMostFourRunes) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, true);
    auto r2 = addRune(P1, Domain::Calm, true);
    auto r3 = addRune(P1, Domain::Calm, true);
    auto r4 = addRune(P1, Domain::Calm, true);
    auto r5 = addRune(P1, Domain::Calm, true);
    EnginePathAgent agent(pickingInOrder({r1, r2, r3, r4}));
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    EXPECT_EQ(agent.prompts.size(), 4u) << "no fifth prompt";
    for (auto r : {r1, r2, r3, r4}) EXPECT_FALSE(o(r).is_exhausted);
    EXPECT_TRUE(o(r5).is_exhausted);
}

TEST_F(VerifyCardsTest, Pin_Sona_NoExhaustedRune_NothingAsked) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto ready = addRune(P1, Domain::Calm);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(ready).is_exhausted);
}

// ─── Hwei, Brooding Painter 642 (fixed) ─────────────────────────────────────
// "Gear — Ready up to 2 runes." Fails on the old code: only the discard was
// asked, and two exhausted runes were readied in map order.
TEST_F(VerifyCardsTest, Hwei_DiscardingAGear_TheControllerPicksWhichRunesAreReadied) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);                        // the card drawn
    auto gear = addToHand(P1, kSealOfStrength163);
    auto r1 = addRune(P1, Domain::Mind, true);
    auto r2 = addRune(P1, Domain::Mind, true);
    auto r3 = addRune(P1, Domain::Mind, true);
    EnginePathAgent agent(pickingInOrder({gear, r3, r2}));   // the discard, then two runes
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "the discard and two runes; no third rune";
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "'None' on the first rune prompt";
    EXPECT_EQ(promptObjects(agent, 2), (std::vector<GameObjectId>{r1, r2}));
    EXPECT_TRUE(o(r1).is_exhausted) << "the controller, not map order, picks the runes";
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

// Fails on the old code: both runes were readied without asking.
TEST_F(VerifyCardsTest, Hwei_DiscardingAGear_MayReadyNoRune) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);
    auto gear = addToHand(P1, kSealOfStrength163);
    auto r1 = addRune(P1, Domain::Mind, true);
    auto r2 = addRune(P1, Domain::Mind, true);
    EnginePathAgent agent(pickingInOrder({gear, kInvalidId}));   // the discard, then "None"
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_TRUE(o(r1).is_exhausted) << "zero of 'up to 2' (CR 355.13)";
    EXPECT_TRUE(o(r2).is_exhausted);
}

TEST_F(VerifyCardsTest, Pin_Hwei_DiscardingAGearWithNoExhaustedRune_AsksOnlyTheDiscard) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);
    auto gear = addToHand(P1, kSealOfStrength163);
    auto ready = addRune(P1, Domain::Mind);
    EnginePathAgent agent(pickingInOrder({gear}));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_FALSE(o(ready).is_exhausted);
}

// ─── Kinkou Monk 141 (fixed) ────────────────────────────────────────────────
// "When you play me, buff up to two other friendly units." Fails on the old
// code: the prompt had no "Done", so the script's 0 matched nothing and both
// units were buffed.
TEST_F(VerifyCardsTest, KinkouMonk_FirstPromptOffersBuffingNoUnit) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto monk = addToHand(P1, kKinkouMonk);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "Done"
    playThroughEngine(monk, &agent);
    ASSERT_TRUE(onBoard(monk));
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "zero of 'up to two' (CR 355.13)";
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{u1, u2}));
    EXPECT_EQ(o(u1).buff_count, 0);
    EXPECT_EQ(o(u2).buff_count, 0);
}

// Fails on the old code: the second prompt had no "Done" either (the wanted
// option is not offered, and the other unit is buffed).
TEST_F(VerifyCardsTest, KinkouMonk_MayBuffOnlyOneUnit) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto monk = addToHand(P1, kKinkouMonk);
    EnginePathAgent agent(pickingInOrder({u2, kInvalidId}));   // u2, then "Done"
    playThroughEngine(monk, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{u1}))
        << "the unit already chosen is not offered again";
    EXPECT_EQ(o(u2).buff_count, 1);
    EXPECT_EQ(o(u1).buff_count, 0) << "up to two: one was chosen";
    EXPECT_EQ(o(monk).buff_count, 0) << "other friendly units";
}

TEST_F(VerifyCardsTest, Pin_KinkouMonk_BuffsTheTwoUnitsChosenAndAsksNoThird) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto u3 = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto monk = addToHand(P1, kKinkouMonk);
    EnginePathAgent agent(pickingInOrder({u3, u1}));
    playThroughEngine(monk, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_FALSE(agent.wasOffered(enemy));
    EXPECT_FALSE(agent.wasOffered(monk));
    EXPECT_EQ(o(u3).buff_count, 1);
    EXPECT_EQ(o(u1).buff_count, 1);
    EXPECT_EQ(o(u2).buff_count, 0);
}

TEST_F(VerifyCardsTest, Pin_KinkouMonk_NoOtherFriendlyUnit_NothingAsked) {
    auto enemy = addUnit(P2, kInvalidId, 2);
    auto monk = addToHand(P1, kKinkouMonk);
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(monk, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(monk));
    EXPECT_EQ(o(monk).buff_count, 0);
    EXPECT_EQ(o(enemy).buff_count, 0);
}

// ─── Volibear, Furious 41 ────────────────────────────────────────────────────
// "When I attack, deal 5 damage split among any number of enemy units here."
// splitDamageAmong(..., may_choose_none) offers "No more targets" on the first
// prompt too (plans/09 item 9.Z1).
TEST_F(VerifyCardsTest, VolibearFurious_FirstPromptOffersDamagingNoUnit) {
    auto voli = addUnit(P1, kVolibearFurious, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 6, 0);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No more targets"
    fireTriggerThroughEngine(voli, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "zero of 'any number' (CR 355.13)";
    EXPECT_EQ(o(enemy).damage_marked, 0);
}

TEST_F(VerifyCardsTest, Pin_VolibearFurious_NoEnemyUnitHere_NothingAsked) {
    auto voli = addUnit(P1, kVolibearFurious, 0, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 6, 1);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(voli, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(elsewhere).damage_marked, 0);
}

// ─── Not a defect: nothing to choose is fine, and "none" is on offer ────────

// 230 Albus Ferros: "When you play me, spend any number of buffs. For each
// buff spent, channel 1 rune exhausted."
TEST_F(VerifyCardsTest, Pin_AlbusFerros_NoneOnTheFirstPrompt_SpendsNothingAndChannelsNothing) {
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).buff_count = 1;
    addRuneToRuneDeck(P1, Domain::Order);
    auto albus = addToHand(P1, kAlbusFerros);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "None"
    playThroughEngine(albus, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0));
    EXPECT_TRUE(onBoard(albus));
    EXPECT_EQ(o(unit).buff_count, 1);
    EXPECT_EQ(ps(P1).rune_deck.size(), 1u);
}

TEST_F(VerifyCardsTest, Pin_AlbusFerros_NoBuffToSpend_NothingAsked) {
    auto unit = addUnit(P1, kInvalidId, 2);
    addRuneToRuneDeck(P1, Domain::Order);
    auto albus = addToHand(P1, kAlbusFerros);
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(albus, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(albus));
    EXPECT_EQ(o(unit).buff_count, 0);
    EXPECT_EQ(ps(P1).rune_deck.size(), 1u);
}

// 376 Janna, Savior: "heal your units here, then move up to one enemy unit
// from here to its base."
TEST_F(VerifyCardsTest, Pin_JannaSavior_NoEnemyHere_NothingAskedAndStillHeals) {
    auto hurt = addUnit(P1, kShipyardSkulker, 0);
    o(hurt).damage_marked = 2;
    auto elsewhere = addUnit(P2, kInvalidId, 2, 0);
    auto janna = addToHand(P1, kJannaSavior);
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(janna, &agent);                  // to my base
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(hurt).damage_marked, 0) << "the heal does not depend on the move";
    EXPECT_EQ(bfOf(elsewhere), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(VerifyCardsTest, Pin_JannaSavior_MovingNoEnemyIsTheFirstPromptsNo_AndStillHeals) {
    state.battlefields[0].controller = P1;
    auto hurt = addUnit(P1, kShipyardSkulker, 0, 0);
    o(hurt).damage_marked = 2;
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    auto janna = addToHand(P1, kJannaSavior);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No": zero of "up to one"
    playThroughEngine(janna, &agent, LocationId{BattlefieldLocation{0}});
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0));
    EXPECT_FALSE(agent.wasOffered(enemy)) << "no unit prompt after 'No'";
    EXPECT_EQ(bfOf(enemy), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(hurt).damage_marked, 0);
}

// 423 Fae Dragon: "When you play me, buff up to four friendly units."
TEST_F(VerifyCardsTest, Pin_FaeDragon_NoneOnTheFirstPrompt_AndFewerThanFour) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto dragon = addToHand(P1, kFaeDragon);
    EnginePathAgent none(EnginePathAgent::script({0}));    // "None"
    playThroughEngine(dragon, &none);
    ASSERT_EQ(none.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(none, 0, 0));
    EXPECT_EQ(o(u1).buff_count, 0);
    EXPECT_EQ(o(u2).buff_count, 0);
    EXPECT_EQ(o(dragon).buff_count, 0);

    EnginePathAgent one(pickingInOrder({u2, kInvalidId}));  // u2, then "None"
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &one);
    EXPECT_EQ(one.prompts.size(), 2u);
    EXPECT_EQ(o(u2).buff_count, 1);
    EXPECT_EQ(o(u1).buff_count, 0);
    EXPECT_EQ(o(dragon).buff_count, 0);
}

// Batch M1: was Pin_FaeDragon_NoUnbuffedFriendlyUnit_NothingAsked (only
// unbuffed units were offered). The units are targets chosen as the ability
// is finalized, and a unit with a buff can still be chosen for an action that
// buffs; it is just not buffed again (CR 427.1.c).
TEST_F(VerifyCardsTest, Pin_FaeDragon_ABuffedUnitIsOfferedAndNotBuffedAgain) {
    auto dragon = addUnit(P1, kFaeDragon, 0);
    o(dragon).buff_count = 1;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(dragon.raw())}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(agent.wasOffered(dragon));
    EXPECT_EQ(o(dragon).buff_count, 1);
}

// 497 Azir, Sovereign: "When I attack, you may move any number of your token
// units to this battlefield."
TEST_F(VerifyCardsTest, Pin_AzirSovereign_NoneOnTheFirstPromptMovesNoToken) {
    auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    auto t1 = addTokenUnit(P1, 1);
    auto t2 = addTokenUnit(P1, 1);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "None"
    fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0));
    EXPECT_TRUE(o(t1).isAtBase());
    EXPECT_TRUE(o(t2).isAtBase());
}

TEST_F(VerifyCardsTest, Pin_AzirSovereign_NoTokenUnit_NothingAsked) {
    auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    auto not_a_token = addUnit(P1, kInvalidId, 1);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(not_a_token).isAtBase());
}

// 540 Bard, Mercurial: "if you paid the additional cost, move any number of
// your units to an open battlefield."
TEST_F(VerifyCardsTest, Pin_BardMercurial_NoneOnTheFirstUnitPromptMovesNoUnit) {
    auto legend = addLegend(P1, kPurifier);
    addUnit(P2, kInvalidId, 2, 0);                     // battlefield 0 is occupied: only 1 is open
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto bard = addToHand(P1, kBardMercurial);
    EnginePathAgent agent(EnginePathAgent::script({1, 0}));   // pay the cost; "None"
    playThroughEngine(bard, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the cost, then the first unit prompt";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_TRUE(promptOffersValue(agent, 1, 0));
    EXPECT_TRUE(agent.wasOffered(u1));
    EXPECT_TRUE(o(u1).isAtBase());
    EXPECT_TRUE(o(bard).isAtBase());
}

TEST_F(VerifyCardsTest, Pin_BardMercurial_NoOpenBattlefield_PaidCostMovesNothing) {
    auto legend = addLegend(P1, kPurifier);
    addUnit(P2, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 1);
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto bard = addToHand(P1, kBardMercurial);
    EnginePathAgent agent(EnginePathAgent::script({1}));   // pay the cost
    playThroughEngine(bard, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "only the cost: there is nowhere to move to";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_TRUE(onBoard(bard));
    EXPECT_TRUE(o(u1).isAtBase());
}

// ─── CR 355.13 does not apply: "any number" is an additional COST ───────────
// (CR 355.10.c: objects named only as part of a cost are not targets.) The
// amount is still the player's: none may be paid while the card is affordable.

// 150 Kraken Hunter: "you may spend any number of buffs as an additional cost."
TEST_F(VerifyCardsTest, Pin_KrakenHunter_AffordableInFull_NoBuffHasToBeSpent) {
    auto buffed = addUnit(P1, kInvalidId, 2);
    o(buffed).buff_count = 2;
    auto hunter = addToHand(P1, kKrakenHunter);
    // "No" to every yes/no: the buff, and [Accelerate].
    EnginePathAgent agent(EnginePathAgent::script({0, 0}));
    playThroughEngine(hunter, &agent);                 // funded: [3] and both Power
    ASSERT_TRUE(onBoard(hunter));
    EXPECT_TRUE(askedLabel(agent, "Kraken Hunter: spend a buff"));
    EXPECT_EQ(o(buffed).buff_count, 2) << "zero buffs spent";
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "the full Power cost was paid";
}

TEST_F(VerifyCardsTest, Pin_KrakenHunter_NoBuff_PlayableAtFullCostWithoutTheQuestion) {
    auto hunter = addToHand(P1, kKrakenHunter);
    ASSERT_FALSE(playsOffered(hunter).empty()) << "no buff: still playable at full cost";
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No" to [Accelerate], if asked
    playThroughEngine(hunter, &agent);
    EXPECT_TRUE(onBoard(hunter));
    EXPECT_FALSE(askedLabel(agent, "Kraken Hunter: spend a buff"));
}

// 231 Commander Ledros: "you may kill any number of friendly units as an
// additional cost."
TEST_F(VerifyCardsTest, Pin_CommanderLedros_AffordableInFull_NoUnitHasToBeKilled) {
    auto unit = addUnit(P1, kInvalidId, 1);
    auto ledros = addToHand(P1, kCommanderLedros);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // "No"
    playThroughEngine(ledros, &agent);                 // funded: [6] and all four Power
    ASSERT_TRUE(onBoard(ledros));
    EXPECT_TRUE(askedLabel(agent, "Commander Ledros: kill a friendly unit"));
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_TRUE(onBoard(unit)) << "zero units killed";
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "the full Power cost was paid";
}

TEST_F(VerifyCardsTest, Pin_CommanderLedros_NoFriendlyUnit_PlayableAtFullCostWithoutTheQuestion) {
    auto ledros = addToHand(P1, kCommanderLedros);
    ASSERT_FALSE(playsOffered(ledros).empty()) << "no unit: still playable at full cost";
    EnginePathAgent agent(EnginePathAgent::script({}));
    playThroughEngine(ledros, &agent);
    EXPECT_TRUE(onBoard(ledros));
    EXPECT_TRUE(agent.prompts.empty());
}


// ═══ Tranche 8: 9.W1 zero objects for non-spell cards (CR 355.13), group T8B ═══
// Already in the file and used below: kElderDragon (680), kCorruptedDragon (878),
// kDarkChild (315), kEmperorsDivide (366).



// ═══ 9.W1: "any number of" / "up to N" on non-spell cards (CR 355.13) ═══════

// ─── "ready up to 2 runes": 666, 315, 284, 932 (all fixed the same way) ─────
// The controller names the runes one at a time; "None" (value 0) is on every
// prompt. Which rune matters: one that will be recycled for its Power is
// better left exhausted (CR 164.2.b works on an exhausted rune too).

// Fails on the old code: the first two exhausted runes in map order were
// readied without a prompt.
TEST_F(VerifyCardsTest, GentleGemdragon_ThePlayerPicksTheRunesAndMayReadyFewer) {
    auto dragon = addUnit(P1, kGentleGemdragon, 0, 0);
    auto r1 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Body, /*exhausted=*/true);
    // The third rune, then "None".
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r3.raw()), 0}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "\"None\" is on the first prompt";
    EXPECT_TRUE(agent.wasOffered(theirs));   // bare "runes": an opponent's too
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
    EXPECT_TRUE(o(theirs).is_exhausted);
}

// No exhausted rune: the trigger resolves and asks nothing.
TEST_F(VerifyCardsTest, Pin_GentleGemdragon_WithNoExhaustedRuneNothingIsAsked) {
    auto dragon = addUnit(P1, kGentleGemdragon, 0, 0);
    auto ready = addRune(P1, Domain::Body);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(ready).is_exhausted);
}

// Fails on the old code: the two lowest-id exhausted runes were readied
// without a prompt.
TEST_F(VerifyCardsTest, DarkChild315_ThePlayerPicksTheRunesAndMayReadyFewer) {
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Chaos, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r3.raw()), 0}));
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(agent, 0, 0)) << "\"None\" is on the first prompt";
    EXPECT_TRUE(agent.wasOffered(theirs)) << "\"runes\": any rune on the board (no \"friendly\" in the text)";
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
    EXPECT_TRUE(o(theirs).is_exhausted);
}

// Fails on the old code (no prompt, both readied).
TEST_F(VerifyCardsTest, DarkChild315_MayReadyNoRune) {
    auto annie = addLegend(P1, kDarkChild);
    addRune(P1, Domain::Fury, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(readyRuneCount(P1), 0);
    EXPECT_EQ(exhaustedRuneCount(P1), 2);
}

// Fails on the old code: with two or fewer exhausted runes it readied them
// all without asking.
TEST_F(VerifyCardsTest, TargonsPeak_MayReadyFewerThanTwoRunes) {
    auto peak = battlefieldCard(0, kTargonsPeak);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    // The second rune, then "None".
    EnginePathAgent one(EnginePathAgent::script({static_cast<int>(r2.raw()), 0}));
    fireTriggerThroughEngine(peak, TriggerType::AtEndOfTurn, &one);
    ASSERT_EQ(one.prompts.size(), 2u);
    EXPECT_TRUE(promptOffersValue(one, 0, 0)) << "\"None\" is on the first prompt";
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);

    // "None" at once: the last exhausted rune stays exhausted.
    EnginePathAgent none(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(peak, TriggerType::AtEndOfTurn, &none);
    EXPECT_EQ(none.prompts.size(), 1u);
    EXPECT_TRUE(o(r1).is_exhausted);
}

// Fails on the old code: with two or more exhausted runes a second rune had
// to be named.
TEST_F(VerifyCardsTest, CuratorOfTheSands_MayReadyFewerThanTwoRunes) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    // Yes; the second rune; "None".
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(r2.raw()), 0}));
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(agent, 1, 0)) << "\"None\" is on the first rune prompt";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_TRUE(o(r3).is_exhausted);
}

// Fails on the old code: a single exhausted rune was readied unasked once the
// "may" was accepted.
TEST_F(VerifyCardsTest, CuratorOfTheSands_MayExhaustAndReadyNoRune) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    // Yes; "None".
    EnginePathAgent agent(EnginePathAgent::script({1, 0}));
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(o(legend).is_exhausted) << "the cost was paid";
    EXPECT_TRUE(o(r1).is_exhausted);
}

// Fails on the old code: with no exhausted rune the "may" was not offered.
// "up to 2" includes none, so the cost can be paid for nothing (CR 355.13).
TEST_F(VerifyCardsTest, CuratorOfTheSands_OfferedWithNoExhaustedRune) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto ready = addRune(P1, Domain::Calm);
    auto src = addUnit(P1, kInvalidId, 3);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u) << "the yes/no only: there is no rune to name";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_FALSE(o(ready).is_exhausted);
}

// ─── Elder Dragon 680 (not a defect) ────────────────────────────────────────
// "choose up to one enemy unit at each location": with no enemy unit anywhere
// the play effect resolves and asks nothing; with units, "None" at every
// location leaves them all untouched.
TEST_F(VerifyCardsTest, Pin_ElderDragon_ZeroUnitsChosen) {
    auto dragon = addUnit(P1, kElderDragon, 0, 0);
    EnginePathAgent nobody(EnginePathAgent::script({}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &nobody);
    EXPECT_TRUE(nobody.prompts.empty());

    auto at_base = addUnit(P2, kInvalidId, 5);
    auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    EnginePathAgent none(EnginePathAgent::script({0, 0}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &none);
    ASSERT_EQ(none.prompts.size(), 2u) << "one prompt per location with an enemy unit";
    EXPECT_TRUE(promptOffersValue(none, 0, 0));
    EXPECT_TRUE(promptOffersValue(none, 1, 0));
    EXPECT_EQ(o(at_base).damage_marked, 0);
    EXPECT_EQ(o(at_bf).damage_marked, 0);
}

// ─── Corrupted Dragon 878 (not a defect) ────────────────────────────────────
// "you may move any number of enemy units here each with 5 [M] or less to
// their base": nothing eligible, nothing asked; with eligible units the first
// prompt has the "stop" option (the one with no object).
TEST_F(VerifyCardsTest, Pin_CorruptedDragon_ZeroUnitsMoved) {
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    auto big = addUnit(P2, kInvalidId, 6, 0);
    EnginePathAgent nobody(EnginePathAgent::script({}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &nobody);
    EXPECT_TRUE(nobody.prompts.empty());
    EXPECT_EQ(bfOf(big), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});

    auto small1 = addUnit(P2, kInvalidId, 2, 0);
    auto small2 = addUnit(P2, kInvalidId, 3, 0);
    // Batch M1: the leading "you may" is a prompt of its own (CR 383.3.a):
    // yes, then "stop" on the first unit prompt (was one prompt).
    EnginePathAgent stop([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.choice_label.rfind("Yes", 0) == 0) return i;
        for (const auto& i : legal)
            if (i.chosen_objects.empty()) return i;
        ADD_FAILURE() << "no 'stop' option";
        return legal.front();
    });
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &stop);
    EXPECT_EQ(stop.prompts.size(), 2u);
    EXPECT_TRUE(stop.wasOffered(small1));
    EXPECT_TRUE(stop.wasOffered(small2));
    EXPECT_FALSE(stop.wasOffered(big));
    EXPECT_EQ(bfOf(small1), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(bfOf(small2), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// ─── Forge of the Future 212 (not a defect) ─────────────────────────────────
// "Kill this: Recycle up to 4 cards from trashes." With both trashes empty the
// ability is still offered (zero is a legal choice, CR 355.13) and its cost
// still paid. The cards are chosen as it is activated, before its cost (CR
// 403.2; testing phase D41 — they used to be chosen as it resolved, by when
// the Forge itself was in a trash): with nothing to choose nothing is asked.
TEST_F(VerifyCardsTest, Pin_ForgeOfTheFuture_ZeroCardsRecycled) {
    ASSERT_TRUE(ps(P1).trash.empty());
    ASSERT_TRUE(ps(P2).trash.empty());
    auto forge = addGear(P1, kForgeOfTheFuture);
    bool offered = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == forge) offered = true;
    });
    EXPECT_TRUE(offered) << "activatable with nothing to recycle (CR 355.13)";
    const int deck_before = deckSize(P1);
    EnginePathAgent none(EnginePathAgent::script({}));
    activate(forge, &none);
    EXPECT_TRUE(none.prompts.empty()) << "nothing in the trashes as it is activated";
    EXPECT_TRUE(inTrash(P1, forge)) << "\"Kill this\" was paid; nothing was recycled";
    EXPECT_EQ(deckSize(P1), deck_before);
}

// ─── Emperor's Divide 366 from facedown ─────────────────────────────────────
// From hand, "any number" may be zero (CR 355.13, pinned above). From facedown
// the choice is tied to the battlefield it was hidden at (CR 811.1.d.2). This
// test used to pin that with no friendly unit there the spell is NOT offered
// (CR 811.1.d read as overriding 355.13). JUDGE RULING 2026-10-11 (plans/09
// 9.Z4): it CAN be played choosing zero units — its first two expectations
// are flipped by that ruling; a unit at another battlefield is still no
// target, and with a unit at its battlefield the plays are as before.
TEST_F(VerifyCardsTest, EmperorsDivide_FromFacedownWithNoFriendlyUnitThereIsPlayedChoosingNone) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    auto spell = addToHand(P1, kEmperorsDivide);
    {
        auto& h = ps(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        auto& card = o(spell);
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = BattlefieldId::fromRaw(0);
        card.hidden_on_turn = 2;
        state.battlefields[0].facedown.push_back(spell);
    }
    // The plays of the facedown card offered while P1 has priority in a
    // closed state (a hidden card has [Reaction], CR 811.6).
    auto hiddenPlays = [&]() {
        std::vector<Intent> out;
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = P1;
        state.turn.oc_state = OpenClosedState::Closed;
        state.turn.priority_holder = P1;
        withEngine([&](GameEngine& e) {
            for (const auto& i : e.generateLegalActions())
                if (i.card == spell) out.push_back(i);
        });
        state.turn.oc_state = OpenClosedState::Open;
        state.turn.priority_holder = std::nullopt;
        return out;
    };

    {
        const auto none = hiddenPlays();
        ASSERT_EQ(none.size(), 1u) << "no friendly unit at any battlefield: played choosing none";
        EXPECT_TRUE(none[0].targets.empty());
    }
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    {
        const auto none = hiddenPlays();
        ASSERT_EQ(none.size(), 1u);
        EXPECT_TRUE(none[0].targets.empty())
            << "a unit at another battlefield is no target (811.1.d.2): still the empty play";
    }

    // The control: a friendly unit at its battlefield makes it playable.
    auto here = addUnit(P1, kInvalidId, 2, 0);
    const auto plays = hiddenPlays();
    ASSERT_EQ(plays.size(), 1u) << "this setup does offer a hidden Emperor's Divide";
    // One play; the units are chosen as it is played, only at its battlefield
    // (testing phase G17: the intent used to name a unit there).
    EXPECT_TRUE(plays[0].targets.empty());

    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(here.raw())}));
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(plays[0]);
        e.testHook_runChain();
    }, &agent);
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "only the units at that battlefield";
    EXPECT_TRUE(o(here).isAtBase());
    EXPECT_EQ(bfOf(elsewhere), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
    EXPECT_TRUE(inTrash(P1, spell));
}

// ═══ Tranche 9: 9.Z2 rune picks at finalization, 9.Z3 whose runes ═══════════
// ═══ 9.Z2: "ready up to N runes" — the runes are named at finalization ══════
// CR 355.5 / 355.5.b (the Targon's Peak example) / 383.3: a triggered
// ability's choices are made as it is finalized, before anyone can respond.
// Each *_NamedBeforeTheOpponentCanRespond test fails on the old code: at the
// opponent's first priority no prompt had been asked yet (the runes were
// picked as the ability resolved).
// Each *_ReadiedOrGoneInResponseIsSkipped test fails on the old code: asked at
// resolution, the two named runes were no longer on offer and the third rune
// was readied in their place.

// ─── Dark Child 315 ─────────────────────────────────────────────────────────
TEST_F(VerifyCardsTest, DarkChild315_RunesAreNamedBeforeTheOpponentCanRespond) {
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Chaos, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r3, r1}));
    size_t asked_at_response = 99;
    bool readied_at_response = true;
    fireAndProbe(annie, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        readied_at_response = !live.getObject(r1).is_exhausted || !live.getObject(r3).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 2u) << "both runes are named as the ability is finalized";
    EXPECT_FALSE(readied_at_response) << "they are readied only as it resolves";
    EXPECT_EQ(agent.prompts.size(), 2u) << "nothing is asked again at resolution";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

TEST_F(VerifyCardsTest, DarkChild315_ARuneReadiedOrGoneInResponseIsSkipped) {
    // The board alone cannot show the helper dropped the stale picks
    // (EffectExecutor::readyObject refuses a ready or off-board rune anyway):
    // the card's own count of what selectSeveral handed back does (9.HH1).
    std::vector<std::string> log;
    auto log_conn = events.on_log.connect([&](const LogEvent& e) { log.push_back(e.message); });
    auto logged = [&](const std::string& part) {
        for (const auto& line : log) if (line.find(part) != std::string::npos) return true;
        return false;
    };
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Chaos, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r1, r2}));
    fireAndProbe(annie, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        live.getObject(r1).is_exhausted = false;   // in response: readied by something else
        removeFromBoard(live, r2);                 // in response: off the board
    });
    EXPECT_EQ(agent.prompts.size(), 2u) << "no new choice as it resolves";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(onBoard(r2));
    EXPECT_TRUE(o(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
    log_conn.disconnect();
    EXPECT_FALSE(logged("DARK CHILD: readied")) << "no trace line: zero runes were handed back by the helper";
}

// ─── Targon's Peak 284 ──────────────────────────────────────────────────────
TEST_F(VerifyCardsTest, TargonsPeak_RunesAreNamedBeforeTheOpponentCanRespond) {
    auto peak = battlefieldCard(0, kTargonsPeak);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r3, r1}));
    size_t asked_at_response = 99;
    bool readied_at_response = true;
    // The delayed end-of-turn trigger (CR 355.5.b: its choices are made when
    // IT is finalized).
    fireAndProbe(peak, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        readied_at_response = !live.getObject(r1).is_exhausted || !live.getObject(r3).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 2u) << "both runes are named as the delayed trigger is finalized";
    EXPECT_FALSE(readied_at_response) << "they are readied only as it resolves";
    EXPECT_EQ(agent.prompts.size(), 2u) << "nothing is asked again at resolution";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

TEST_F(VerifyCardsTest, TargonsPeak_ARuneReadiedOrGoneInResponseIsSkipped) {
    // The board alone cannot show the helper dropped the stale picks
    // (EffectExecutor::readyObject refuses a ready or off-board rune anyway):
    // the card's own count of what selectSeveral handed back does (9.HH1).
    std::vector<std::string> log;
    auto log_conn = events.on_log.connect([&](const LogEvent& e) { log.push_back(e.message); });
    auto logged = [&](const std::string& part) {
        for (const auto& line : log) if (line.find(part) != std::string::npos) return true;
        return false;
    };
    auto peak = battlefieldCard(0, kTargonsPeak);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r1, r2}));
    fireAndProbe(peak, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        live.getObject(r1).is_exhausted = false;
        removeFromBoard(live, r2);
    });
    EXPECT_EQ(agent.prompts.size(), 2u) << "no new choice as it resolves";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(onBoard(r2));
    EXPECT_TRUE(o(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
    log_conn.disconnect();
    EXPECT_TRUE(logged("TARGON'S PEAK: readied 0 runes")) << "the helper handed back zero runes";
}

// The conquer ability itself has no choice (CR 355.5.b): its finalize pass
// must not schedule the delayed trigger — it is scheduled once, as the conquer
// ability resolves. Guards the end-of-choices marker of the conquer branch
// (without it: one delayed ability at the opponent's priority, two afterwards).
TEST_F(VerifyCardsTest, TargonsPeak_ConquerSchedulesTheDelayedTriggerOnceAsItResolves) {
    auto peak = battlefieldCard(0, kTargonsPeak);
    addRune(P1, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({}));
    size_t scheduled_at_response = 99;
    fireAndProbe(peak, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        scheduled_at_response = live.delayed_abilities.size();
    });
    EXPECT_EQ(scheduled_at_response, 0u) << "nothing happens while the conquer ability is finalized";
    EXPECT_EQ(state.delayed_abilities.size(), 1u);
    EXPECT_TRUE(agent.prompts.empty()) << "no rune is named for the conquer ability";
    EXPECT_EQ(exhaustedRuneCount(P1), 1);
}

// ─── Gentle Gemdragon 666 ───────────────────────────────────────────────────
TEST_F(VerifyCardsTest, GentleGemdragon_RunesAreNamedBeforeTheOpponentCanRespond) {
    auto dragon = addUnit(P1, kGentleGemdragon, 0, 0);
    auto r1 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r3, r1}));
    size_t asked_at_response = 99;
    bool readied_at_response = true;
    fireAndProbe(dragon, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        readied_at_response = !live.getObject(r1).is_exhausted || !live.getObject(r3).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 2u) << "both runes are named as the ability is finalized";
    EXPECT_FALSE(readied_at_response) << "they are readied only as it resolves";
    EXPECT_EQ(agent.prompts.size(), 2u) << "nothing is asked again at resolution";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

TEST_F(VerifyCardsTest, GentleGemdragon_ARuneReadiedOrGoneInResponseIsSkipped) {
    // The board alone cannot show the helper dropped the stale picks
    // (EffectExecutor::readyObject refuses a ready or off-board rune anyway):
    // the card's own count of what selectSeveral handed back does (9.HH1).
    std::vector<std::string> log;
    auto log_conn = events.on_log.connect([&](const LogEvent& e) { log.push_back(e.message); });
    auto logged = [&](const std::string& part) {
        for (const auto& line : log) if (line.find(part) != std::string::npos) return true;
        return false;
    };
    auto dragon = addUnit(P1, kGentleGemdragon, 0, 0);
    auto r1 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r1, r2}));
    fireAndProbe(dragon, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        live.getObject(r1).is_exhausted = false;
        removeFromBoard(live, r2);
    });
    EXPECT_EQ(agent.prompts.size(), 2u) << "no new choice as it resolves";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(onBoard(r2));
    EXPECT_TRUE(o(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
    log_conn.disconnect();
    EXPECT_FALSE(logged("GENTLE GEMDRAGON: readied")) << "no trace line: zero runes were handed back by the helper";
}

// ─── Sona, Harmonious 73 ────────────────────────────────────────────────────
TEST_F(VerifyCardsTest, Sona_RunesAreNamedBeforeTheOpponentCanRespond) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r3, r1, kInvalidId}));   // two, then "None"
    size_t asked_at_response = 99;
    bool readied_at_response = true;
    fireAndProbe(sona, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        readied_at_response = !live.getObject(r1).is_exhausted || !live.getObject(r3).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 3u) << "the runes are named as the ability is finalized";
    EXPECT_FALSE(readied_at_response) << "they are readied only as it resolves";
    EXPECT_EQ(agent.prompts.size(), 3u) << "nothing is asked again at resolution";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

TEST_F(VerifyCardsTest, Sona_ARuneReadiedOrGoneInResponseIsSkipped) {
    // The board alone cannot show the helper dropped the stale picks
    // (EffectExecutor::readyObject refuses a ready or off-board rune anyway):
    // the card's own count of what selectSeveral handed back does (9.HH1).
    std::vector<std::string> log;
    auto log_conn = events.on_log.connect([&](const LogEvent& e) { log.push_back(e.message); });
    auto logged = [&](const std::string& part) {
        for (const auto& line : log) if (line.find(part) != std::string::npos) return true;
        return false;
    };
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r1, r2, kInvalidId}));
    fireAndProbe(sona, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        live.getObject(r1).is_exhausted = false;
        removeFromBoard(live, r2);
    });
    EXPECT_EQ(agent.prompts.size(), 3u) << "no new choice as it resolves";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(onBoard(r2));
    EXPECT_TRUE(o(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
    log_conn.disconnect();
    EXPECT_TRUE(logged("SONA, HARMONIOUS: readied 0 rune(s)")) << "the helper handed back zero runes";
}

// CR 383 (the Sona example): "if I'm at a battlefield" is asked when the
// ability goes on the chain; "If she is removed in reaction to the triggered
// ability, it will still resolve." Fails on the old code: the condition (and
// the rune prompts) came at resolution, so with Sona gone nothing was readied.
TEST_F(VerifyCardsTest, Sona_RemovedInResponse_TheAbilityStillReadiesTheNamedRunes) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({r2, kInvalidId}));
    fireAndProbe(sona, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        removeFromBoard(live, sona);
    });
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_FALSE(onBoard(sona));
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted) << "the ability resolves without its source";
}

// Not at a battlefield when the ability would go on the chain: nothing is
// asked and nothing readied (unchanged; the condition is still checked once).
TEST_F(VerifyCardsTest, Pin_Sona_NotAtABattlefield_NothingAskedNothingReadied) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, -1);   // at base
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(r1).is_exhausted);
}

// ─── Curator of the Sands 932 ───────────────────────────────────────────────
// Fails on the old code: the card had no finalize pass at all — at the
// opponent's first priority nothing had been asked and the legend was ready.
TEST_F(VerifyCardsTest, CuratorOfTheSands_MayCostAndRunesAreDecidedBeforeTheOpponentCanRespond) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    // Yes; the third rune; the first rune.
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(r3.raw()), static_cast<int>(r1.raw())}));
    size_t asked_at_response = 99;
    bool cost_paid_at_response = false;
    bool readied_at_response = true;
    causeAndProbe([&](GameState& live) {
        live.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        cost_paid_at_response = live.getObject(legend).is_exhausted;
        readied_at_response = !live.getObject(r1).is_exhausted || !live.getObject(r3).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 3u) << "the may and both runes, as the ability is finalized";
    EXPECT_TRUE(cost_paid_at_response) << "\"exhaust me\" is paid at finalization (CR 383.3.b)";
    EXPECT_FALSE(readied_at_response) << "the runes are readied only as it resolves";
    EXPECT_EQ(agent.prompts.size(), 3u) << "nothing is asked again at resolution";
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

// Fails on the old code: asked at resolution, the two named runes were no
// longer on offer and the third rune was readied in their place.
TEST_F(VerifyCardsTest, CuratorOfTheSands_ARuneReadiedOrGoneInResponseIsSkipped) {
    // The board alone cannot show the helper dropped the stale picks
    // (EffectExecutor::readyObject refuses a ready or off-board rune anyway):
    // the card's own count of what selectSeveral handed back does (9.HH1).
    std::vector<std::string> log;
    auto log_conn = events.on_log.connect([&](const LogEvent& e) { log.push_back(e.message); });
    auto logged = [&](const std::string& part) {
        for (const auto& line : log) if (line.find(part) != std::string::npos) return true;
        return false;
    };
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(r1.raw()), static_cast<int>(r2.raw())}));
    causeAndProbe([&](GameState& live) {
        live.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent, [&](GameState& live) {
        live.getObject(r1).is_exhausted = false;
        removeFromBoard(live, r2);
    });
    EXPECT_EQ(agent.prompts.size(), 3u) << "no new choice as it resolves";
    EXPECT_TRUE(o(legend).is_exhausted) << "the cost stays paid";
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_FALSE(onBoard(r2));
    EXPECT_TRUE(o(r2).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
    log_conn.disconnect();
    EXPECT_TRUE(logged("CURATOR OF THE SANDS: readied 0 rune(s)")) << "the helper handed back zero runes";
}

// Declined at finalization: no cost, no rune prompt, nothing at resolution.
TEST_F(VerifyCardsTest, Pin_CuratorOfTheSands_DeclinedAtFinalization_NothingIsPaidOrAsked) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    EnginePathAgent agent(EnginePathAgent::script({0}));   // No
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(o(legend).is_exhausted);
    EXPECT_TRUE(o(r1).is_exhausted);
}

// ─── Hwei, Brooding Painter 642 (not a defect) ──────────────────────────────
// "When I move, draw 1, then discard 1. Then, do the following based on the
// discarded card's type: ... Gear — Ready up to 2 runes." The rune choice
// belongs to the reflexive trigger the discard creates (CR 388.1.b, 389.1),
// not to the move ability: when the move ability is finalized there is nothing
// to name yet (which card is discarded is only known while it resolves).
TEST_F(VerifyCardsTest, Pin_Hwei_TheRunesAreNamedAtTheReflexiveTriggersFinalizationNotTheMoveAbilitys) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);                        // the card drawn
    auto gear = addToHand(P1, kSealOfStrength163);
    auto r1 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({gear, r3, r2}));   // the discard, then two runes
    size_t asked_at_response = 99;
    bool gear_in_hand_at_response = false;
    int deck_at_response = -1;
    fireAndProbe(hwei, TriggerType::WhenIMove, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        gear_in_hand_at_response = live.getObject(gear).zone == ZoneType::Hand;
        deck_at_response = static_cast<int>(live.player(P1).main_deck.size());
    });
    EXPECT_EQ(asked_at_response, 0u) << "the move ability has no choice of its own to finalize";
    EXPECT_TRUE(gear_in_hand_at_response) << "nothing is discarded before it resolves";
    EXPECT_EQ(deck_at_response, 1) << "nothing is drawn before it resolves";
    ASSERT_EQ(agent.prompts.size(), 3u) << "the discard, then the two runes";
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{r1, r2, r3}))
        << "the runes are offered once the discarded card's type is known";
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

// ═══ 9.Z3: whose runes ══════════════════════════════════════════════════════
// "ready up to 2 runes" (315, 284, 666, 932, 642) prints no "friendly" /
// "your": "rune" refers to any rune on the board (CR 133.3.a, 355.9.a.1) and
// only a printed restriction narrows a choice (CR 355.9.b), so an opponent's
// exhausted rune may be named. Each test fails on the old code: only the
// controller's runes were offered. Sona 73 prints "friendly runes": pinned.

TEST_F(VerifyCardsTest, DarkChild315_AnOpponentsExhaustedRuneMayBeReadied) {
    auto annie = addLegend(P1, kDarkChild);
    auto mine = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Fury, /*exhausted=*/true);
    auto their_ready = addRune(P2, Domain::Fury);
    EnginePathAgent agent(pickingInOrder({theirs, kInvalidId}));   // theirs, then "None"
    fireTriggerThroughEngine(annie, TriggerType::AtEndOfTurn, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{mine, theirs}))
        << "every exhausted rune on the board, whoever controls it";
    EXPECT_FALSE(agent.wasOffered(their_ready));
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_TRUE(o(mine).is_exhausted);
}

TEST_F(VerifyCardsTest, TargonsPeak_AnOpponentsExhaustedRuneMayBeReadied) {
    auto peak = battlefieldCard(0, kTargonsPeak);
    auto mine = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Fury, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({theirs, kInvalidId}));
    fireTriggerThroughEngine(peak, TriggerType::AtEndOfTurn, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{mine, theirs}));
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_TRUE(o(mine).is_exhausted);
}

TEST_F(VerifyCardsTest, GentleGemdragon_AnOpponentsExhaustedRuneMayBeReadied) {
    auto dragon = addUnit(P1, kGentleGemdragon, 0, 0);
    auto mine = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Body, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({theirs, kInvalidId}));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{mine, theirs}));
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_TRUE(o(mine).is_exhausted);
}

TEST_F(VerifyCardsTest, CuratorOfTheSands_AnOpponentsExhaustedRuneMayBeReadied) {
    auto legend = addLegend(P1, kCuratorOfTheSands);
    auto mine = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Calm, /*exhausted=*/true);
    auto src = addUnit(P1, kInvalidId, 3);
    // Yes; their rune; "None".
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(theirs.raw()), 0}));
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        st.getObject(src).card_counters["__ability_energy_cost"] = 7;
        events.emit(ObjectStateChangedEvent{src, "ability_activated"});
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{mine, theirs}));
    EXPECT_TRUE(o(legend).is_exhausted);
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_TRUE(o(mine).is_exhausted);
}

TEST_F(VerifyCardsTest, Hwei_DiscardingAGear_AnOpponentsExhaustedRuneMayBeReadied) {
    auto hwei = addUnit(P1, kHwei, 0, 0);
    addToDeck(P1, kMeditation);
    auto gear = addToHand(P1, kSealOfStrength163);
    auto mine = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(pickingInOrder({gear, theirs, kInvalidId}));
    fireTriggerThroughEngine(hwei, TriggerType::WhenIMove, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{mine, theirs}));
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_TRUE(o(mine).is_exhausted);
}

// Sona prints "ready up to 4 FRIENDLY runes": an opponent's exhausted rune is
// not offered, and with no friendly exhausted rune nothing is asked.
TEST_F(VerifyCardsTest, Pin_Sona_FriendlyRunesOnly_AnOpponentsRuneIsNotOffered) {
    EXPECT_NE(card_db.get(kSonaHarmonious).ability_text.find("friendly runes"), std::string::npos);
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto mine_ready = addRune(P1, Domain::Calm);
    auto theirs = addRune(P2, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(sona, TriggerType::AtEndOfTurn, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(theirs).is_exhausted);
    EXPECT_FALSE(o(mine_ready).is_exhausted);
}

// ═══ Tranche 10: 9.BB1 selectSeveral re-entry, 9.BB2 Aphelios 372 / Acceleration Gate 937 ═══
// ═══ Tranche 10: 9.BB1 selectSeveral is finalize-safe, 9.BB2 Aphelios 372 / Acceleration Gate 937 ═══

// ─── 9.BB1: selectSeveral, entered again with a list that has changed ───────
// A pick already stored is returned only while it is still in the caller's
// current list; one that is not is dropped, nothing takes its place and
// nothing is asked again. The five "ready up to N runes" cards now go through
// selectSeveral at resolution too (their own read-back loops are gone), so
// these run the helper's re-entry path; the *_ARuneReadiedOrGoneInResponseIsSkipped
// tests above cover "first readied, second gone".

// The FIRST of the two named runes is gone: the second is still readied.
TEST_F(VerifyCardsTest, DarkChild315_TheFirstOfTwoNamedRunesGone_TheSecondIsStillReadied) {
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Chaos, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r1.raw()), static_cast<int>(r2.raw())}));
    fireAndProbe(annie, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 2u) << "both runes are named at finalization";
        removeFromBoard(live, r1);                 // in response: the first pick leaves
    });
    ASSERT_EQ(agent.prompts.size(), 2u) << "no new choice as it resolves";
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{r1, r2, r3}));
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{r2, r3}));
    EXPECT_TRUE(o(r1).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_FALSE(o(r2).is_exhausted) << "the other named rune is still readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
}

// The list ran out at finalization (one exhausted rune, "up to 2"): a rune
// that becomes exhausted in response is not asked for as the ability resolves.
TEST_F(VerifyCardsTest, DarkChild315_ARuneExhaustedInResponseIsNotNamedAsItResolves) {
    auto annie = addLegend(P1, kDarkChild);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Chaos);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r1.raw())}));
    fireAndProbe(annie, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 1u);
        live.getObject(r2).is_exhausted = true;    // in response: a new candidate
    });
    ASSERT_EQ(agent.prompts.size(), 1u) << "the choice ended at finalization";
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{r1}));
    EXPECT_FALSE(o(r1).is_exhausted);
    EXPECT_TRUE(o(r2).is_exhausted) << "it was not a rune named at finalization";
}

// "Up to 4", stopped after two with "None": at resolution the stop is kept
// (no prompt for the runes left over), the first pick is gone and dropped,
// the second is readied.
TEST_F(VerifyCardsTest, Sona_StoppedEarly_TheFirstNamedRuneGone_TheSecondIsStillReadied) {
    auto sona = addUnit(P1, kSonaHarmonious, 0, 0);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r4 = addRune(P1, Domain::Calm);
    // r1; r2; "None".
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r1.raw()), static_cast<int>(r2.raw()), 0}));
    fireAndProbe(sona, TriggerType::AtEndOfTurn, &agent, [&](GameState& live) {
        EXPECT_EQ(agent.prompts.size(), 3u);
        removeFromBoard(live, r1);
        live.getObject(r4).is_exhausted = true;
    });
    ASSERT_EQ(agent.prompts.size(), 3u) << "no new choice as it resolves";
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "the third prompt was answered \"None\"";
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_TRUE(o(r3).is_exhausted);
    EXPECT_TRUE(o(r4).is_exhausted);
}

// A caller that asks only as it resolves: Fae Dragon 423 ("buff up to four
// friendly units"). The unit picked first leaves the board while the second
// prompt is open. The old helper then looked the first pick up in the rebuilt
// list and erased end() — undefined behaviour (this test crashes or corrupts
// the list there; it cannot be relied on to fail cleanly).
TEST_F(VerifyCardsTest, FaeDragon_AUnitPickedEarlierLeavesBetweenTwoPrompts_TheOtherPickIsBuffed) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto u3 = addUnit(P1, kInvalidId, 2);
    auto dragon = addUnit(P1, kFaeDragon, 3);
    GameEngine* live = nullptr;
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        auto object = [&](GameObjectId id) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            ADD_FAILURE() << "prompt " << n << ": object " << id << " was not offered";
            return legal.front();
        };
        switch (n++) {
        case 0: return object(u1);
        case 1:
            if (live) removeFromBoard(live->mutableState(), u1);   // u1 is gone before the answer
            return object(u2);
        default: return legal.back();                              // "None"
        }
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 3u) << "u1, u2, then one more prompt answered \"None\"";
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{u1, u2, u3, dragon}));
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{u2, u3, dragon}));
    EXPECT_EQ(promptObjects(agent, 2), (std::vector<GameObjectId>{u3, dragon}))
        << "what the current list still holds, without the stored picks";
    EXPECT_FALSE(onBoard(u1));
    EXPECT_EQ(o(u1).buff_count, 0) << "a pick that is no longer in the list is dropped";
    EXPECT_EQ(o(u2).buff_count, 1) << "the other pick still takes effect";
    EXPECT_EQ(o(u3).buff_count, 0);
    EXPECT_EQ(o(dragon).buff_count, 0);
}

// The same for Albus Ferros 230 ("spend any number of buffs"), whose list has
// one entry per buff: the unit picked first loses its buff while the second
// prompt is open. Old helper: erase(end()), undefined behaviour.
TEST_F(VerifyCardsTest, AlbusFerros_ABuffPickedEarlierIsGoneBetweenTwoPrompts_TheOtherIsSpent) {
    auto u1 = addUnit(P1, kInvalidId, 2);
    auto u2 = addUnit(P1, kInvalidId, 2);
    o(u1).buff_count = 1;
    o(u2).buff_count = 1;
    addRuneToRuneDeck(P1, Domain::Order);
    addRuneToRuneDeck(P1, Domain::Order);
    auto albus = addUnit(P1, kAlbusFerros, 3);
    GameEngine* live = nullptr;
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        auto object = [&](GameObjectId id) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            ADD_FAILURE() << "prompt " << n << ": object " << id << " was not offered";
            return legal.front();
        };
        switch (n++) {
        case 0: return object(u1);
        case 1:
            if (live) live->mutableState().getObject(u1).buff_count = 0;   // its buff is gone
            return object(u2);
        default:
            ADD_FAILURE() << "a third prompt: nothing was left to pick";
            return legal.back();
        }
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().queueTriggeredAbility(albus, P1, TriggerType::WhenYouPlayMe);
        e.testHook_runChain();
        live = nullptr;
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_EQ(promptObjects(agent, 0), (std::vector<GameObjectId>{u1, u2}));
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{u2}));
    EXPECT_EQ(o(u2).buff_count, 0) << "the buff that is still there is spent";
    EXPECT_EQ(ps(P1).rune_deck.size(), 1u) << "one buff spent: one rune channeled, not two";
}

// ─── 9.BB2: Aphelios, Exalted 372 — "Ready 2 runes" ─────────────────────────
// Fails on the old code: the runes were asked for as the ability resolved (one
// prompt at the opponent's first priority) and only the controller's runes in
// its base were offered.
TEST_F(VerifyCardsTest, ApheliosExalted_NamesHisRunesBeforeTheOpponentCanRespond_AnOpponentsToo) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto mine1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto mine2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Calm, /*exhausted=*/true);
    auto their_ready = addRune(P2, Domain::Calm);
    // Mode 0 ("Ready 2 runes"); their rune; my second rune.
    EnginePathAgent agent(EnginePathAgent::script(
        {0, static_cast<int>(theirs.raw()), static_cast<int>(mine2.raw())}));
    size_t asked_at_response = 99;
    bool readied_at_response = true;
    fireAndProbe(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        readied_at_response = !live.getObject(theirs).is_exhausted ||
                              !live.getObject(mine2).is_exhausted;
    });
    EXPECT_EQ(asked_at_response, 3u) << "the mode and both runes are chosen at finalization";
    EXPECT_FALSE(readied_at_response) << "they are readied only as it resolves";
    ASSERT_EQ(agent.prompts.size(), 3u) << "nothing is asked again at resolution";
    EXPECT_EQ(promptObjects(agent, 1), (std::vector<GameObjectId>{mine1, mine2, theirs}))
        << "every exhausted rune on the board, whoever controls it";
    EXPECT_EQ(promptObjects(agent, 2), (std::vector<GameObjectId>{mine1, mine2}));
    EXPECT_EQ(agent.prompts[1].size(), 3u) << "\"Ready 2 runes\" is not \"up to\": no \"None\"";
    EXPECT_EQ(agent.prompts[2].size(), 2u);
    EXPECT_FALSE(agent.wasOffered(their_ready));
    EXPECT_FALSE(o(theirs).is_exhausted);
    EXPECT_FALSE(o(mine2).is_exhausted);
    EXPECT_TRUE(o(mine1).is_exhausted);
}

// Fails on the old code: asked at resolution, with r1 gone the two runes left
// were readied — r3 in the place of the one that left.
TEST_F(VerifyCardsTest, ApheliosExalted_ARuneGoneInResponseIsSkipped) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(r1.raw()), static_cast<int>(r2.raw())}));
    fireAndProbe(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent, [&](GameState& live) {
        removeFromBoard(live, r1);                 // in response: the first named rune leaves
    });
    EXPECT_EQ(agent.prompts.size(), 3u) << "no new choice as it resolves";
    EXPECT_TRUE(o(r1).is_exhausted) << "a rune that left the board is not readied";
    EXPECT_FALSE(o(r2).is_exhausted) << "the other named rune is still readied";
    EXPECT_TRUE(o(r3).is_exhausted) << "no other rune takes the place of a named one";
}

// Exactly two exhausted runes: both are readied and there is nothing to ask.
// Fails on the old code: the opponent's rune was not one of "runes".
TEST_F(VerifyCardsTest, ApheliosExalted_TwoExhaustedRunes_BothAreReadiedWithoutAsking) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto mine = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto theirs = addRune(P2, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the mode only";
    EXPECT_FALSE(o(mine).is_exhausted);
    EXPECT_FALSE(o(theirs).is_exhausted);
}

// Fewer than two exhausted runes: the one there is is readied (CR 055).
TEST_F(VerifyCardsTest, Pin_ApheliosExalted_OneExhaustedRune_ItIsReadiedWithoutAsking) {
    auto aphelios = addUnit(P1, kApheliosExalted, 4);
    auto only = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto ready = addRune(P1, Domain::Calm);
    EnginePathAgent agent(EnginePathAgent::script({0}));
    fireTriggerThroughEngine(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the mode only";
    EXPECT_FALSE(o(only).is_exhausted);
    EXPECT_FALSE(o(ready).is_exhausted);
}

// ─── 9.BB2: Acceleration Gate 937 — not a defect ────────────────────────────
// "Ready up to 4 units, gear, and/or runes.": no "friendly" / "your", so the
// opponent's exhausted rune, unit and gear are offered beside the player's.
TEST_F(VerifyCardsTest, Pin_AccelerationGate_OffersAnOpponentsExhaustedRuneUnitAndGear) {
    EXPECT_EQ(card_db.get(kAccelerationGate).ability_text,
              "Ready up to 4 units, gear, and/or runes.");
    auto my_rune = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto their_rune = addRune(P2, Domain::Mind, /*exhausted=*/true);
    auto their_ready_rune = addRune(P2, Domain::Mind);
    auto their_unit = addUnit(P2, kInvalidId, 2);
    auto their_gear = addGear(P2, kInvalidId);
    o(their_unit).is_exhausted = true;
    o(their_gear).is_exhausted = true;
    auto gate = addToHand(P1, kAccelerationGate);
    // Their rune, then "Done".
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(their_rune.raw()), 0}));
    playThroughEngine(gate, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    // Batch M1: chosen as the spell is played, when nothing says what will be
    // exhausted as it resolves, and the text has no "exhausted" restriction
    // (CR 355.9): a ready rune is offered too (was left out).
    EXPECT_EQ(promptObjects(agent, 0),
              (std::vector<GameObjectId>{my_rune, their_rune, their_ready_rune, their_unit,
                                         their_gear}));
    EXPECT_TRUE(agent.wasOffered(their_ready_rune));
    EXPECT_FALSE(o(their_rune).is_exhausted);
    EXPECT_TRUE(o(my_rune).is_exhausted);
    EXPECT_TRUE(o(their_unit).is_exhausted);
    EXPECT_TRUE(o(their_gear).is_exhausted);
}


// ═══ Tranche 11 (T11A): 9.EE1 nameable options, 9.EE2 nothing asked at resolution ═══
// Snippet T11A (9.EE1, 9.EE3) — paste into tests/cards/test_review_fix_verify_cards.cpp.
// No new constants, no new fixture helper (kRekSaiSwarmQueen, kVoidBurrower,
// kMeditation, kShipyardSkulker and promptOffersValue are already in the file).

// ─── 9.EE1: a decline option an agent can name ──────────────────────────────
// "You may banish one": the option that banishes none carries chosen_value 0
// (it used to carry neither a value nor an object, so only its position told
// it apart). Both tests fail on the old code: the script's 0 matched no
// option of the prompt (a strict-script failure), the first option was taken
// instead — for Rek'Sai that is the top card, which was banished and played.

TEST_F(VerifyCardsTest, RekSaiSwarmQueen_BanishNoneRecyclesBothRevealedCards) {
    auto reksai = addUnit(P1, kRekSaiSwarmQueen, 0, 0);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    // The unit could be paid for: it is the answer, not the cost, that keeps it unplayed.
    const int energy = card_db.get(kShipyardSkulker).energy_cost;
    ps(P1).rune_pool.energy = energy;
    EnginePathAgent agent(EnginePathAgent::script({
        1,     // "may reveal the top 2 cards": yes
        0,     // "You may banish one": banish none
    }));
    fireTriggerThroughEngine(reksai, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, the banish; nothing is played, so no location";
    const auto& banish = agent.prompts[1];
    ASSERT_EQ(banish.size(), 3u) << "the two revealed cards, then \"banish none\"";
    EXPECT_TRUE(promptOffersValue(agent, 1, 0));
    EXPECT_EQ(banish[0].chosen_objects, (std::vector<GameObjectId>{top}));
    EXPECT_EQ(banish[1].chosen_objects, (std::vector<GameObjectId>{below}));
    EXPECT_TRUE(banish[2].chosen_objects.empty()) << "the decline stays the last option";
    EXPECT_EQ(banish[2].chosen_value, std::optional<int>{0});
    EXPECT_FALSE(banish[0].chosen_value.has_value()) << "a card is named by its object only";
    EXPECT_FALSE(banish[1].chosen_value.has_value());
    EXPECT_FALSE(onBoard(top));
    EXPECT_FALSE(inBanishment(P1, top));
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 0) << "\"You MAY banish one\"";
    EXPECT_EQ(ps(P1).rune_pool.energy, energy) << "nothing was played";
    EXPECT_EQ(deckSize(P1), 2) << "\"Recycle the rest\": both";
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_TRUE(inDeck(P1, below));
}

TEST_F(VerifyCardsTest, VoidBurrower_BanishNoneIsNamedByValueAndStaysTheFirstOption) {
    auto legend = addLegend(P1, kVoidBurrower);
    auto below = addToDeck(P1, kMeditation);
    auto top = addToDeck(P1, kShipyardSkulker);
    const int energy = card_db.get(kShipyardSkulker).energy_cost;
    ps(P1).rune_pool.energy = energy;
    EnginePathAgent agent(EnginePathAgent::script({
        1,     // "may exhaust me to reveal 2": yes
        0,     // "You may banish one": banish none
    }));
    fireTriggerThroughEngine(legend, TriggerType::WhenIConquer, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u) << "the may, the banish";
    const auto& banish = agent.prompts[1];
    ASSERT_EQ(banish.size(), 3u) << "\"banish none\", then the two revealed cards";
    EXPECT_TRUE(banish[0].chosen_objects.empty()) << "the decline stays the first option";
    EXPECT_EQ(banish[0].chosen_value, std::optional<int>{0});
    EXPECT_EQ(banish[1].chosen_objects, (std::vector<GameObjectId>{top}));
    EXPECT_EQ(banish[2].chosen_objects, (std::vector<GameObjectId>{below}));
    EXPECT_FALSE(banish[1].chosen_value.has_value()) << "a card is named by its object only";
    EXPECT_FALSE(banish[2].chosen_value.has_value());
    EXPECT_TRUE(o(legend).is_exhausted) << "the cost was paid all the same";
    EXPECT_FALSE(onBoard(top));
    EXPECT_FALSE(inBanishment(P1, top));
    EXPECT_EQ(ps(P1).cards_banished_this_turn, 0);
    EXPECT_EQ(ps(P1).rune_pool.energy, energy) << "nothing was played";
    EXPECT_EQ(deckSize(P1), 2) << "\"Recycle the rest\": both";
    EXPECT_TRUE(inDeck(P1, top));
    EXPECT_TRUE(inDeck(P1, below));
}

// 9.EE3 (the readiable() re-check after selectSeveral removed in 315, 284, 666,
// 932, 73): no behaviour change, so no new test. Pinned by the existing
//   DarkChild315_ / TargonsPeak_ / GentleGemdragon_ / Sona_ /
//   CuratorOfTheSands_ARuneReadiedOrGoneInResponseIsSkipped
//   DarkChild315_TheFirstOfTwoNamedRunesGone_TheSecondIsStillReadied
// (and the other 9.BB1 tests after it).


// ═══ Tranche 11 (T11B): 9.EE1 nameable options, 9.EE2 nothing asked at resolution ═══
// tests_T11B.cpp — item 9.EE2 (the Sona shape): a callback opted into choices at
// finalization that returned before its end-of-choices marker with nothing
// stored, so the resolution asked (and paid) when the condition had changed.
// One test per fixed card: at finalization the condition is false, the probe at
// the opponent's first priority makes it true, and nothing is asked or paid as
// the ability resolves. Each fails on the old code at the strict script (a
// prompt arrives after the script ran out) and at the prompt count.





// ─── 9.EE2: a "may" skipped at finalization is not asked at resolution ──────

// 534 Treasure Hoard: [1] unpayable as the ability is finalized, payable by
// the time it resolves.
TEST_F(VerifyCardsTest, TreasureHoard_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto hoard = battlefieldCard(0, kTreasureHoard);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(hoard, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty()) << "the \"may\" is decided at finalization (CR 383.3.a)";
    EXPECT_EQ(ps(P1).rune_pool.energy, 1) << "nothing is paid as it resolves";
    EXPECT_EQ(countNamed(P1, "Gold"), 0);
}

// 277 Monastery of Hirana: no buff to spend at finalization, one by resolution.
TEST_F(VerifyCardsTest, MonasteryOfHirana_NoBuffAtFinalization_NothingAskedOrSpentAtResolution) {
    auto monastery = battlefieldCard(0, kMonasteryOfHirana);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(monastery, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        live.getObject(unit).buff_count = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(unit).buff_count, 1) << "no buff is spent as it resolves";
    EXPECT_EQ(handSize(P1), 0);
}

// 770 Ripper's Bay.
TEST_F(VerifyCardsTest, RippersBay_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto bay = battlefieldCard(0, kRippersBay);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(bay, TriggerType::WhenAUnitReturnsToHandHere, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
}

// 765 Dusk Rose Lab: no unit here at finalization; one arrives in response.
TEST_F(VerifyCardsTest, DuskRoseLab_NoUnitHereAtFinalization_NothingAskedOrKilledAtResolution) {
    auto lab = battlefieldCard(0, kDuskRoseLab);
    auto unit = addUnit(P1, kInvalidId, 2);   // in the base
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(lab, TriggerType::AtStartOfBeginning, &agent, [&](GameState& live) {
        live.battlefields[0].controller = P1;
        putOnBoard(live, unit, 0);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(unit)) << "the cost is not paid as it resolves";
    EXPECT_EQ(handSize(P1), 0);
}

// 532 Sunken Temple.
TEST_F(VerifyCardsTest, SunkenTemple_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto temple = battlefieldCard(0, kSunkenTemple);
    addUnit(P1, kInvalidId, 5, 0);            // conquered with a [Mighty] unit
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(temple, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(handSize(P1), 0);
}

// 949 Protective Sands.
TEST_F(VerifyCardsTest, ProtectiveSands_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto sands = battlefieldCard(0, kProtectiveSands);
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(sands, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(handSize(P1), 0);
}

// 774 Valley of Idols: the played unit is off the board at finalization and
// back on it by resolution.
TEST_F(VerifyCardsTest, ValleyOfIdols_UnitGoneAtFinalization_NothingAskedOrPaidAtResolution) {
    auto valley = battlefieldCard(0, kValleyOfIdols);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    sendToTrash(unit);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(valley, TriggerType::WhenYouPlayAUnit, &agent, [&](GameState& live) {
        putOnBoard(live, unit, 0);
    }, unit);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(o(unit).buff_count, 0);
}

// 72 Solari Shrine: exhausted at finalization, readied in response.
TEST_F(VerifyCardsTest, SolariShrine_ExhaustedAtFinalization_NothingAskedOrExhaustedAtResolution) {
    auto shrine = addGear(P1, kSolariShrine);
    o(shrine).is_exhausted = true;
    addToDeck(P1, kRecruit);
    auto dead = addUnit(P2, kInvalidId, 2, 0);
    sendToTrash(dead);
    o(dead).last_known.valid = true;          // a stunned enemy unit P1 killed
    o(dead).last_known.stunned = true;
    o(dead).last_known.killed_by = P1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(shrine, TriggerType::WhenAnEnemyUnitDies, &agent, [&](GameState& live) {
        live.getObject(shrine).is_exhausted = false;
    }, dead);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(shrine).is_exhausted) << "the cost is not paid as it resolves";
    EXPECT_EQ(handSize(P1), 0);
}

// 152 Mistfall.
TEST_F(VerifyCardsTest, Mistfall_ExhaustedAtFinalization_NothingAskedOrPaidAtResolution) {
    auto mistfall = addGear(P1, kMistfall);
    o(mistfall).is_exhausted = true;
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    o(unit).is_exhausted = true;
    power(P1, Domain::Body) = 1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(mistfall, TriggerType::WhenYouBuffAFriendlyUnit, &agent, [&](GameState& live) {
        live.getObject(mistfall).is_exhausted = false;
    }, unit);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(power(P1, Domain::Body), 1);
    EXPECT_FALSE(o(mistfall).is_exhausted);
    EXPECT_TRUE(o(unit).is_exhausted);
}

// 465 Spirit Wheel.
TEST_F(VerifyCardsTest, SpiritWheel_ExhaustedAtFinalization_NothingAskedOrPaidAtResolution) {
    auto wheel = addGear(P1, kSpiritWheel);
    o(wheel).is_exhausted = true;
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kRecruit);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(wheel, TriggerType::WhenYouChooseAFriendlyUnit, &agent, [&](GameState& live) {
        live.getObject(wheel).is_exhausted = false;
    }, unit);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_FALSE(o(wheel).is_exhausted);
    EXPECT_EQ(handSize(P1), 0);
}

// 671 Blood Rose.
TEST_F(VerifyCardsTest, BloodRose_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto rose = addGear(P1, kBloodRose);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(rose, TriggerType::WhenYouPlayAUnit, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    }, unit);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(ps(P1).xp, 0);
}

// 555 Blade Dancer, "When you conquer, you may pay [1] to ready me": ready at
// finalization, exhausted in response.
TEST_F(VerifyCardsTest, BladeDancer_ReadyAtFinalization_NothingAskedOrPaidAtResolution) {
    auto legend = addLegend(P1, kBladeDancer);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(legend, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        live.getObject(legend).is_exhausted = true;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_TRUE(o(legend).is_exhausted);
}

// 783 Piltover Enforcer: exhausted at finalization, readied in response.
TEST_F(VerifyCardsTest, PiltoverEnforcer_ExhaustedAtFinalization_NothingAskedOrExhaustedAtResolution) {
    auto legend = addLegend(P1, kPiltoverEnforcer);
    o(legend).is_exhausted = true;
    auto conquered = battlefieldCard(0, kDuskRoseLab);
    ps(P1).last_combat_bf = state.battlefields[0].id;
    ps(P1).excess_combat_damage = 3;
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    o(unit).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(legend, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        live.getObject(legend).is_exhausted = false;
    }, conquered);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(legend).is_exhausted) << "the cost is not paid as it resolves";
    EXPECT_TRUE(o(unit).is_exhausted);
}

// 6 Flame Chompers!: [R] unpayable at finalization.
TEST_F(VerifyCardsTest, FlameChompers_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto chompers = addToTrash(P1, kFlameChompers);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(chompers, TriggerType::WhenIAmDiscarded, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(power(P1, Domain::Fury), 1);
    EXPECT_TRUE(inTrash(P1, chompers));
}

// 37 Immortal Phoenix: the [R] of [1][R] unpayable at finalization.
TEST_F(VerifyCardsTest, ImmortalPhoenix_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto phoenix = addToTrash(P1, kImmortalPhoenix);
    ps(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(phoenix, TriggerType::WhenYouKillAUnitWithASpell, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(power(P1, Domain::Fury), 1);
    EXPECT_TRUE(inTrash(P1, phoenix));
}

// 35 Vayne, Hunter (the reprint 537 never had the early return).
TEST_F(VerifyCardsTest, Vayne035_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto vayne = addUnit(P1, kVayne035, 0, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(vayne, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_TRUE(onBoard(vayne));
}

// 194 Nocturne, Horrifying, seen anywhere but in a Main Deck: nothing is
// asked. (Testing phase D71, by design: the ability is a replacement applied
// as he is seen, so there is no finalization and resolution to differ —
// the old test checked a triggered ability not in the deck at finalization
// and back in it by resolution.)
TEST_F(VerifyCardsTest, Nocturne_SeenOutsideTheDeck_NothingAsked) {
    auto nocturne = addToHand(P1, kNocturneHorrifying);
    EnginePathAgent agent(EnginePathAgent::script({}));
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.seenOnTopOfDeck(P1, nocturne); },
                         &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inHand(P1, nocturne));
    EXPECT_FALSE(inBanishment(P1, nocturne));
}

// 641 Diana, Lunari.
TEST_F(VerifyCardsTest, DianaLunari_UnpayableAtFinalization_NothingAskedOrPaidAtResolution) {
    auto diana = addUnit(P1, kDianaLunari, 0, 0);
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(diana, TriggerType::WhenAShowdownBeginsHere, &agent, [&](GameState& live) {
        live.player(P1).rune_pool.energy = 1;
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).rune_pool.energy, 1);
    EXPECT_EQ(handSize(P1), 0);
}

// 955 Shadow Clone: no unit in the trash at finalization; one dies in response.
TEST_F(VerifyCardsTest, ShadowClone_NoUnitInTrashAtFinalization_NothingAskedOrBanishedAtResolution) {
    auto clone = addUnit(P1, kShadowClone, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(clone, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        removeFromBoard(live, other);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, other));
    EXPECT_FALSE(inBanishment(P1, other));
}

// 617 Vex, Mocking: already at that battlefield at finalization, moved away
// in response.
TEST_F(VerifyCardsTest, VexMocking_AlreadyThereAtFinalization_NothingAskedOrMovedAtResolution) {
    auto vex = addUnit(P1, kVexMocking, 0, 0);
    auto stunned = addUnit(P2, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(vex, TriggerType::WhenYouStun, &agent, [&](GameState& live) {
        putOnBoard(live, vex, -1);
    }, stunned);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(vex).isAtBase()) << "she is not moved back";
}

// ─── 9.EE2: nothing to choose at finalization is not asked at resolution ────

// 228 Vanguard Helm: no other friendly unit at finalization.
TEST_F(VerifyCardsTest, VanguardHelm_NoUnitAtFinalization_NothingAskedOrBuffedAtResolution) {
    auto helm = addGear(P1, kVanguardHelm);
    auto dead = addUnit(P1, kInvalidId, 2);
    o(dead).buff_count = 1;                   // "a buffed friendly unit dies"
    sendToTrash(dead);
    auto late = addUnit(P1, kInvalidId, 2);
    sendToTrash(late);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(helm, TriggerType::WhenAFriendlyUnitDies, &agent, [&](GameState& live) {
        putOnBoard(live, late);
    }, dead);
    EXPECT_TRUE(agent.prompts.empty()) << "targets are chosen at finalization (CR 383.3)";
    EXPECT_EQ(o(late).buff_count, 0);
}

// 165 Cemetery Attendant: no unit in the trash at finalization.
TEST_F(VerifyCardsTest, CemeteryAttendant_EmptyTrashAtFinalization_NothingAskedOrReturnedAtResolution) {
    auto attendant = addUnit(P1, kCemeteryAttendant, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(attendant, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        removeFromBoard(live, other);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, other));
    EXPECT_FALSE(inHand(P1, other));
}

// 308 Annie, Stubborn: no spell in the trash at finalization.
TEST_F(VerifyCardsTest, AnnieStubborn_NoSpellInTrashAtFinalization_NothingAskedOrReturnedAtResolution) {
    auto annie = addUnit(P1, kAnnieStubborn, 0, 0);
    auto spell = addToHand(P1, kFallingStar);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(annie, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        auto& p = live.player(P1);            // the spell is discarded in response
        p.hand.erase(std::remove(p.hand.begin(), p.hand.end(), spell), p.hand.end());
        live.getObject(spell).zone = ZoneType::Trash;
        p.trash.push_back(spell);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_FALSE(inHand(P1, spell));
}

// 225 Solari Chief: no enemy unit at finalization.
TEST_F(VerifyCardsTest, SolariChief_NoEnemyAtFinalization_NothingAskedOrStunnedAtResolution) {
    auto chief = addUnit(P1, kSolariChief, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 2);
    sendToTrash(enemy);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(chief, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        putOnBoard(live, enemy);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_FALSE(o(enemy).is_stunned);
}

// 729 Starhound: no Bird, Cat, Dog or Poro in the trash at finalization.
TEST_F(VerifyCardsTest, Starhound_NoneInTrashAtFinalization_NothingAskedOrReturnedAtResolution) {
    auto hound = addUnit(P1, kStarhound, 0, 0);
    auto poro = addUnit(P1, kInvalidId, 1);
    o(poro).tags = {"Poro"};
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(hound, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        removeFromBoard(live, poro);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, poro));
    EXPECT_FALSE(inHand(P1, poro));
}

// ─── 9.EE2: "here" with the source not at a battlefield at finalization ─────

// 41 Volibear, Furious.
TEST_F(VerifyCardsTest, Volibear_NotAtABattlefieldAtFinalization_NothingAskedOrDealtAtResolution) {
    auto volibear = addUnit(P1, kVolibearFurious);       // in the base
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireAndProbe(volibear, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        putOnBoard(live, volibear, 0);
    });
    EXPECT_TRUE(agent.prompts.empty()) << "split-damage targets are chosen at finalization";
    EXPECT_EQ(o(foe).damage_marked, 0);
}

// 351 Lucian, Gunslinger.
TEST_F(VerifyCardsTest, LucianGunslinger_NotAtABattlefieldAtFinalization_NothingAskedOrDealtAtResolution) {
    auto lucian = addUnit(P1, kLucianGunslinger);        // in the base
    ASSERT_GE(o(lucian).assault_value, 1) << "the printed [Assault]";
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(lucian, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        putOnBoard(live, lucian, 0);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(foe).damage_marked, 0);
}

// 404 Ezreal, Dashing.
TEST_F(VerifyCardsTest, EzrealDashing_NotAtABattlefieldAtFinalization_NothingAskedOrDealtAtResolution) {
    auto ezreal = addUnit(P1, kEzrealDashing);           // in the base
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(ezreal, TriggerType::WhenIAttackOrDefend, &agent, [&](GameState& live) {
        putOnBoard(live, ezreal, 0);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(foe).damage_marked, 0);
}

// 674 Irresistible Faefolk.
TEST_F(VerifyCardsTest, Faefolk_NotAtABattlefieldAtFinalization_NothingAskedOrMovedAtResolution) {
    auto faefolk = addUnit(P1, kFaefolk);                // in the base
    auto foe = addUnit(P2, kInvalidId, 2);               // in its base
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(faefolk, TriggerType::WhenIMoveToFB, &agent, [&](GameState& live) {
        putOnBoard(live, faefolk, 0);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(o(foe).isAtBase());
}

// 828 Riven, Shattered ("When I attack").
TEST_F(VerifyCardsTest, RivenShattered_NotAtABattlefieldAtFinalization_NothingAskedAtResolution) {
    auto riven = addUnit(P1, kRivenShattered);           // in the base
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(riven, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        putOnBoard(live, riven, 0);
    });
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(foe).damage_marked, 0);
}

// 875 Jayce, Hammer in Hand: off the board at finalization, back on it by
// resolution.
TEST_F(VerifyCardsTest, JayceHammerInHand_OffTheBoardAtFinalization_NothingAskedAtResolution) {
    auto jayce = addUnit(P1, kJayceHammerInHand, 0, 0);
    sendToTrash(jayce);
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireNotPerformed(jayce, TriggerType::WhenIBecomeReady, &agent, [&](GameState& live) {
        putOnBoard(live, jayce);
    });
    EXPECT_TRUE(agent.prompts.empty()) << "the keyword is chosen at finalization or not at all";
}

// ─── 9.EE2: a trigger condition is asked when the event happens ─────────────
// (CR 383.2.a.1). On the new code no ability goes on the chain, so the probe
// never runs; on the old code it went on the chain, returned early in its
// finalize pass and asked as it resolved.

// 573 Fresh Beans: "When you play a unit during a showdown".
TEST_F(VerifyCardsTest, FreshBeans_NoShowdownWhenTheUnitIsPlayed_NoAbilityAndNothingAsked) {
    auto beans = addGear(P1, kFreshBeans);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({}));
    const bool queued = fireAndProbeIfQueued(beans, TriggerType::WhenYouPlayAUnit, &agent,
        [&](GameState& live) { live.battlefields[0].showdown_in_progress = true; }, unit);
    EXPECT_FALSE(queued) << "\"during a showdown\" is part of the trigger condition";
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_FALSE(o(beans).is_exhausted);
    EXPECT_EQ(handSize(P1), 0);
}

// 562 Mischievous Marai: "When you play me to a battlefield".
TEST_F(VerifyCardsTest, MischievousMarai_PlayedToBase_NoAbilityAndNothingAsked) {
    auto marai = addUnit(P1, kMischievousMarai);         // in the base
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    const bool queued = fireAndProbeIfQueued(marai, TriggerType::WhenYouPlayMe, &agent,
        [&](GameState& live) { putOnBoard(live, marai, 0); });
    EXPECT_FALSE(queued) << "\"to a battlefield\" is part of the trigger condition";
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(foe).damage_marked, 0);
}

// 824 Tomb-Raider Barbara: "if you control 7 or more runes".
TEST_F(VerifyCardsTest, TombRaiderBarbara_SixRunesWhenPlayed_NoAbilityAndNothingAsked) {
    auto barbara = addUnit(P1, kTombRaiderBarbara, 0, 0);
    for (int i = 0; i < 6; ++i) addRune(P1, Domain::Fury);
    auto seventh = addRune(P2, Domain::Fury);
    auto gear = addGear(P2, kInvalidId);
    EnginePathAgent agent(EnginePathAgent::script({}));
    const bool queued = fireAndProbeIfQueued(barbara, TriggerType::WhenYouPlayMe, &agent,
        [&](GameState& live) { live.getObject(seventh).controller = P1; });
    EXPECT_FALSE(queued) << "the intervening \"if\" is part of the trigger condition";
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(gear));
}

// 923 Ambessa: "[Empowered][>] When I attack" — not Empowered as she attacks.
TEST_F(VerifyCardsTest, Ambessa_NotEmpoweredWhenSheAttacks_NoAbilityAndNothingAsked) {
    auto ambessa = addUnit(P1, kAmbessaRespectedAndFeared, 0, 0);
    auto foe = addUnit(P2, kInvalidId, 1, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    const bool queued = fireAndProbeIfQueued(ambessa, TriggerType::WhenIAttack, &agent,
        [&](GameState& live) { live.getObject(ambessa).empowered = 1; });
    EXPECT_FALSE(queued) << "she has the ability only while Empowered (CR 828.1.b.1)";
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(foe));
}

// 866 Dame the Despoiler: "[Empowered][>] When I attack or defend".
TEST_F(VerifyCardsTest, DameTheDespoiler_NotEmpoweredWhenSheAttacks_NoAbilityAndNothingAsked) {
    auto dame = addUnit(P1, kDameTheDespoiler, 0, 0);
    addUnit(P2, kInvalidId, 9, 0);
    EnginePathAgent agent(EnginePathAgent::script({}));
    const bool queued = fireAndProbeIfQueued(dame, TriggerType::WhenIAttackOrDefend, &agent,
        [&](GameState& live) { live.getObject(dame).empowered = 1; });
    EXPECT_FALSE(queued) << "she has the ability only while Empowered (CR 828.1.b.1)";
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(o(dame).temp_might_bonus, 0);
}

// ─── 9.EE2, branch form: the helper asked is fixed at finalization ──────────

// 936 Defender of Tomorrow: not Empowered when the ability is activated (one
// gear is chosen), Empowered in response. Fails on the old code: as it
// resolved it took the Empowered branch and asked "which ability" then.
TEST_F(VerifyCardsTest, DefenderOfTomorrow_EmpoweredInResponse_AsksNothingMoreAtResolution) {
    auto legend = addLegend(P1, kDefenderOfTomorrow);
    auto g1 = addGear(P1, kInvalidId);
    auto g2 = addGear(P1, kInvalidId);
    o(g1).is_exhausted = true;
    o(g2).is_exhausted = true;
    ps(P1).rune_pool.energy = 1;
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = legend;
    act.ability_index = 0;
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(g2.raw())}));
    size_t asked_at_response = 99;
    executeAndProbe(act, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        live.getObject(legend).empowered = 1;
    });
    EXPECT_EQ(asked_at_response, 1u) << "the gear is chosen as the ability is activated";
    EXPECT_EQ(agent.prompts.size(), 1u) << "nothing is asked as it resolves";
    EXPECT_FALSE(o(g2).is_exhausted) << "the gear chosen at activation is readied";
    EXPECT_TRUE(o(g1).is_exhausted);
}

// 372 Aphelios, Exalted: "Buff a friendly unit" chosen with no friendly unit
// on the board; one arrives in response. Fails on the old code: the unit was
// asked for as the ability resolved.
TEST_F(VerifyCardsTest, Aphelios_BuffModeWithNoUnitAtFinalization_NothingAskedOrBuffedAtResolution) {
    auto aphelios = addUnit(P1, kApheliosExalted, 0, 0);
    sendToTrash(aphelios);                    // he died before the ability was finalized
    auto late = addUnit(P1, kInvalidId, 2);
    sendToTrash(late);
    EnginePathAgent agent(EnginePathAgent::script({2}));   // "Buff a friendly unit"
    fireNotPerformed(aphelios, TriggerType::WhenEquipmentAttachedToMe, &agent, [&](GameState& live) {
        putOnBoard(live, late);
    });
    EXPECT_EQ(agent.prompts.size(), 1u) << "the mode only; no unit could be chosen";
    EXPECT_EQ(o(late).buff_count, 0);
}

// ═══ Tranche 12: 9.GG3 ═══════════════════════════════════════════════════════
// Already in the file and used below: kElderDragon (680), kLongSword (345),
// kRecruit (954), kFreshBeans (573), kMischievousMarai (562),
// kTombRaiderBarbara (824), kDameTheDespoiler (866),
// kAmbessaRespectedAndFeared (923).
// No new fixture helper.

// ═══ 9.GG3 ══════════════════════════════════════════════════════════════════

// ─── part 1: a battlefield added while the ability is on the chain ──────────

// 680 Elder Dragon: "choose up to one enemy unit at each location" — the
// locations there are as the ability is finalized. Fails on the old code: the
// per-location loop ran over state.battlefields as it was at resolution, so
// the new battlefield (with an enemy unit) got a prompt then.
TEST_F(VerifyCardsTest, ElderDragon_ABattlefieldAddedInResponse_IsNotAskedAboutAtResolution) {
    auto dragon = addUnit(P1, kElderDragon, 0, 0);
    auto at_base = addUnit(P2, kInvalidId, 5);
    auto late = addUnit(P2, kInvalidId, 5);
    sendToTrash(late);                                  // not on the board yet
    // The opponent's base is the only location with an enemy unit: one prompt.
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(at_base.raw())}));
    size_t asked_at_response = 99;
    fireAndProbe(dragon, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        BattlefieldState pit;                           // as Baron Nashor's Pit arrives
        pit.id = BattlefieldId::fromRaw(live.battlefields.size());
        live.battlefields.push_back(pit);
        putOnBoard(live, late, static_cast<int>(pit.id.raw()));
    });
    ASSERT_EQ(state.battlefields.size(), 3u);
    ASSERT_EQ(bfOf(late), std::optional<BattlefieldId>{BattlefieldId::fromRaw(2)});
    EXPECT_EQ(asked_at_response, 1u) << "one location had an enemy unit at finalization";
    EXPECT_EQ(agent.prompts.size(), 1u) << "nothing is asked as it resolves";
    EXPECT_FALSE(agent.wasOffered(late));
    EXPECT_TRUE(!onBoard(at_base) || o(at_base).damage_marked >= 1) << "\"Deal 1 to them\"";
    EXPECT_EQ(o(late).damage_marked, 0) << "no unit was chosen at the new battlefield";
}

// 667 Imposing Challenger: with one other battlefield there is no "which
// battlefield" to ask. Fails on the old code: with a second other battlefield
// by the time it resolved, the destination was asked for then.
TEST_F(VerifyCardsTest, ImposingChallenger_ABattlefieldAddedInResponse_NoDestinationIsAskedAtResolution) {
    auto imp = addUnit(P1, kImposingChallenger, 0, 0);
    auto foe = addUnit(P2, kInvalidId, 2, 0);
    // Yes; the enemy unit. Battlefield 1 is the only "different battlefield".
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(foe.raw())}));
    size_t asked_at_response = 99;
    fireAndProbe(imp, TriggerType::WhenIMove, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        BattlefieldState pit;
        pit.id = BattlefieldId::fromRaw(live.battlefields.size());
        live.battlefields.push_back(pit);
    });
    ASSERT_EQ(state.battlefields.size(), 3u);
    EXPECT_EQ(asked_at_response, 2u) << "the may and the unit, at finalization";
    EXPECT_EQ(agent.prompts.size(), 2u) << "nothing is asked as it resolves";
    EXPECT_EQ(bfOf(foe), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)})
        << "moved to the battlefield that was the only other one at finalization";
}

// ─── part 2: Veiled Temple 535 — the second "you may" (CR 383.3.a.3) ────────

// "When you conquer here, you may ready a friendly gear. If it's an Equipment,
// you may detach it." The first may and the gear at finalization, the detach
// on resolution. Fails on the old code: all three prompts came before the
// opponent could respond.
TEST_F(VerifyCardsTest, VeiledTemple_DetachIsDecidedOnResolutionNotAtFinalization) {
    EXPECT_EQ(card_db.get(kVeiledTemple).ability_text,
              "When you conquer here, you may ready a friendly gear. If it's an Equipment, "
              "you may detach it.");
    auto temple = battlefieldCard(0, kVeiledTemple);
    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    attach(sword, unit);
    o(sword).is_exhausted = true;
    // Yes; the sword; "Detach it" (mode 1).
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(sword.raw()), 1}));
    size_t asked_at_response = 99;
    bool exhausted_at_response = false, attached_at_response = false;
    fireAndProbe(temple, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        asked_at_response = agent.prompts.size();
        exhausted_at_response = live.getObject(sword).is_exhausted;
        attached_at_response = live.getObject(sword).attached_to.has_value();
    });
    EXPECT_EQ(asked_at_response, 2u)
        << "the opening may and the gear at finalization; the later may on resolution";
    EXPECT_TRUE(exhausted_at_response);
    EXPECT_TRUE(attached_at_response);
    ASSERT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(promptOffersValue(agent, 2, 0)) << "leave it attached";
    EXPECT_TRUE(promptOffersValue(agent, 2, 1)) << "detach it";
    EXPECT_FALSE(o(sword).is_exhausted);
    EXPECT_FALSE(o(sword).attached_to.has_value());
    EXPECT_TRUE(o(unit).attachments.empty());
}

// The Equipment comes off in response: as the ability resolves there is
// nothing to detach, so nothing is asked. Fails on the old code: the detach
// had been asked (and answered) at finalization — a third prompt.
TEST_F(VerifyCardsTest, VeiledTemple_EquipmentDetachedInResponse_DetachIsNotAsked) {
    auto temple = battlefieldCard(0, kVeiledTemple);
    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = addGear(P1, kLongSword);
    attach(sword, unit);
    o(sword).is_exhausted = true;
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(sword.raw())}));
    fireAndProbe(temple, TriggerType::WhenYouConquerHere, &agent, [&](GameState& live) {
        auto& g = live.getObject(sword);
        auto& u = live.getObject(unit);
        u.attachments.erase(std::remove(u.attachments.begin(), u.attachments.end(), sword),
                            u.attachments.end());
        u.attachment_might_bonus -= g.might_bonus;
        u.recomputeMight();
        g.attached_to = std::nullopt;
    });
    EXPECT_EQ(agent.prompts.size(), 2u) << "the may and the gear; no detach question";
    EXPECT_FALSE(o(sword).is_exhausted) << "the gear is still readied";
}

// ─── part 4: the trigger condition is asked once, when the event happens ────
// (CR 383.2.a.1; the Sona example: "If she is removed in reaction to the
// triggered ability, it will still resolve.")

// 824 Tomb-Raider Barbara. Fails on the old code: with six runes at resolution
// the gear list was empty, so the gear chosen at finalization was a mistarget.
TEST_F(VerifyCardsTest, TombRaiderBarbara_ARuneLeavesInResponse_TheChosenGearIsStillKilled) {
    auto barbara = addUnit(P1, kTombRaiderBarbara, 0, 0);
    GameObjectId last_rune = kInvalidId;
    for (int i = 0; i < 7; ++i) last_rune = addRune(P1, Domain::Fury);
    auto gear = addGear(P2, kInvalidId);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(gear.raw())}));
    fireAndProbe(barbara, TriggerType::WhenYouPlayMe, &agent, [&](GameState& live) {
        removeFromBoard(live, last_rune);               // six runes as it resolves
    });
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(onBoard(gear)) << "the ability was on the chain: it still resolves";
}

// 923 Ambessa. Fails on the old code: disempowered at resolution the unit list
// was empty, so the unit chosen at finalization was a mistarget.
TEST_F(VerifyCardsTest, Ambessa_DisempoweredInResponse_TheChosenUnitIsStillKilled) {
    auto ambessa = addUnit(P1, kAmbessaRespectedAndFeared, 0, 0);
    o(ambessa).empowered = 1;
    auto foe = addUnit(P2, kInvalidId, 1, 0);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(foe.raw())}));
    fireAndProbe(ambessa, TriggerType::WhenIAttack, &agent, [&](GameState& live) {
        live.getObject(ambessa).empowered = 0;
    });
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_FALSE(onBoard(foe)) << "the ability was on the chain: it still resolves";
}

// 866 Dame the Despoiler. Fails on the old code for the same reason: no Might.
TEST_F(VerifyCardsTest, DameTheDespoiler_DisempoweredInResponse_HerMightIsStillRaised) {
    auto dame = addUnit(P1, kDameTheDespoiler, 0, 0);
    o(dame).empowered = 1;
    auto giant = addUnit(P2, kInvalidId, 9, 0);
    const int before = o(dame).current_might;
    ASSERT_LT(before, 9);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(giant.raw())}));
    fireAndProbe(dame, TriggerType::WhenIAttackOrDefend, &agent, [&](GameState& live) {
        live.getObject(dame).empowered = 0;
    });
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(o(dame).temp_might_bonus, 9 - before + 1)
        << "raised to the chosen unit's Might, then +1";
}

// 573 Fresh Beans. The condition held when the unit was played and no longer
// holds when the ability is finalized. Fails on the old code: the may's
// precondition asked "during a showdown" again, so the may was not offered.
// (Not a state a game reaches by itself — nothing happens between a trigger
// and its finalization — but it is the only place the second ask could be
// seen: once the may is accepted the cost is paid, and a paid may is not
// re-validated at resolution.)
TEST_F(VerifyCardsTest, FreshBeans_TheShowdownIsAskedAboutOnlyWhenTheUnitIsPlayed) {
    auto beans = addGear(P1, kFreshBeans);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script({1}));
    withEngine([&](GameEngine& e) {
        auto& live = e.mutableState();
        live.battlefields[0].showdown_in_progress = true;
        e.testHook_executor().queueTriggeredAbility(beans, P1, TriggerType::WhenYouPlayAUnit, unit);
        live.battlefields[0].showdown_in_progress = false;
        e.testHook_runChain();
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the ability triggered: its may is asked";
    EXPECT_TRUE(o(beans).is_exhausted);
    EXPECT_EQ(handSize(P1), 1);
}

// 573 Fresh Beans: the showdown ends in response. A pin, not a red test: the
// old code drew here too (the cost was paid at finalization, and a paid may is
// not re-validated).
TEST_F(VerifyCardsTest, Pin_FreshBeans_TheShowdownEndsInResponse_TheCardIsStillDrawn) {
    auto beans = addGear(P1, kFreshBeans);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    addToDeck(P1, kRecruit);
    state.battlefields[0].showdown_in_progress = true;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    bool exhausted_at_response = false;
    fireAndProbe(beans, TriggerType::WhenYouPlayAUnit, &agent, [&](GameState& live) {
        exhausted_at_response = live.getObject(beans).is_exhausted;
        live.battlefields[0].showdown_in_progress = false;
    }, unit);
    EXPECT_TRUE(exhausted_at_response) << "exhausted as the ability is finalized";
    EXPECT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(handSize(P1), 1);
}

// 562 Mischievous Marai — NOT A DEFECT: the battlefield test left in onTrigger
// is not the trigger condition asked again but what "here" refers to, which is
// checked as the instruction is executed (CR 359.3.f.2, the Yasuo example:
// moved back to base in response, "the attack trigger mistargets").
TEST_F(VerifyCardsTest, Pin_MischievousMarai_MovedToBaseInResponse_HereIsGoneAndItMistargets) {
    auto marai = addUnit(P1, kMischievousMarai, 0, 0);
    auto foe = addUnit(P2, kInvalidId, 9, 0);
    // She stays: 2 to the chosen unit.
    EnginePathAgent stays(EnginePathAgent::script({static_cast<int>(foe.raw())}));
    fireAndProbe(marai, TriggerType::WhenYouPlayMe, &stays, [](GameState&) {});
    ASSERT_EQ(stays.prompts.size(), 1u);
    EXPECT_EQ(o(foe).damage_marked, 2);
    // She is moved to her base in response: the unit is no longer "here".
    EnginePathAgent moved(EnginePathAgent::script({static_cast<int>(foe.raw())}));
    fireAndProbe(marai, TriggerType::WhenYouPlayMe, &moved, [&](GameState& live) {
        putOnBoard(live, marai);                        // -1: her controller's base
    });
    EXPECT_EQ(moved.prompts.size(), 1u) << "nothing is asked as it resolves";
    EXPECT_EQ(o(foe).damage_marked, 2) << "no further damage: a mistarget";
}

// ═══ Tranche 14: 9.HH5 — selectSeveral itself, through a test card ═══════════
// Stored picks from the finalize pass; one of them is no longer in the list
// when the ability resolves. The helper must hand back only the others and
// ask nothing again. (The six card-level pins read this off trace lines.)
TEST_F(VerifyCardsTest, SelectSeveral_AStoredPickNoLongerInTheListIsDroppedAndNothingIsAskedAgain) {
    card_registry.registerCard(kSelectSeveralProbe, std::make_unique<SelectSeveralProbeCard>());
    card_db.buildFromClasses(card_registry);
    auto probe = addUnit(P1, kSelectSeveralProbe, 1, 0);
    auto a = addUnit(P1, kShipyardSkulker, 0, 0);
    auto b = addUnit(P1, kShipyardSkulker, 0, 0);
    auto c = addUnit(P1, kShipyardSkulker, 0, 0);
    GameObjectId late = kInvalidId;
    size_t prompts_at_priority = 0;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(a.raw()), static_cast<int>(b.raw()), static_cast<int>(c.raw())}));
    fireAndProbe(probe, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        prompts_at_priority = agent.prompts.size();
        EXPECT_EQ(live.getObject(probe).card_counters.count("__probe_count"), 0u)
            << "nothing has resolved yet";
        removeFromBoard(live, b);               // a stored pick leaves the list
        // ...and a new candidate appears: it must not be asked about either.
        late = live.createObject();
        auto& u = live.getObject(late);
        u.owner = u.controller = P1;
        u.card_type = CardType::Unit;
        u.name = "Latecomer";
        u.base_might = u.current_might = 1;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{0};
    });
    EXPECT_EQ(prompts_at_priority, 3u) << "all three picks were made at finalization";
    EXPECT_EQ(agent.prompts.size(), 3u) << "nothing is asked again as it resolves";
    const auto& counters = o(probe).card_counters;
    ASSERT_EQ(counters.count("__probe_count"), 1u) << "the ability resolved";
    EXPECT_EQ(counters.at("__probe_count"), 2) << "the stale pick is dropped, not kept";
    ASSERT_EQ(counters.count("__probe_1"), 1u);
    EXPECT_EQ(counters.at("__probe_0"), static_cast<int>(a.raw()));
    EXPECT_EQ(counters.at("__probe_1"), static_cast<int>(c.raw())) << "pick order kept, no gap";
    EXPECT_EQ(counters.count("__probe_2"), 0u);
}

// ═══ Tranche 15: 9.HH6 — selectSeveral with one object listed several times ══
// X has two buffs and Y one, so the list is {X, X, Y}; all three are picked at
// finalization. In response X loses one buff: the list at resolution is
// {X, Y}. selectSeveral must return {X, Y}: each stored pick uses up ONE
// instance of its object in the current list, so the first stored X counts,
// the second finds no instance left and is dropped, and Y counts. Returning X
// twice would let a caller spend a buff that is no longer there; returning
// {Y} alone would throw away a pick that is still good.
TEST_F(VerifyCardsTest, SelectSeveral_AnObjectListedTwiceWithOneInstanceGoneCountsOnce) {
    card_registry.registerCard(kSelectSeveralDupProbe,
                               std::make_unique<SelectSeveralDupProbeCard>());
    card_db.buildFromClasses(card_registry);
    auto probe = addUnit(P1, kSelectSeveralDupProbe, 1, 0);
    auto x = addUnit(P1, kShipyardSkulker, 0, 0);
    auto y = addUnit(P1, kShipyardSkulker, 0, 0);
    o(x).buff_count = 2;
    o(y).buff_count = 1;
    size_t prompts_at_priority = 0;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(x.raw()), static_cast<int>(x.raw()), static_cast<int>(y.raw())}));
    fireAndProbe(probe, TriggerType::WhenIConquer, &agent, [&](GameState& live) {
        prompts_at_priority = agent.prompts.size();
        live.getObject(x).buff_count = 1;       // one of X's two buffs is gone
    });
    EXPECT_EQ(prompts_at_priority, 3u) << "X, X and Y were picked at finalization";
    // While choosing, an object with two instances left is offered once per
    // prompt, and still offered after one of them is taken.
    ASSERT_GE(agent.prompts.size(), 2u);
    EXPECT_TRUE(agent.wasOffered(x));
    EXPECT_EQ(agent.prompts.size(), 3u) << "nothing is asked again as it resolves";
    const auto& counters = o(probe).card_counters;
    ASSERT_EQ(counters.count("__probe_count"), 1u) << "the ability resolved";
    EXPECT_EQ(counters.at("__probe_count"), 2) << "X once, Y once";
    ASSERT_EQ(counters.count("__probe_1"), 1u);
    EXPECT_EQ(counters.at("__probe_0"), static_cast<int>(x.raw()));
    EXPECT_EQ(counters.at("__probe_1"), static_cast<int>(y.raw()));
    EXPECT_EQ(counters.count("__probe_2"), 0u);
}

// The control: with both of X's buffs still there, all three stored picks
// come back, X twice.
TEST_F(VerifyCardsTest, SelectSeveral_AnObjectListedTwiceIsReturnedTwiceWhenBothInstancesRemain) {
    card_registry.registerCard(kSelectSeveralDupProbe,
                               std::make_unique<SelectSeveralDupProbeCard>());
    card_db.buildFromClasses(card_registry);
    auto probe = addUnit(P1, kSelectSeveralDupProbe, 1, 0);
    auto x = addUnit(P1, kShipyardSkulker, 0, 0);
    auto y = addUnit(P1, kShipyardSkulker, 0, 0);
    o(x).buff_count = 2;
    o(y).buff_count = 1;
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(x.raw()), static_cast<int>(x.raw()), static_cast<int>(y.raw())}));
    fireAndProbe(probe, TriggerType::WhenIConquer, &agent, [](GameState&) {});
    EXPECT_EQ(agent.prompts.size(), 3u);
    const auto& counters = o(probe).card_counters;
    ASSERT_EQ(counters.count("__probe_count"), 1u);
    EXPECT_EQ(counters.at("__probe_count"), 3);
    EXPECT_EQ(counters.at("__probe_0"), static_cast<int>(x.raw()));
    EXPECT_EQ(counters.at("__probe_1"), static_cast<int>(x.raw()));
    EXPECT_EQ(counters.at("__probe_2"), static_cast<int>(y.raw()));
}


// ═══ Judge rulings 2026-10-11 ═══════════════════════════════════════════════

// ─── 9.Z4: a hidden spell with "any number of" / "up to N" and nothing to
// choose at its battlefield is playable from facedown choosing none ─────────
// (CR 355.13 wins over 811.1.d's "no valid targets".) The rule is applied in
// one place, GameEngine::generateHiddenPlayActions, to every spell whose
// requirement is optional. The [Hidden] spells with that wording are three:
// Emperor's Divide 366 (was refused: the test above and this one), Fox-Fire
// 253 and Guerilla Warfare 260 (were already offered: pinned here).
struct HiddenSpellSetup {
    static GameObjectId hide(VerifyCardsTest& t, GameState& state, GameObjectId spell, int bf) {
        (void)t;
        auto& card = state.getObject(spell);
        auto& h = state.player(card.owner).hand;
        h.erase(std::remove(h.begin(), h.end(), spell), h.end());
        card.zone = ZoneType::BattlefieldZone;
        card.is_hidden = true;
        card.hidden_at = BattlefieldId::fromRaw(bf);
        card.hidden_on_turn = 2;
        state.battlefields[bf].facedown.push_back(spell);
        return spell;
    }
};

TEST_F(VerifyCardsTest, EmperorsDivide_PlayedFromFacedownChoosingNone_MovesNothing) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    auto spell = HiddenSpellSetup::hide(*this, state, addToHand(P1, kEmperorsDivide), 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    auto enemy_here = addUnit(P2, kInvalidId, 2, 0);
    std::vector<Intent> plays;
    EnginePathAgent agent(EnginePathAgent::script({}));
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) {
        e.mutableState().turn.oc_state = OpenClosedState::Closed;
        e.mutableState().turn.priority_holder = P1;
        for (const auto& i : e.generateLegalActions())
            if (i.card == spell) plays.push_back(i);
        e.mutableState().turn.oc_state = OpenClosedState::Open;
        e.mutableState().turn.priority_holder = std::nullopt;
        ASSERT_EQ(plays.size(), 1u);
        ASSERT_TRUE(plays[0].targets.empty());
        e.testHook_executeIntent(plays[0]);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to choose";
    EXPECT_TRUE(inTrash(P1, spell)) << "it was played and resolved";
    EXPECT_EQ(bfOf(elsewhere), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)}) << "nothing moved";
    EXPECT_EQ(bfOf(enemy_here), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(VerifyCardsTest, Pin_FoxFire_PlayedFromFacedownWithNoUnitAtItsBattlefield_KillsNothing) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    auto spell = HiddenSpellSetup::hide(*this, state, addToHand(P1, kFoxFire), 0);
    auto elsewhere = addUnit(P2, kInvalidId, 1, 1);
    std::vector<Intent> plays;
    EnginePathAgent agent(EnginePathAgent::script({EnginePathAgent::kRest}));
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) {
        e.mutableState().turn.oc_state = OpenClosedState::Closed;
        e.mutableState().turn.priority_holder = P1;
        for (const auto& i : e.generateLegalActions())
            if (i.card == spell) plays.push_back(i);
        e.mutableState().turn.oc_state = OpenClosedState::Open;
        e.mutableState().turn.priority_holder = std::nullopt;
        ASSERT_EQ(plays.size(), 1u) << "\"any number of units\" includes none";
        e.testHook_executeIntent(plays[0]);
        e.testHook_runChain();
    }, &agent);
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "tied to its battlefield (CR 811.1.d.2)";
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_TRUE(o(elsewhere).location.has_value()) << "nothing was killed";
}

TEST_F(VerifyCardsTest, Pin_GuerillaWarfare_PlayedFromFacedownWithNoHiddenCardInTheTrash) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    auto spell = HiddenSpellSetup::hide(*this, state, addToHand(P1, kGuerillaWarfare), 0);
    std::vector<Intent> plays;
    EnginePathAgent agent(EnginePathAgent::script({}));
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) {
        e.mutableState().turn.oc_state = OpenClosedState::Closed;
        e.mutableState().turn.priority_holder = P1;
        for (const auto& i : e.generateLegalActions())
            if (i.card == spell) plays.push_back(i);
        e.mutableState().turn.oc_state = OpenClosedState::Open;
        e.mutableState().turn.priority_holder = std::nullopt;
        ASSERT_EQ(plays.size(), 1u) << "\"up to two cards\" includes none";
        e.testHook_executeIntent(plays[0]);
        e.testHook_runChain();
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(ps(P1).hide_ignoring_costs_this_turn) << "the second sentence stands alone";
}

// ─── 9.R5: the Rune Deck is "a deck" for Void Hatchling 341 ─────────────────
// Twisted Fate, Gambler 200: "When I attack, reveal the top rune of your rune
// deck, then recycle it. Do one of the following based on its domain". With a
// Void Hatchling its controller looks at the top rune first and may recycle
// it; the rune revealed is the one on top after that. (Fails on the old code:
// no look — the Order rune on top was revealed and the first prompt was the
// stun.) Twisted Fate is the only card that reveals from a Rune Deck.
TEST_F(VerifyCardsTest, TwistedFateGambler_VoidHatchlingLooksAtTheTopRuneFirstAndMayRecycleIt) {
    withVoidHatchling();
    auto tf = addUnit(P1, kTwistedFateGambler, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto drawn = addToDeck(P1, kShipyardSkulker);
    auto below = addRuneToRuneDeck(P1, Domain::Mind);     // second from the top
    auto top = addRuneToRuneDeck(P1, Domain::Order);      // the top rune
    ASSERT_EQ(ps(P1).rune_deck.back(), top);
    EnginePathAgent agent(EnginePathAgent::script({1}));   // the look: recycle it
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "the look; [B] Draw 1 asks nothing";
    EXPECT_TRUE(askedLabel(agent, "Void Hatchling"));
    EXPECT_TRUE(inHand(P1, drawn)) << "the Mind rune below was the one revealed: Draw 1";
    EXPECT_FALSE(o(enemy).is_stunned) << "the Order rune was recycled at the look, not revealed";
    ASSERT_EQ(ps(P1).rune_deck.size(), 2u);
    EXPECT_EQ(ps(P1).rune_deck[1], top) << "recycled at the look: under the deck";
    EXPECT_EQ(ps(P1).rune_deck[0], below) << "revealed, then recycled: the bottom";
}

TEST_F(VerifyCardsTest, TwistedFateGambler_VoidHatchlingMayLeaveTheTopRune) {
    withVoidHatchling();
    auto tf = addUnit(P1, kTwistedFateGambler, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 5, 0);
    auto drawn = addToDeck(P1, kShipyardSkulker);
    addRuneToRuneDeck(P1, Domain::Mind);
    auto top = addRuneToRuneDeck(P1, Domain::Order);
    // The look: leave it; then [Y] "Stun an enemy unit".
    EnginePathAgent agent(EnginePathAgent::script({0, static_cast<int>(enemy.raw())}));
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &agent);
    ASSERT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(o(enemy).is_stunned);
    EXPECT_FALSE(inHand(P1, drawn));
    EXPECT_EQ(ps(P1).rune_deck.front(), top) << "revealed, then recycled";
}

// Without a Hatchling nothing is asked before the reveal.
TEST_F(VerifyCardsTest, Pin_TwistedFateGambler_NoVoidHatchling_NoLook) {
    auto tf = addUnit(P1, kTwistedFateGambler, 0, 0);
    auto drawn = addToDeck(P1, kShipyardSkulker);
    addRuneToRuneDeck(P1, Domain::Order);
    addRuneToRuneDeck(P1, Domain::Mind);                  // the top rune
    EnginePathAgent agent(EnginePathAgent::script({}));
    fireTriggerThroughEngine(tf, TriggerType::WhenIAttack, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inHand(P1, drawn));
}

}  // namespace
}  // namespace test_review_fix_verify_cards
}  // namespace riftbound::test
