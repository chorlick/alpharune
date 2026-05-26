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

class CaptainFarron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Other friendly units here have [Assault]." Grant the Assault keyword
    // (and its +1 [M] while attacking) to every OTHER friendly unit at the
    // same battlefield as me. Mirrors Rumble/Forecaster aura pattern, scoped
    // to "here" (same battlefield).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller)
                continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;  // only active while I'm at a battlefield
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;  // "other" — exclude myself
                if (!u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != my_bf) continue;  // "here"
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Assault;
                // Aura Assault grants +1 [M] while attacking (recomputeMight
                // only sums the dedicated assault_value, not keyword presence,
                // so model the +1 directly while the unit is an attacker).
                if (u.combat_designation == CombatDesignation::Attacker)
                    ae.might_bonus = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 15;
        d.def_id = R"RB(ogn-015-298)RB";
        d.name = R"RB(Captain Farron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-015/298)RB";
        d.collector_number = 15;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB(Other friendly units here have [Assault]. (+1 [M] while they're attackers.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4c65e2cae6748590f589ad8b26bc5a20c0d770c2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_15(CardRegistry& r) {
    r.registerCard(15, std::make_unique<CaptainFarron>());
}

} // namespace riftbound
