#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AlphaWildclaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Tank]" is engine-handled.
    // "Your units here with less Might than me can't be chosen by enemy spells
    // and abilities." — set on those units each aura pass (the engine clears
    // the flag before any hook runs). It compares Might, so it is decided in
    // the Might-dependent hook, on this pass's Might.
    bool hasMightDependentAura() const override { return true; }
    void applyMightDependentAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects.onBoard()) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            const auto here = self.battlefieldId();
            if (!here.has_value()) continue;   // "here" is a battlefield
            for (auto& [uid, u] : state.objects.onBoard()) {
                if (uid == sid || !u.isUnit() || u.controller != controller) continue;
                if (u.battlefieldId() != here) continue;
                if (u.current_might < self.current_might) u.untargetable_by_enemy = true;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 619;
        d.def_id = R"RB(unl-057-219)RB";
        d.name = R"RB(Alpha Wildclaw)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-057/219)RB";
        d.collector_number = 57;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Cat)RB", R"RB(Freljord)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
Your units here with less Might than me can't be chosen by enemy spells and abilities.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/39ec974dae18bf82fcc18a95044d8df04cc14d3c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_619(CardRegistry& r) {
    r.registerCard(619, std::make_unique<AlphaWildclaw>());
}

} // namespace riftbound
