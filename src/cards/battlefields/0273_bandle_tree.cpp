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

class BandleTree : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may hide an additional card here." Raise this BF's facedown
    // capacity from the default 1 to 2. Idempotent (set, not increment), so
    // re-running the aura recalc is safe.
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            if (bf.facedown_max_occupancy < 2) bf.facedown_max_occupancy = 2;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 273;
        d.def_id = R"RB(ogn-278-298)RB";
        d.name = R"RB(Bandle Tree)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-278/298)RB";
        d.collector_number = 278;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(You may hide an additional card here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/538f93a34006c4afaddb890cbc75b4db222f1783-1038x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_273(CardRegistry& r) {
    r.registerCard(273, std::make_unique<BandleTree>());
}

} // namespace riftbound
