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

class GentlemenSDuel : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Give a friendly unit +3 [M] this turn. Then choose an enemy unit.
    // They deal damage equal to their Mights to each other."
    bool needsPlayTimeTargetPair() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // First target: friendly unit. Second: enemy unit.
        auto legal_a = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value())
                    out.push_back(id);
            return out;
        }();
        auto legal_b_fn = [&](GameObjectId /*a*/) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller != ctx.controller &&
                    obj.location.has_value() && !obj.untargetable_by_enemy)
                    out.push_back(id);
            return out;
        };
        auto [friendly, enemy] = pickTargetPair(
            ctx, "Gentlemen's Duel: friendly unit, then enemy unit",
            legal_a, legal_b_fn);
        if (friendly == kInvalidId) return;  // suspend or fizzle

        // Give the friendly unit +3 [M] this turn (resolves before the duel).
        ctx.executor.giveTemporaryMight(friendly, 3);

        if (enemy == kInvalidId) return;  // no enemy chosen — duel doesn't happen
        if (!ctx.state.objectExists(friendly) || !ctx.state.objectExists(enemy))
            return;

        // Both deal damage equal to their current Mights to each other.
        // Snapshot both mights BEFORE dealing (simultaneous), then kill
        // lethally-damaged units.
        int fm = ctx.state.getObject(friendly).current_might;
        int em = ctx.state.getObject(enemy).current_might;
        ctx.executor.dealDamage(enemy, fm, friendly);
        ctx.executor.dealDamage(friendly, em, enemy);
        for (auto id : {friendly, enemy}) {
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        }
        ctx.events.logTrace("GENTLEMEN'S DUEL: +3 [M] to friendly, then duel ("
                            + std::to_string(fm) + " vs " + std::to_string(em) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 306;
        d.def_id = R"RB(ogs-008-024)RB";
        d.name = R"RB(Gentlemen's Duel)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-008/024)RB";
        d.collector_number = 8;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a friendly unit +3 [M] this turn. Then choose an enemy unit. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4568ab827fdcd42d6bb86b1c2de6182e286c9ee9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_306(CardRegistry& r) {
    r.registerCard(306, std::make_unique<GentlemenSDuel>());
}

} // namespace riftbound
