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

class GemcraftSeer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Clause 1: "[Vision]" on me — engine-handled (TriggerManager::onCardPlayed
    // peeks the top card on play for any unit with the Vision keyword).
    // Clause 2: "Other friendly units have [Vision]." Grant the Vision keyword
    // via aura to every other friendly unit. Mirrors Forecaster's grant.
    // NOTE: the engine's play-time Vision peek reads obj.keywords.has(Vision),
    // not the aura-granted set, so an aura-only Vision unit won't peek on play
    // (documented engine gap shared with Forecaster); the keyword presence is
    // still correctly reflected via hasKeyword() for any reader that uses it.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        GameObjectId self_id = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            self_id = id;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (uid == self_id) continue;  // "other"
            if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                continue;
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Vision;
            u.aura_effects.push_back(ae);
        }
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
