#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class ArachnoidHorror : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Hunt 2]
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        gainXp(ctx, ctx.controller, 2);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }

    // "I can be played to an occupied battlefield if an enemy unit is alone
    // there." The unit-play generator has no hook that ADDS a location (it
    // reads only the narrowing pair below), so this card lists every place it
    // may be played: the normal ones (CR 355.2: base, battlefields its
    // controller controls, open ones under a board-wide grant) plus the
    // battlefields where a lone enemy unit stands.
    // ENGINE-GAP: "Friendly units can be played to an occupied battlefield if
    // an enemy unit is alone there." is not implemented: it needs a
    // PlayerState grant the generator reads for every friendly unit, as
    // grant_friendly_units_open_bf is. A Dragon Roost paid play of this card
    // is not offered either (the narrowing hook bypasses it).
    static bool enemyAloneAt(const GameState& state, PlayerId player, BattlefieldId bf) {
        int units = 0, enemies = 0;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.battlefieldId() != bf) continue;
            ++units;
            if (obj.controller != player) ++enemies;
        }
        return units == 1 && enemies == 1;
    }
    bool restrictsPlayLocations() const override { return true; }
    std::vector<LocationId> getPlayLocations(const GameState& state,
                                             PlayerId player) const override {
        std::vector<LocationId> out;
        out.push_back(BaseLocation{player});
        const auto& ps = state.player(player);
        if (ps.units_play_base_only) return out;   // Mageseeker Warden
        for (const auto& bf : state.battlefields) {
            if (bf.blocks_unit_play) continue;     // Rockfall Path
            const bool controlled = bf.controller.has_value() && *bf.controller == player;
            const bool open = !bf.controller.has_value();
            if (controlled || (open && ps.grant_friendly_units_open_bf) ||
                enemyAloneAt(state, player, bf.id))
                out.push_back(BattlefieldLocation{bf.id});
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 679;
        d.def_id = R"RB(unl-117-219)RB";
        d.name = R"RB(Arachnoid Horror)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-117/219)RB";
        d.collector_number = 117;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hunt);
        d.ability_text = R"RB([Hunt 2] (When I conquer or hold, gain 2 XP.)
I can be played to an occupied battlefield if an enemy unit is alone there.
Friendly units can be played to an occupied battlefield if an enemy unit is alone there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae2ec829a6714ac75e8b203631af08c0c1e1565f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_679(CardRegistry& r) {
    r.registerCard(679, std::make_unique<ArachnoidHorror>());
}

} // namespace riftbound
