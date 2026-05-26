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

class BreakneckMech : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Locate an on-board instance controlled by `controller`.
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            self_id = sid;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller || !u.location.has_value()) continue;
            if (!hasTag(u, "Mech")) continue;
            for (Keyword kw : {Keyword::Deflect, Keyword::Ganking}) {
                GameObject::AuraEffect ae;
                ae.source = self_id;
                ae.keyword = kw;
                u.aura_effects.push_back(ae);
            }
        }
    }

    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value())
                continue;
            if (obj.card_def_id == cardDefId()) continue;   // "another Mech" (skip self def)
            if (hasTag(obj, "Mech")) return true;
        }
        return false;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 394;
        d.def_id = R"RB(sfd-071-221)RB";
        d.name = R"RB(Breakneck Mech)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-071/221)RB";
        d.collector_number = 71;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(Your Mechs have [Deflect] and [Ganking]. (Opponents must pay [A] to choose us with a spell or ability. We can move from battlefield to battlefield.)
I enter ready if you control another Mech.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8564fc233cfcb4fc774ac4bc4ee72085d7586163-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_394(CardRegistry& r) {
    r.registerCard(394, std::make_unique<BreakneckMech>());
}

} // namespace riftbound
