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

class OrnnSForge : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Each Forge gives ITS controller the reduction, once: the hook runs
        // once per battlefield slot holding this card and is not told which,
        // so it walks every such slot and skips one already handled this pass
        // (the engine drops aura-sourced modifiers at the top of each pass).
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            const auto& bf_obj = state.getObject(bf.card_object_id);
            if (bf_obj.card_def_id != cardDefId()) continue;
            if (!bf.controller.has_value()) continue;
            PlayerId who = *bf.controller;
            auto& mods = state.player(who).cost_modifiers;
            bool already = false;
            for (const auto& have : mods)
                if (have.source == bf.card_object_id && have.first_gear_per_turn) already = true;
            if (already) continue;
            PlayerState::CostModifier m;
            m.source = bf.card_object_id;
            m.energy_reduction = 1;
            m.gear_only = true;
            m.first_gear_per_turn = true;
            m.affects_non_token_only = true;
            m.affects_friendly_only = true;
            m.this_turn_only = false;
            mods.push_back(m);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 527;
        d.def_id = R"RB(sfd-213-221)RB";
        d.name = R"RB(Ornn's Forge)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-213/221)RB";
        d.collector_number = 213;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While you control this battlefield, the first friendly non-token gear played each turn costs [1] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e228948bc7a6d8ee858fbc0bed3d93a53f467097-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_527(CardRegistry& r) {
    r.registerCard(527, std::make_unique<OrnnSForge>());
}

} // namespace riftbound
