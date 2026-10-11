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

class KinkouTemple : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "Units here with [Tank] have +1 [M]."
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (const auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            for (auto& [uid, u] : state.objects.onBoard()) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                // Printed or "this turn" Tank, or Tank granted in this pass by
                // an Equipment (Doran's Shield) or another card's hook.
                if (!hasKeywordInAuraPass(u, Keyword::Tank)) continue;
                // Once per Temple (the hook runs once per slot holding this card).
                if (hasAuraFrom(u, bf.card_object_id)) continue;
                auraMight(u, bf.card_object_id, 1);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 946;
        d.def_id = R"RB(ven-159-166)RB";
        d.name = R"RB(Kinkou Temple)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-159/166)RB";
        d.collector_number = 159;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Units here with [Tank] have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/afef083faa1f3813e5744a552c9873c798bedfd5-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_946(CardRegistry& r) {
    r.registerCard(946, std::make_unique<KinkouTemple>());
}

}  // namespace riftbound
