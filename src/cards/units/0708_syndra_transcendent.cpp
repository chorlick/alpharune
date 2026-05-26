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
    // ESCALATE(spells-have-repeat continuous field): [Repeat] (CR 820) is an
    // optional additional cost paid at spell-play time (ChainItem::repeats_paid),
    // parsed by the engine from the SPELL's OWN ability_text. There is no
    // surface for a continuous effect to GRANT [Repeat] to a player's spells —
    // the play-action generator consults no "spells have Repeat" player field,
    // and applyPassiveAura's AuraEffect carries only Might/keyword/combat-damage
    // suppression. Whole card blocked. (Same gap as The Academy #772.)
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
