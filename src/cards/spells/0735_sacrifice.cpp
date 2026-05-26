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

class Sacrifice : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
            const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.controller == controller &&
                obj.location.has_value() && obj.current_might >= 5) {
                out.push_back(id);
            }
        }
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !enumerateLegalTargets(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId victim = pickTarget(ctx, "Sacrifice: kill a Mighty unit", legal);
        if (victim == kInvalidId) return;  // no Mighty unit / suspended
        if (!ctx.state.objectExists(victim)) return;
        ctx.executor.killObject(victim);  // additional cost paid
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("SACRIFICE: killed a Mighty unit -> draw 2 + channel 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 735;
        d.def_id = R"RB(unl-173-219)RB";
        d.name = R"RB(Sacrifice)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-173/219)RB";
        d.collector_number = 173;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, kill a friendly [Mighty] unit. (A unit is Mighty while it has 5+ [M].)
Draw 2 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/139153ae4b4f786442018c09765c67e35515df24-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_735(CardRegistry& r) {
    r.registerCard(735, std::make_unique<Sacrifice>());
}

} // namespace riftbound
