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

// "Play me only to a battlefield you conquered this turn. (You can't play me
// anywhere else.)"
// Wired via the NARROWING play-location hook: restrictsPlayLocations() makes
// the action generator use ONLY getPlayLocations(), suppressing the default
// base + controlled-BF plays. getPlayLocations() returns each battlefield the
// player conquered this turn (BattlefieldState::conquered_on_turn /
// conquered_by_player, stamped in GameEngine::scoreConquer). If none, the unit
// is unplayable this turn.

class PerchedGrimwyrm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool restrictsPlayLocations() const override { return true; }
    std::vector<LocationId> getPlayLocations(const GameState& state,
                                             PlayerId player) const override {
        std::vector<LocationId> out;
        for (const auto& bf : state.battlefields) {
            if (bf.conquered_by_player == player &&
                bf.conquered_on_turn == state.turn.turn_number)
                out.push_back(BattlefieldLocation{bf.id});
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 338;
        d.def_id = R"RB(sfd-015-221)RB";
        d.name = R"RB(Perched Grimwyrm)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-015/221)RB";
        d.collector_number = 15;
        d.artist = R"RB(Kudos Productions & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 4;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Play me only to a battlefield you conquered this turn. (You can't play me anywhere else.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/71af1587378c1c3feabd1b148fb776e42c7d27e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_338(CardRegistry& r) {
    r.registerCard(338, std::make_unique<PerchedGrimwyrm>());
}

} // namespace riftbound
