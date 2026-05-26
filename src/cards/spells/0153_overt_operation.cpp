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

class OvertOperation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value()) {
                    out.push_back(id);
                }
            }
            return out;
        };

        // "you may spend its buff to ready it" — gate the whole spend-to-ready
        // step on one agent decision; legal only if some friendly unit
        // actually has a buff to spend.
        int answer = confirmOptional(ctx, "Overt Operation: spend buffs to ready",
            [&]() {
                for (auto id : friendly_units()) {
                    if (ctx.state.getObject(id).buff_count > 0) return true;
                }
                return false;
            });
        if (answer == -1) return;  // yielded for agent input

        if (answer == 1) {
            for (auto id : friendly_units()) {
                auto& u = ctx.state.getObject(id);
                if (u.buff_count <= 0) continue;
                // Spend one buff: -1 [M], then ready.
                u.buff_count -= 1;
                u.temp_might_bonus -= 1;
                u.recomputeMight();
                ctx.executor.readyObject(id);
                ctx.events.logTrace("OVERT OPERATION: spent buff to ready " + u.name);
            }
        }

        // "Then buff all friendly units." (Engine buff = +1 [M].)
        for (auto id : friendly_units()) {
            ctx.executor.buffUnit(id);
        }
        ctx.events.logTrace("OVERT OPERATION: buffed all friendly units");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 153;
        d.def_id = R"RB(ogn-153-298)RB";
        d.name = R"RB(Overt Operation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-153/298)RB";
        d.collector_number = 153;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
For each friendly unit, you may spend its buff to ready it. Then buff all friendly units. (Each one that doesn't have a buff gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/69dba13c930ba3962851346d2bc6cbeb4ca48455-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_153(CardRegistry& r) {
    r.registerCard(153, std::make_unique<OvertOperation>());
}

} // namespace riftbound
