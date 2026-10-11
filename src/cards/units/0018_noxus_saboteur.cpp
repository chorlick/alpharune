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

// "Your opponents' [Hidden] cards can't be revealed here."
// A facedown card is revealed by being played from facedown (CR 811), so
// while I'm at a battlefield my opponents can't play their hidden cards there.
// Set per opponent in BattlefieldState::opp_hidden_unrevealable from
// applyPassiveAura (reset every aura pass); the facedown play generator reads
// it. The Saboteur itself has no [Hidden].

class NoxusSaboteur : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller)
                continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            for (auto& b : state.battlefields)
                if (b.id == *bf) b.opp_hidden_unrevealable[playerIndex(opponent(controller))] = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 18;
        d.def_id = R"RB(ogn-018-298)RB";
        d.name = R"RB(Noxus Saboteur)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-018/298)RB";
        d.collector_number = 18;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        // No printed [Hidden]: the text only refers to the opponents' Hidden cards.
        d.ability_text = R"RB(Your opponents' [Hidden] cards can't be revealed here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b78f0c822cb984db24ac3f1956cc8c10f8f88b22-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_18(CardRegistry& r) {
    r.registerCard(18, std::make_unique<NoxusSaboteur>());
}

} // namespace riftbound
