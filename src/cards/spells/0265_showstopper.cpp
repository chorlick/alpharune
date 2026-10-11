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

class Showstopper : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Buff a friendly unit in your base, then move it to a battlefield."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        GameObjectId unit = targets[0];
        const auto& u = ctx.state.getObject(unit);
        if (!u.isAtBase()) return;   // no longer "in your base": not a legal target
        // The battlefield is the controller's choice. It is asked for before
        // the buff: the callback is re-entered with the answer, and the buff
        // must be placed once.
        const int dest = pickMoveDest(*this, ctx, "Showstopper: move it to", u,
                                      /*battlefields_only=*/true);
        if (dest == -1) return;      // waiting for the battlefield
        ctx.executor.buffUnit(unit);  // "+1 [M] buff if it doesn't have one"
        if (dest >= 0) moveTo(ctx, unit, dest);
        RIFT_TRACE(ctx.events, "SHOWSTOPPER: buff base unit, move to a battlefield");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    // "a friendly unit in your base"
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto id : Card::enumerateLegalTargets(state, controller))
            if (state.getObject(id).isAtBase()) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
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
