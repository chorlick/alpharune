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

class SiphonPower : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a battlefield. Give friendly units there +1 [M] this turn and
    //  enemy units there -1 [M] this turn, to a minimum of 1 [M]."
    // Choose a unit at a battlefield to identify the battlefield.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId anchor;
        if (!targets.empty()) {
            anchor = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            anchor = pickTarget(ctx, "Siphon Power (a unit at the battlefield)", legal);
        }
        if (anchor == kInvalidId || !ctx.state.objectExists(anchor)) return;
        auto bf = ctx.state.getObject(anchor).battlefieldId();
        if (!bf) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.battlefieldId() != bf) continue;
            if (obj.controller == ctx.controller)
                ctx.executor.giveTemporaryMight(id, 1);
            else
                ctx.executor.giveTemporaryMight(id, -1, /*minimum=*/1);
        }
        ctx.events.logTrace("SIPHON POWER: friendly +1 / enemy -1 (min 1) at one BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 261;
        d.def_id = R"RB(ogn-266-298)RB";
        d.name = R"RB(Siphon Power)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-266/298)RB";
        d.collector_number = 266;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Viktor)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a battlefield. Give friendly units there +1 [M] this turn and enemy units there -1 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b449170ba312711c82708d1fea2b044822ce5eaa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_261(CardRegistry& r) {
    r.registerCard(261, std::make_unique<SiphonPower>());
}

} // namespace riftbound
