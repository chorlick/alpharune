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

class DivineJudgment : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands.
    //  Recycle the rest."
    // Each player keeps up to 2 of each category; the rest are recycled.
    // The "which 2 to keep" is the player's choice; approximated here by
    // keeping the highest-Might units and the first cards in each other
    // category (documented approximation — per-player selection not plumbed).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        for (auto p : {ctx.controller, opponent(ctx.controller)}) {
            recycleExcept(ctx, p);
        }
        ctx.events.logTrace("DIVINE JUDGMENT: each player keeps 2/category, recycle rest");
    }

private:
    void recycleExcept(CardContext& ctx, PlayerId p) const {
        // Gather candidates per category.
        std::vector<std::pair<int, GameObjectId>> units;   // (might, id) on board
        std::vector<GameObjectId> gear;                     // on board
        std::vector<GameObjectId> runes;                    // on board (base)
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != p && obj.owner != p) continue;
            if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                units.emplace_back(obj.current_might, id);
            else if (obj.isGear() && obj.controller == p && obj.location.has_value())
                gear.push_back(id);
            else if (obj.isRune() && obj.controller == p && obj.location.has_value())
                runes.push_back(id);
        }
        // Keep the 2 highest-Might units: sort ascending, recycle all but the
        // last 2 entries.
        std::sort(units.begin(), units.end());
        std::vector<GameObjectId> to_recycle;
        size_t keep_units = units.size() > 2 ? units.size() - 2 : 0;
        for (size_t i = 0; i < keep_units; ++i) to_recycle.push_back(units[i].second);
        for (size_t i = 2; i < gear.size(); ++i)  to_recycle.push_back(gear[i]);
        for (size_t i = 2; i < runes.size(); ++i) to_recycle.push_back(runes[i]);

        // Hand cards: keep first 2, recycle the rest (remove from hand first).
        auto& ps = ctx.state.player(p);
        std::vector<GameObjectId> hand_recycle;
        if (ps.hand.size() > 2)
            hand_recycle.assign(ps.hand.begin() + 2, ps.hand.end());
        for (auto cid : hand_recycle) {
            auto it = std::find(ps.hand.begin(), ps.hand.end(), cid);
            if (it != ps.hand.end()) ps.hand.erase(it);
        }
        to_recycle.insert(to_recycle.end(), hand_recycle.begin(), hand_recycle.end());

        if (!to_recycle.empty())
            ctx.executor.recycleCards(p, to_recycle);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 244;
        d.def_id = R"RB(ogn-244-298)RB";
        d.name = R"RB(Divine Judgment)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-244/298)RB";
        d.collector_number = 244;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Each player chooses 2 units, 2 gear, 2 runes, and 2 cards in their hands. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/995778eea2e24fdc62ade38c2baa25b9f1e6ab79-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_244(CardRegistry& r) {
    r.registerCard(244, std::make_unique<DivineJudgment>());
}

} // namespace riftbound
