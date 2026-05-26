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

class HextechAnomaly : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return false; }  // Reaction
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "Pay any amount of [A] to [Add] that much Energy." [A] power is paid
        // the standard way — spend floating pool power first, then recycle
        // exhausted runes at base (each recycle = 1 power). Then Add X Energy.
        auto& ps = ctx.state.player(ctx.controller);
        auto base_loc = BaseLocation{ctx.controller};
        std::vector<GameObjectId> recyclable;  // exhausted runes at base
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller || !obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            recyclable.push_back(id);
        }
        int floating = ps.rune_pool.totalPower();
        int max_x = floating + static_cast<int>(recyclable.size());
        int x = pickXAmount(ctx, "Hextech Anomaly: pay X [A] → Add X energy", 0, max_x);
        if (x < 0) return;        // pending choice
        if (x == 0) { ctx.events.logTrace("HEXTECH ANOMALY: X=0"); return; }

        int remaining = x;
        // Spend floating pool power first (universal, then domain).
        int take_u = std::min(remaining, ps.rune_pool.universal_power);
        ps.rune_pool.universal_power -= take_u; remaining -= take_u;
        for (int d = 0; d < static_cast<int>(Domain::Count) && remaining > 0; ++d) {
            int take = std::min(remaining, ps.rune_pool.power[d]);
            ps.rune_pool.power[d] -= take; remaining -= take;
        }
        // Recycle exhausted runes for the rest (move base → rune deck).
        for (size_t i = 0; i < recyclable.size() && remaining > 0; ++i, --remaining) {
            auto& r = ctx.state.getObject(recyclable[i]);
            r.location = std::nullopt;
            r.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), recyclable[i]);
        }
        ctx.executor.addFloatingEnergy(ctx.controller, x);
        ctx.events.logTrace("HEXTECH ANOMALY: paid " + std::to_string(x) +
                             " [A] → +" + std::to_string(x) + " energy");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 405;
        d.def_id = R"RB(sfd-083-221)RB";
        d.name = R"RB(Hextech Anomaly)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-083/221)RB";
        d.collector_number = 83;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — Pay any amount of [A] to [Add] that much Energy. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/32a46943778755e2b6210e5adb4507780f08e2c8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_405(CardRegistry& r) {
    r.registerCard(405, std::make_unique<HextechAnomaly>());
}

} // namespace riftbound
