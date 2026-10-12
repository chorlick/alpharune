#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class FortifiedPosition : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you defend here, choose a unit. It gains [Shield 2] this combat."
    // "A unit": any unit on the board, either player's (not only a defender).
    // "This combat", not "this turn": the Shield is a continuous effect that
    // lasts while the combat it was given in is going on (applyPassiveAura),
    // as Fiora, Peerless's "this combat" is. The chosen unit remembers the
    // amount, the battlefield of that combat and the turn; once that
    // battlefield has no combat in progress the memory is dropped, so a unit
    // that defends again later in the turn does not have it.
    static constexpr const char* kShield = "__fortified_shield";
    static constexpr const char* kCombatBf = "__fortified_combat_bf";   // battlefield + 1
    static constexpr const char* kTurn = "__fortified_turn";            // turn + 1
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto units = chooseableUnits(ctx.state, ctx.controller,
                                           [](const GameObject&) { return true; });
        GameObjectId picked = pickTarget(ctx, "Fortified Position (gain Shield 2)", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;     // none / mistarget / waiting
        const auto here = battlefieldOfCard(ctx.state, ctx.source);
        if (!here.has_value() || !ctx.state.objectExists(picked)) return;
        auto& unit = ctx.state.getObject(picked);
        if (!unit.isUnit() || !unit.location.has_value()) return;   // it has left
        auto& c = unit.card_counters;
        const int combat = static_cast<int>(here->raw()) + 1;
        const int turn = ctx.state.turn.turn_number + 1;
        // A grant left from an earlier combat is not added to.
        if (c[kCombatBf] != combat || c[kTurn] != turn) c[kShield] = 0;
        c[kShield] += 2;
        c[kCombatBf] = combat;
        c[kTurn] = turn;
        ctx.executor.recalculateContinuousEffects();
        RIFT_TRACE(ctx.events, "FORTIFIED POSITION: defend -> " + unit.name +
                            " gains [Shield 2] this combat");
    }
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Called once per slot holding this card and not told which, so each
        // unit is given its Shield once.
        // (Every object: a unit that has left the board, or whose combat is over, has its memory dropped.)
        for (auto& [id, obj] : state.objects) {
            auto shield = obj.card_counters.find(kShield);
            if (shield == obj.card_counters.end()) continue;
            const int amount = shield->second;
            const int combat = obj.card_counters[kCombatBf];
            const bool this_turn = obj.card_counters[kTurn] == state.turn.turn_number + 1;
            bool going = false;
            for (const auto& bf : state.battlefields)
                if (static_cast<int>(bf.id.raw()) + 1 == combat) going = bf.combat_in_progress;
            if (!obj.location.has_value() || !this_turn || !going || amount <= 0) {
                obj.card_counters.erase(kShield);
                obj.card_counters.erase(kCombatBf);
                obj.card_counters.erase(kTurn);
                continue;
            }
            bool given = false;
            for (const auto& ae : obj.aura_effects)
                if (ae.source == id && ae.keyword == Keyword::Shield &&
                    ae.keyword_value == amount) given = true;
            if (!given) auraKeyword(obj, id, Keyword::Shield, amount);
        }
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 274;
        d.def_id = R"RB(ogn-279-298)RB";
        d.name = R"RB(Fortified Position)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-279/298)RB";
        d.collector_number = 279;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you defend here, choose a unit. It gains [Shield 2] this combat. (+2 [M] while it's a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/45363bbd907f4f3717868cb04b3cfed814b3bb32-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_274(CardRegistry& r) {
    r.registerCard(274, std::make_unique<FortifiedPosition>());
}

} // namespace riftbound
