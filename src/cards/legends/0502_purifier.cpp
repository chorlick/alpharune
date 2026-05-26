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

class Purifier : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Confirm an on-board instance of this legend controlled by `controller`.
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            self_id = sid;
            break;
        }
        if (self_id == kInvalidId) return;
        // For each friendly Equipment that is attached to a unit, grant the
        // equipped unit [Assault] via an aura effect.
        for (auto& [gid, g] : state.objects) {
            if (g.controller != controller) continue;
            if (!isEquipment(g)) continue;
            if (!g.attached_to.has_value()) continue;
            auto uid = *g.attached_to;
            if (!state.objects.count(uid)) continue;
            auto& u = state.objects.at(uid);
            if (!u.isUnit()) continue;
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Assault;
            ae.keyword_value = 1;
            u.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 502;
        d.def_id = R"RB(sfd-183-221)RB";
        d.name = R"RB(Purifier)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-183/221)RB";
        d.collector_number = 183;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Lucian)RB"};
        d.rarity = Rarity::Rare;
        d.assault_value = 1;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB(Your Equipment each give [Assault]. (+1 [M] while equipped unit is an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/450cfb71b8890904d48b37a24bbdc78f8d849614-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_502(CardRegistry& r) {
    r.registerCard(502, std::make_unique<Purifier>());
}

} // namespace riftbound
