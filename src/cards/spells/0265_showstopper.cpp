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

class Showstopper : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Buff a friendly unit in your base, then move it to a battlefield."
    // (Previously moved to base — direction was inverted.)
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId unit = targets[0];
        if (ctx.state.getObject(unit).buff_count == 0)
            ctx.executor.buffUnit(unit);  // "+1 [M] buff if it doesn't have one"
        // Move it to a battlefield (prefer one the controller already controls).
        BattlefieldId dest = kInvalidId;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == ctx.controller) {
                dest = bf.id;
                break;
            }
        }
        if (dest == kInvalidId && !ctx.state.battlefields.empty())
            dest = ctx.state.battlefields.front().id;
        if (dest != kInvalidId)
            ctx.executor.moveToBattlefield(unit, dest);
        ctx.events.logTrace("SHOWSTOPPER: buff base unit, move to a battlefield");
    }
    TargetRequirements getTargetRequirements() const override {
        // "a friendly unit in your base"
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 265;
        d.def_id = R"RB(ogn-270-298)RB";
        d.name = R"RB(Showstopper)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-270/298)RB";
        d.collector_number = 270;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Sett)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Buff a friendly unit in your base, then move it to a battlefield. (If it doesn't have a buff, it gets a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c9b6f7a7cca1589fb53276f74ac8bc547b31e5ec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_265(CardRegistry& r) {
    r.registerCard(265, std::make_unique<Showstopper>());
}

} // namespace riftbound
