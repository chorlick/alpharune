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

class SyndraTranscendent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm in a showdown, your spells have [Repeat] [2][P]."
    // Wired via PlayerState::spells_have_repeat_* (set in applyPassiveAura while
    // an instance of me is at a battlefield with a showdown in progress). The
    // spell-play path builds a RepeatCost from these when the spell has no
    // printed [Repeat]. [2][P] = 2 energy + 1 Chaos power.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller)
                continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            for (const auto& b : state.battlefields) {
                if (b.id == *bf && b.showdown_in_progress) {
                    auto& ps = state.player(controller);
                    ps.spells_have_repeat_energy = 2;
                    ps.spells_have_repeat_power = 1;
                    ps.spells_have_repeat_domain = Domain::Chaos;  // [P]
                    return;
                }
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 708;
        d.def_id = R"RB(unl-146-219)RB";
        d.name = R"RB(Syndra, Transcendent)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-146/219)RB";
        d.collector_number = 146;
        d.artist = R"RB(Naifan Zhang)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB", R"RB(Syndra)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB(While I'm in a showdown, your spells have [Repeat] [2][P]. (You may pay the additional cost to repeat the spell's effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a3695d011175801aaf6ec5a6557b65b8d6341e81-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_708(CardRegistry& r) {
    r.registerCard(708, std::make_unique<SyndraTranscendent>());
}

} // namespace riftbound
