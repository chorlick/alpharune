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
    // for [B]." Wired via Card::alternativePlayCost: when the controller has
    // spent 4+ on a single spell this turn (PlayerState::max_spell_spent_this_turn,
    // tracked at spell play), the champion-play generator offers an extra
    // Intent::use_alt_play_cost option whose cost is [B] = 1 Mind power, and
    // executePlayCard pays that instead of the printed [4].
    AltPlayCost alternativePlayCost(const GameState& state,
                                    PlayerId player) const override {
        if (state.player(player).max_spell_spent_this_turn < 4) return {};
        AltPlayCost c;
        c.valid = true;
        c.energy = 0;
        c.power = 1;
        c.power_domain = Domain::Mind;  // [B]
        return c;
    }
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
