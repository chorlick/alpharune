/// A spell that makes its choices before its cost is paid, and chooses a unit
/// with [Deflect] (plans/09 item 9.VV1).
///
/// Deflect's "[A] more" is paid when the unit is chosen, ahead of the spell's
/// own cost. It was paid by recycling whichever rune came first: a ready rune
/// was recycled without being exhausted, so its Energy was lost, and a rune of
/// the spell's own Domain could go although another would have done. The
/// spell was then offered, chosen for, and could not be paid:
/// "GameEngine::executePlaySpell: Switcheroo made choices it cannot be paid
/// for with" ended real games (vex vs vex, seed 20261016).

#include "cards/card_test_fixture.h"

namespace riftbound::test::deflect_before_cost {

constexpr CardDefId kSwitcheroo = 466;   // [2] + 2 Chaos: "Swap the Might of two units at the same battlefield this turn."

class DeflectBeforeCost : public CardTestFixture {
protected:
    GameObjectId deflector(int at_bf, int deflect) {
        auto id = addUnit(P2, kInvalidId, 2, at_bf);
        auto& u = state.getObject(id);
        u.keywords.set(Keyword::Deflect);
        u.deflect_value = deflect;
        return id;
    }
    std::optional<Intent> offer(GameObjectId card) {
        std::optional<Intent> found;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.card == card) found = i;
        });
        return found;
    }
    int runesInBase(PlayerId p) {
        int n = 0;
        for (const auto& [id, o] : state.objects)
            if (o.isRune() && o.controller == p && o.location.has_value()) ++n;
        return n;
    }
};

TEST_F(DeflectBeforeCost, TheRuneRecycledForDeflectIsNotOneTheSpellNeeds) {
    // The game's situation: the two Chaos runes the spell's Power needs come
    // first, a third rune of another Domain is there for the Deflect.
    auto spell = addToHand(P1, kSwitcheroo);
    addRune(P1, Domain::Chaos);
    addRune(P1, Domain::Chaos);
    addRune(P1, Domain::Body);
    auto mine = addUnit(P1, kInvalidId, 5, 0);
    auto theirs = deflector(0, 1);
    ASSERT_TRUE(offer(spell).has_value()) << "2 Energy, 2 Chaos and 1 more Power are all there";

    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(theirs.raw()), static_cast<int>(mine.raw()), EnginePathAgent::kRest}));
    ASSERT_NO_THROW(playThroughEngine(spell, &agent, std::nullopt, nullptr, /*fund=*/false));
    EXPECT_TRUE(inTrash(P1, spell)) << "the spell was paid for and resolved";
    EXPECT_EQ(state.getObject(mine).current_might, 2) << "Might swapped";
    EXPECT_EQ(state.getObject(theirs).current_might, 5);
    EXPECT_EQ(runesInBase(P1), 0) << "all three runes were recycled: 2 Chaos, 1 for Deflect";
}

TEST_F(DeflectBeforeCost, AReadyRuneRecycledForDeflectStillGivesItsEnergy) {
    // Three Energy for the spell, three runes: each must be exhausted for
    // Energy before it is recycled, also the one that pays the Deflect.
    auto spell = addToHand(P1, kSwitcheroo);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Chaos);
    state.player(P1).cost_modifiers.push_back({});
    state.player(P1).cost_modifiers.back().energy_increase = 1;   // [3] + 2 Chaos
    auto mine = addUnit(P1, kInvalidId, 5, 0);
    auto theirs = deflector(0, 1);
    ASSERT_TRUE(offer(spell).has_value());

    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(theirs.raw()), static_cast<int>(mine.raw()), EnginePathAgent::kRest}));
    ASSERT_NO_THROW(playThroughEngine(spell, &agent, std::nullopt, nullptr, /*fund=*/false));
    EXPECT_TRUE(inTrash(P1, spell));
    EXPECT_EQ(state.getObject(theirs).current_might, 5);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "no Energy is left floating";
}

