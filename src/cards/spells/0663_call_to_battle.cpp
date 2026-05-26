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

class CallToBattle : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Move a unit you control to a battlefield you control. Then, choose an
    //  opponent. They move a unit they control to the same battlefield."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
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
        GameObjectId unit = pickTarget(ctx, "Call to Battle: move a unit you control",
                                       friendlyUnits(ctx));
        if (unit == kInvalidId || !ctx.state.objectExists(unit)) return;

        // Destination: a battlefield you control that this unit isn't already at.
        auto unit_bf = ctx.state.getObject(unit).battlefieldId();
        std::optional<BattlefieldId> dest;
        for (const auto& bf : ctx.state.battlefields) {
            if (!bf.controller || *bf.controller != ctx.controller) continue;
            if (unit_bf && *unit_bf == bf.id) continue;
            dest = bf.id;
            break;
        }
        // Fall back to any battlefield you control (even if already there).
        if (!dest) {
            for (const auto& bf : ctx.state.battlefields) {
                if (bf.controller && *bf.controller == ctx.controller) { dest = bf.id; break; }
            }
        }
        if (!dest) return;  // no controlled battlefield to move to
        ctx.executor.moveToBattlefield(unit, *dest);
        ctx.events.logTrace("CALL TO BATTLE: moved a friendly unit to a controlled BF");

        // "Then, choose an opponent. They move a unit they control to the same
        //  battlefield." Opponent's choice isn't surfaced through the caster's
        //  agent; approximate their rational pick (highest-Might unit not
        //  already there).
        PlayerId opp = opponent(ctx.controller);
        GameObjectId opp_unit = kInvalidId;
        int best_might = -1;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            auto obf = obj.battlefieldId();
            if (obf && *obf == *dest) continue;   // already there
            if (obj.current_might > best_might) { opp_unit = id; best_might = obj.current_might; }
        }
        if (opp_unit != kInvalidId && ctx.state.objectExists(opp_unit)) {
            ctx.executor.moveToBattlefield(opp_unit, *dest);
            ctx.events.logTrace("CALL TO BATTLE: opponent moved a unit to the same BF");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 663;
        d.def_id = R"RB(unl-101-219)RB";
        d.name = R"RB(Call to Battle)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-101/219)RB";
        d.collector_number = 101;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Move a unit you control to a battlefield you control. Then, choose an opponent. They move a unit they control to the same battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c7e0f93c72654f786f5df37895c6e95fa9d4d60-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_663(CardRegistry& r) {
    r.registerCard(663, std::make_unique<CallToBattle>());
}

} // namespace riftbound
