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

class ImposingChallenger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I move, you may move an enemy unit here with less Might than me to
    //  a different battlefield."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }

    std::vector<GameObjectId> candidates(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        const auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return out;
        int my_might = self.current_might;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            if (obj.untargetable_by_enemy) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != *my_bf) continue;   // "here" = my battlefield
            if (obj.current_might >= my_might) continue;  // "with less Might than me"
            out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int conf = confirmOptional(ctx, "Imposing Challenger: move an enemy unit here?",
                                   [&]() { return !candidates(ctx).empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / none

        GameObjectId tgt = pickTarget(ctx, "Imposing Challenger: choose an enemy unit",
                                      candidates(ctx));
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;

        // "to a different battlefield" — pick any battlefield other than my_bf.
        std::optional<BattlefieldId> dest;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.id == *my_bf) continue;
            dest = bf.id;
            break;
        }
        if (!dest) return;  // no other battlefield exists
        ctx.executor.moveToBattlefield(tgt, *dest);
        ctx.events.logTrace("IMPOSING CHALLENGER: moved an enemy unit here to a different BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 667;
        d.def_id = R"RB(unl-105-219)RB";
        d.name = R"RB(Imposing Challenger)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-105/219)RB";
        d.collector_number = 105;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, you may move an enemy unit here with less Might than me to a different battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/01596f6039ef5618fae686e1c7df291e1c570fc3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_667(CardRegistry& r) {
    r.registerCard(667, std::make_unique<ImposingChallenger>());
}

} // namespace riftbound
