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

class MageseekerWarden : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm at a battlefield, opponents can only play units to their base."
    //   Wired via PlayerState::units_play_base_only set on the OPPONENT when an
    //   instance of me is at a battlefield (the play-from-hand generator then
    //   suppresses that player's BF unit-plays). Flag is reset+recomputed each
    //   cleanup, so it lapses when I leave the board / go to base.
    // "While I'm at a battlefield, spells and abilities can't ready enemy units
    //  and gear." — DEFERRED (needs a ready-suppression hook in readyObject that
    //  knows the ready came from a spell/ability source; tracked separately).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // "While I'm at a battlefield" — only when an instance of me is actually
        // at a BF (applyPassiveAura doesn't pass our object id, so scan).
        bool at_bf = false;
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.isAtBattlefield()) { at_bf = true; break; }
        }
        if (!at_bf) return;
        PlayerId opp = (controller == PlayerId::Player1) ? PlayerId::Player2
                                                         : PlayerId::Player1;
        state.player(opp).units_play_base_only = true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 70;
        d.def_id = R"RB(ogn-070-298)RB";
        d.name = R"RB(Mageseeker Warden)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-070/298)RB";
        d.collector_number = 70;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Demacia)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While I'm at a battlefield, opponents can only play units to their base.
While I'm at a battlefield, spells and abilities can't ready enemy units and gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae844b929f817cdf76fe40c7bf5d5fc02062bdac-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_70(CardRegistry& r) {
    r.registerCard(70, std::make_unique<MageseekerWarden>());
}

} // namespace riftbound
