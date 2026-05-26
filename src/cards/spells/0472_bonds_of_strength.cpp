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

// "Give two friendly units each +1 [M] this turn."

class BondsOfStrength : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .must_be_friendly = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller == controller) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_units = [&](GameObjectId exclude) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == exclude) continue;
                if (!obj.location.has_value() || !obj.isUnit()) continue;
                if (obj.controller != ctx.controller) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Bonds of Strength",
            friendly_units(kInvalidId),
            [&](GameObjectId picked_a) { return friendly_units(picked_a); });
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a != kInvalidId && ctx.state.objectExists(a))
            ctx.executor.giveTemporaryMight(a, 1);
        if (b != kInvalidId && ctx.state.objectExists(b))
            ctx.executor.giveTemporaryMight(b, 1);
        ctx.events.logTrace("BONDS OF STRENGTH: two friendly units +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 472;
        d.def_id = R"RB(sfd-151-221)RB";
        d.name = R"RB(Bonds of Strength)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-151/221)RB";
        d.collector_number = 151;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Give two friendly units each +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/344034c00d9fad558e98a64efab0310ad4bac37c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_472(CardRegistry& r) {
    r.registerCard(472, std::make_unique<BondsOfStrength>());
}

} // namespace riftbound
