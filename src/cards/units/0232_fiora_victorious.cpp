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

class FioraVictorious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]."
    // The keywords come before the Might arithmetic (CR 478-479), so the
    // [Shield] this ability grants must not count toward the Mighty test that
    // grants it: a buffed, defending Fiora who loses her buff drops straight
    // from 6 [M] to 4 [M] with no keywords (CR 478.4 example).
    // The hook runs before this pass has folded any Might, so `current_might`
    // is the previous pass's total; `__fiora_shield_folded` records whether
    // that total includes this ability's own +1 (granted while a defender).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        static const std::string kFolded = "__fiora_shield_folded";
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            if (!self.location.has_value()) { self.card_counters.erase(kFolded); continue; }
            int might = self.current_might;
            auto folded = self.card_counters.find(kFolded);
            if (folded != self.card_counters.end() && folded->second == 1) might -= 1;
            if (might < 5) {   // not [Mighty] without her own [Shield]
                self.card_counters.erase(kFolded);
                continue;
            }
            auraKeyword(self, sid, Keyword::Deflect);
            auraKeyword(self, sid, Keyword::Ganking);
            auraKeyword(self, sid, Keyword::Shield);
            // The engine's fold adds the [Shield] +1 while she is a defender.
            self.card_counters[kFolded] =
                self.combat_designation == CombatDesignation::Defender ? 1 : 0;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 232;
        d.def_id = R"RB(ogn-232-298)RB";
        d.name = R"RB(Fiora, Victorious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-232/298)RB";
        d.collector_number = 232;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Fiora)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]. (I'm Mighty while I have 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/24d4d1997e6a5b145412e402fc69399d590ecda0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_232(CardRegistry& r) {
    r.registerCard(232, std::make_unique<FioraVictorious>());
}

} // namespace riftbound
