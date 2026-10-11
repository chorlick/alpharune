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

class GemcraftSeer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Vision]" on me is the keyword's own ability (TriggerManager queues
    // it, engine/keyword_abilities.h). "Other friendly units have [Vision]."
    // is granted by aura, once per Seer on the board (applyPassiveAura is
    // called once per card and controller, so every instance is looped):
    // each grant is an instance of [Vision], and instances trigger separately
    // (CR 817.2).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self_id, GameObject& self) {
            if (!self.location.has_value()) return;
            for (auto& [uid, u] : state.objects.onBoard()) {
                if (uid == self_id) continue;  // "other"
                if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                    continue;
                auraKeyword(u, self_id, Keyword::Vision);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 100;
        d.def_id = R"RB(ogn-100-298)RB";
        d.name = R"RB(Gemcraft Seer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-100/298)RB";
        d.collector_number = 100;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
Other friendly units have [Vision].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4b6965bd592a5f602ab932094d677b6c85b2372b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_100(CardRegistry& r) {
    r.registerCard(100, std::make_unique<GemcraftSeer>());
}

} // namespace riftbound
