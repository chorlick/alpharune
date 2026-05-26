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

class AnnieStubborn : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    std::vector<GameObjectId> trashSpells(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isSpell()) out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto eligible = trashSpells(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Annie, Stubborn (spell from trash)",
                                          eligible);
        // pickTarget returned kInvalidId because it suspended for an agent
        // decision (resume_point 7) — return and resume later.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(picked);
        obj.zone = ZoneType::Hand;
        obj.location = std::nullopt;
        ps.hand.push_back(picked);
        ctx.events.logTrace("ANNIE, STUBBORN: returned " + obj.name +
                             " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 308;
        d.def_id = R"RB(ogs-010-024)RB";
        d.name = R"RB(Annie, Stubborn)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-010/024)RB";
        d.collector_number = 10;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Annie)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, return a spell from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/44eb968cf0c54e75970588b69eef5c5f5ccc9b24-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_308(CardRegistry& r) {
    r.registerCard(308, std::make_unique<AnnieStubborn>());
}

} // namespace riftbound
