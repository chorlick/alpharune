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

class GrandDuelist : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When one of your units becomes [Mighty], you may exhaust me to channel 1
    // rune exhausted. (A unit is Mighty while it has 5+ [M].)"
    // ESCALATE(WhenAUnitBecomesMighty): the TriggerType enum value exists but has
    // NO dispatch — there is no engine emit for a unit crossing the 5+ Might
    // "becomes Mighty" threshold (Might changes via buffs/auras during cleanup
    // with no edge-detection emit). Left unimplemented pending wired dispatch.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 519;
        d.def_id = R"RB(sfd-205-221)RB";
        d.name = R"RB(Grand Duelist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-205/221)RB";
        d.collector_number = 205;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Fiora)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When one of your units becomes [Mighty], you may exhaust me to channel 1 rune exhausted. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/37064aa79c13316b5dd28f0a2b054821a43f6650-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_519(CardRegistry& r) {
    r.registerCard(519, std::make_unique<GrandDuelist>());
}

} // namespace riftbound
