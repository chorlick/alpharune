#include "cards/card.h"
#include "cards/card_helpers.h"
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

class MysticVortex : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "During showdowns here, cards with [Reaction] cost [A] more to play.
    // (Hidden cards have [Reaction].)"
    // The card publishes the standing amount: reaction_tax_power is "what a
    // [Reaction] card costs more in showdowns here". WHEN it is owed is the
    // reader's live check (extraAnyPowerCost), not a condition here: a
    // showdown opens and closes (runShowdown / runCombat) with no aura pass
    // in between, so a value published only "while a showdown is on" was still
    // 0 at the first decisions of every showdown.
    //  - "showdowns here": a Non-Combat Showdown here, or the Combat Showdown
    //    Step of a combat here (CR 316.8.a, 466). The Combat Damage and
    //    Resolution Steps come after "the Showdown closes" (CR 467.2): no tax.
    //  - "here" belongs to "showdowns". The taxed cards are "cards with
    //    [Reaction]", whoever plays them, wherever they go and whatever they
    //    choose (an [Ambush] unit to another battlefield included).
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            bf.reaction_tax_power = 1;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 947;
        d.def_id = R"RB(ven-160-166)RB";
        d.name = R"RB(Mystic Vortex)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-160/166)RB";
        d.collector_number = 160;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(During showdowns here, cards with [Reaction] cost [A] more to play. (Hidden cards have [Reaction].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/55f6fec944c489010464cb554c4661d94518fa75-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_947(CardRegistry& r) {
    r.registerCard(947, std::make_unique<MysticVortex>());
}

}  // namespace riftbound