// ── Every spell of the same shape ───────────────────────────────────────────
//
// Why the registry-level tests did not see it. FinalizePassSpellDeclarations-
// MatchTheirPickLists, FinalizePassSpell_NoRegisteredSpellComesNearThe-
// ProbeBound and CardContract_NoRegisteredCardEndsItsFinalizePassAnywhereElse
// play every such spell, but from a pool of floating Energy and Power
// (`setPool(P1, 30, 10)`) and against units without [Deflect]: no rune is
// ever recycled for a Deflect ahead of the cost, so which rune goes, and
// whether its Energy is kept, never mattered. This sweep is the missing case:
// every registered spell that chooses before its cost is paid, played from
// RUNES ONLY, each enemy unit with [Deflect], the runes its own Power needs
// first in line and only then the spare ones. Whenever the play is offered it
// must complete: offered means payable, whatever is chosen.
TEST_F(DeflectBeforeCost, NoRegisteredSpellThatChoosesBeforeItsCostIsLeftUnpayableByADeflect) {
    const GameState blank = state;
    std::vector<CardDefId> ids;
    for (const auto& [id, def] : card_db.all()) {
        const Card* card = card_registry.get(id);
        if (!card || def.card_type != CardType::Spell || id.raw() > 955) continue;
        if ((card->choicesAtFinalization() & Card::kFinalizeResolve) != 0 ||
            card->needsPlayTimeTarget() || card->needsPlayTimeTargetPair())
            ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    ASSERT_GE(ids.size(), 80u);

    // Prefers an enemy unit (they all have Deflect) at every prompt that
    // offers one; otherwise the first or the last option.
    struct Chooser : AgentInterface {
        bool last = false;
        Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
            for (const auto& i : legal)
                if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
                    s.objectExists(i.chosen_objects.front())) {
                    const auto& o = s.getObject(i.chosen_objects.front());
                    if (o.isUnit() && o.controller != i.player && o.deflect_value > 0) return i;
                }
            for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
            return last ? legal.back() : legal.front();
        }
    };

    int offered = 0, completed = 0, paid_deflect = 0;
    std::vector<std::string> unpaid, other;
    for (CardDefId id : ids) {
        const CardDef& def = card_db.get(id);
        Domain spare = Domain::Fury;
        for (int d = 0; d < static_cast<int>(Domain::Count); ++d)
            if (std::find(def.domains.begin(), def.domains.end(), static_cast<Domain>(d)) ==
                def.domains.end()) { spare = static_cast<Domain>(d); break; }
        for (int extra = 0; extra <= 3; ++extra)
        for (int last = 0; last < 2; ++last) {
            state = blank;
            state.battlefields[0].controller = P1;
            state.battlefields[1].controller = P1;
            const GameObjectId spell = addToHand(P1, id);
            // The runes the spell's own cost needs come first; the spare ones,
            // of another Domain, after them. Nothing is floating.
            const int own = std::max(def.energy_cost, def.power_cost);
            const Domain mine = def.domains.empty() ? Domain::Fury : def.domains.front();
            for (int i = 0; i < own; ++i) addRune(P1, mine);
            for (int i = 0; i < extra; ++i) addRune(P1, spare);
            addUnit(P1, kInvalidId, 3, 0);
            addUnit(P1, kInvalidId, 6, 1);
            addUnit(P1, kInvalidId, 2);
            for (int at : {0, 1, -1}) {
                auto& u = state.getObject(addUnit(P2, kInvalidId, 4, at));
                u.keywords.set(Keyword::Deflect);
                u.deflect_value = 1;
            }
            addToHand(P1, 128);
            for (int i = 0; i < 6; ++i) addToDeck(P1, 128);
            const std::string where = def.name + " (" + toString(id) + "), " +
                std::to_string(extra) + " spare rune(s), " + (last ? "last" : "first") + " option";
            int deflects = 0;
            auto conn = events.on_log.connect([&](const LogEvent& e) {
                if (e.message.find("DEFLECT: ") != std::string::npos) ++deflects;
            });
            try {
                std::optional<Intent> play;
                withEngine([&](GameEngine& e) {
                    e.testHook_cleanup();
                    for (const auto& i : e.generateLegalActions())
                        if (i.card == spell && !play) play = i;
                });
                if (play) {
                    ++offered;
                    Chooser a1, a2;
                    a1.last = a2.last = last == 1;
                    withEngine([&](GameEngine& e) {
                        e.testHook_executeIntent(*play);
                        settleLikeTheEngine(e);
                    }, &a1, &a2);
                    ++completed;
                    if (deflects > 0) ++paid_deflect;
                }
            } catch (const std::logic_error& ex) {
                if (std::string(ex.what()).find("cannot be paid for") != std::string::npos)
                    unpaid.push_back(where);
                else
                    other.push_back(where + ": " + ex.what());
            } catch (const std::exception& ex) {
                other.push_back(where + ": " + ex.what());
            }
            conn.disconnect();
        }
    }
    std::cout << "[ " << ids.size() << " spells that choose before their cost; " << offered
              << " plays offered from runes alone, " << completed << " completed, "
              << paid_deflect << " of them paid a Deflect on the way ]\n";
    EXPECT_GT(offered, 200);
    EXPECT_GT(paid_deflect, 100) << "the sweep reaches the case";
    std::string list;
    for (size_t i = 0; i < unpaid.size() && i < 12; ++i) list += "\n  " + unpaid[i];
    EXPECT_TRUE(unpaid.empty()) << unpaid.size()
        << " plays were offered, chosen for, and could not be paid:" << list;
    list.clear();
    for (size_t i = 0; i < other.size() && i < 12; ++i) list += "\n  " + other[i];
    EXPECT_TRUE(other.empty()) << other.size() << " other failures:" << list;
}

} // namespace riftbound::test::deflect_before_cost
