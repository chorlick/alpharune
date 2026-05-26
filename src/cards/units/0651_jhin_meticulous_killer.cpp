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

class JhinMeticulousKiller : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Vision]" is engine-handled.
    // "If you've spent [4] or more to play a spell this turn, you may play me
    // for [B]." is an ENGINE GAP. Two missing pieces: (1) there is no
    // per-card alternative-cost ("play me for X instead") play path — only
    // selfCostReduction (energy-only) and OptionalAdditionalCost exist, neither
    // of which can swap the printed cost for a fixed [B] (Mind) power; and
    // (2) there is no persistent "spent [4]+ on a spell this turn" flag
    // (PlayerState::last_spell_energy_spent tracks only the MOST RECENT spell).
    // Left unimplemented.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 651;
        d.def_id = R"RB(unl-089-219)RB";
        d.name = R"RB(Jhin, Meticulous Killer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-089/219)RB";
        d.collector_number = 89;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Jhin)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
If you've spent [4] or more to play a spell this turn, you may play me for [B].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/88fb6023ee9bd90fef3f36995ca27615dcd669f7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_651(CardRegistry& r) {
    r.registerCard(651, std::make_unique<JhinMeticulousKiller>());
}

} // namespace riftbound
