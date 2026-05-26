#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class BlackFlameAltar : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Find this battlefield's id (the BF whose card_object_id is an
        // instance of this card def).
        for (auto& [bid, bf_obj] : state.objects) {
            if (bf_obj.card_def_id != cardDefId()) continue;
            if (!bf_obj.isBattlefield()) continue;
            // Locate the BattlefieldState whose card_object_id == bid.
            std::optional<BattlefieldId> bf_id;
            for (const auto& bf : state.battlefields) {
                if (bf.card_object_id == bid) { bf_id = bf.id; break; }
            }
            if (!bf_id) continue;
            // Grant [Shield] to each unit at this BF that has [Temporary].
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != *bf_id) continue;
                if (!u.hasKeyword(Keyword::Temporary)) continue;
                GameObject::AuraEffect ae;
                ae.source = bid;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 764;
        d.def_id = R"RB(unl-208-219)RB";
        d.name = R"RB(Black Flame Altar)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-208/219)RB";
        d.collector_number = 208;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Units here with [Temporary] have [Shield]. (+1 [M] while they're defenders.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8cd4edde3e59c8edc8cdcea626fc443c5ed6e1f2-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_764(CardRegistry& r) {
    r.registerCard(764, std::make_unique<BlackFlameAltar>());
}

} // namespace riftbound
