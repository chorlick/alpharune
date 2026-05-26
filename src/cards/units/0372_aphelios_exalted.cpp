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

// "When you attach an Equipment to me, choose one that hasn't been chosen this
//  turn — Ready 2 runes. / Channel 1 rune exhausted. / Buff a friendly unit."
// ESCALATE(attach-Equipment TriggerType + event): no event is emitted when a
//  gear/Equipment is attached to a unit (standardEquip / attachFree emit only
//  ObjectStateChangedEvent{"equipped"}, which TriggerManager does not dispatch
//  as a trigger), and there is no "When you attach an Equipment to me"
//  TriggerType. So this triggered modal ability cannot be wired from the card
//  layer. The modal body itself (ready 2 runes / channel 1 exhausted / buff,
//  with once-per-turn-per-mode tracking) is implementable once the trigger
//  exists, but without the firing event there is nothing to drive it — so the
//  whole ability is escalated rather than partially stubbed.
class ApheliosExalted : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 372;
        d.def_id = R"RB(sfd-049-221)RB";
        d.name = R"RB(Aphelios, Exalted)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-049/221)RB";
        d.collector_number = 49;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Aphelios)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you attach an Equipment to me, choose one that hasn't been chosen this turn —
Ready 2 runes.Channel 1 rune exhausted.Buff a friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/df9ccbba2501059e5781a44434745a1f0e33ecae-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_372(CardRegistry& r) {
    r.registerCard(372, std::make_unique<ApheliosExalted>());
}

} // namespace riftbound
