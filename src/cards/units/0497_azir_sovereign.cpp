#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "[Accelerate] ... When I attack, you may move any number of your token units
//  to this battlefield." ([Accelerate] is engine-handled.)
// "Any number" + "you may": auto-moves all friendly token units not already
// here — moving your tokens onto your attack is strictly beneficial, so the
// optimal "any number" is all of them.

class AzirSovereign : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        std::vector<GameObjectId> tokens;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.isToken() || !obj.location.has_value()) continue;
            if (obj.battlefieldId() == my_bf) continue;  // already here
            tokens.push_back(id);
        }
        for (auto id : tokens) ctx.executor.moveToBattlefield(id, *my_bf);
        if (!tokens.empty())
            ctx.events.logTrace("AZIR SOVEREIGN: attack -> moved " +
                std::to_string(tokens.size()) + " token unit(s) to this battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 497;
        d.def_id = R"RB(sfd-177-221)RB";
        d.name = R"RB(Azir, Sovereign)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-177/221)RB";
        d.collector_number = 177;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Bird)RB", R"RB(Azir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][Y] as an additional cost to have me enter ready.)
When I attack, you may move any number of your token units to this battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/088970936fc800bd2c4b36636734e16c83e1c8f9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_497(CardRegistry& r) {
    r.registerCard(497, std::make_unique<AzirSovereign>());
}

} // namespace riftbound
