#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class StareDown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit and a battlefield. Move all enemy units at that
    //  battlefield with less Might than the chosen unit to their base. Gain 1 XP."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    std::vector<GameObjectId> friendlyUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId chosen = pickTarget(ctx, "Stare Down: choose a friendly unit",
                                         friendlyUnits(ctx));
        if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) {
            // Still gain XP per card text? Card requires a chosen unit, so if
            // none exists the spell does nothing meaningful. Gain XP regardless
            // (the "Gain 1 XP" clause is unconditional).
            ctx.state.player(ctx.controller).xp += 1;
            return;
        }
        int threshold = ctx.state.getObject(chosen).current_might;
        PlayerId opp = opponent(ctx.controller);

        // "a battlefield" — no BF-pick API; choose the battlefield holding the
        // most eligible enemy units (enemies with less Might than the chosen).
        std::map<BattlefieldId, std::vector<GameObjectId>> by_bf;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            if (obj.current_might >= threshold) continue;
            by_bf[*bf].push_back(id);
        }
        BattlefieldId best_bf = kInvalidId;
        size_t best = 0;
        for (auto& [bf, units] : by_bf) {
            if (units.size() > best) { best = units.size(); best_bf = bf; }
        }
        if (best_bf != kInvalidId) {
            for (auto id : by_bf[best_bf]) {
                if (ctx.state.objectExists(id)) ctx.executor.moveToBase(id);
            }
            ctx.events.logTrace("STARE DOWN: moved enemy units (< " +
                                 std::to_string(threshold) + "M) at a BF to base");
        }
        ctx.state.player(ctx.controller).xp += 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 669;
        d.def_id = R"RB(unl-107-219)RB";
        d.name = R"RB(Stare Down)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-107/219)RB";
        d.collector_number = 107;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Choose a friendly unit and a battlefield. Move all enemy units at that battlefield with less Might than the chosen unit to their base. Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/58d0353a966bb0ca292855a0a55de5769e28d155-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_669(CardRegistry& r) {
    r.registerCard(669, std::make_unique<StareDown>());
}

} // namespace riftbound
