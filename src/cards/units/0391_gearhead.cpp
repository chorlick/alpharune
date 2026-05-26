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

class Gearhead : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Accelerate]" engine-handled. "Each Equipment attached to me gives
    // double its base Might bonus." The unit already counts each gear's
    // base might_bonus once (attachment_might_bonus); add an aura granting
    // an EXTRA copy of each attached Equipment's base might bonus to double it.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            int extra = 0;
            for (auto att_id : self.attachments) {
                if (!state.objectExists(att_id)) continue;
                const auto& att = state.getObject(att_id);
                if (!isEquipment(att)) continue;
                extra += att.might_bonus;  // double = +1 more copy of base
            }
            if (extra == 0) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.might_bonus = extra;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 391;
        d.def_id = R"RB(sfd-068-221)RB";
        d.name = R"RB(Gearhead)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-068/221)RB";
        d.collector_number = 68;
        d.artist = R"RB(Six More Vodka & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.might = 3;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.)
Each Equipment attached to me gives double its base Might bonus.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/355b97bf135b8c60afabca7e4c6ecbf1aa25b4d9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_391(CardRegistry& r) {
    r.registerCard(391, std::make_unique<Gearhead>());
}

} // namespace riftbound
