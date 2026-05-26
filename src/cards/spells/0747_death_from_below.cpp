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

class DeathFromBelow : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        int might = ctx.state.getObject(targets[0]).current_might;
        ctx.executor.killObject(targets[0]);
        ctx.events.logTrace("DEATH FROM BELOW: killed unit (might=" +
                             std::to_string(might) + ")");
        if (might <= 3) {
            // "You may play this from your trash for [A]." ctx.source is this
            // spell's own object; it lands in trash right after onResolve
            // returns. Grant the controller a this-turn replay for [A]
            // (one power of any single domain).
            PlayerState::TrashReplayGrant grant;
            grant.card = ctx.source;
            grant.energy = 0;
            grant.power = 1;
            grant.any_domain = true;   // [A]
            ctx.state.player(ctx.controller).trash_replay_grants.push_back(grant);
            ctx.events.logTrace("DEATH FROM BELOW: granted [A] trash-replay");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 747;
        d.def_id = R"RB(unl-186-219)RB";
        d.name = R"RB(Death from Below)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-186/219)RB";
        d.collector_number = 186;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Pyke)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Kill a unit at a battlefield. Then, if it had 3 [M] or less, do this: You may play this from your trash for [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/17d93a11a252287e3b0f4bbe32722ecc9469ec66-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_747(CardRegistry& r) {
    r.registerCard(747, std::make_unique<DeathFromBelow>());
}

} // namespace riftbound
