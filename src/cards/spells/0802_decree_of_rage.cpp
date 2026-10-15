#include "cards/card.h"
#include "cards/card_helpers.h"
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

class DecreeOfRage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "This can't be countered. Deal 4 to an enemy Calm ([G]) unit."
    // Every counter effect asks card_helpers.h isUncounterable() before it
    // removes a chain item (counterChosenItem, Defy, Hard Bargain, Flurry of
    // Feathers, Abandon, Rebuttal).
    bool cantBeCountered() const override { return true; }
    static bool hasDomain(const GameObject& obj, Domain d) {
        return std::find(obj.domains.begin(), obj.domains.end(), d) != obj.domains.end();
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    using Card::enumerateLegalTargets;
    std::vector<GameObjectId> enumerateLegalTargets(const GameState& state,
                                                    PlayerId controller) const override {
        return chooseableUnits(state, controller, [&](const GameObject& o) {
            return o.controller != controller && hasDomain(o, Domain::Calm);
        });
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Decree of Rage", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.dealDamage(picked, 4, ctx.source);
        if (ctx.state.objectExists(picked) && ctx.state.getObject(picked).hasLethalDamage())
            ctx.executor.killObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 802;
        d.def_id = R"RB(ven-015-166)RB";
        d.name = R"RB(Decree of Rage)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-015/166)RB";
        d.collector_number = 15;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
This can't be countered.
Deal 4 to an enemy Calm ([G]) unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/320d9f52a4f22fa3405bb9da82c4d02451d7c133-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_802(CardRegistry& r) {
    r.registerCard(802, std::make_unique<DecreeOfRage>());
}

}  // namespace riftbound
