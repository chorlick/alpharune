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

class OrnnForgeGod : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Weaponmaster: find a friendly Equipment gear and attach it to me
        // (free — "[A] less" reduces the simple equip cost to nothing here).
        if (!ctx.state.objectExists(ctx.source)) return;
        GameObjectId best_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            bool is_equipment = false;
            for (auto& tag : obj.tags) {
                if (tag == "Equipment") { is_equipment = true; break; }
            }
            if (!is_equipment) continue;
            best_gear = id;
            break;
        }
        if (best_gear == kInvalidId) return;

        auto& gear = ctx.state.getObject(best_gear);
        // "even if it's already attached" — detach from current bearer first.
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(), old.attachments.end(), best_gear);
                if (it != old.attachments.end()) old.attachments.erase(it);
                old.recomputeMight();
            }
            gear.attached_to = std::nullopt;
        }
        auto& self = ctx.state.getObject(ctx.source);
        gear.attached_to = ctx.source;
        self.attachments.push_back(best_gear);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
        ctx.events.logTrace("ORNN: Weaponmaster equip " + gear.name);
    }

    // "I have +1 [M] for each friendly gear." Recomputed every aura pass.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Locate this OrnnForgeGod instance (on board, controlled by `controller`).
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            int gear_count = 0;
            for (auto& [gid, g] : state.objects) {
                if (g.isGear() && g.controller == controller && g.location.has_value())
                    ++gear_count;
            }
            if (gear_count <= 0) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = gear_count;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 407;
        d.def_id = R"RB(sfd-085-221)RB";
        d.name = R"RB(Ornn, Forge God)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-085/221)RB";
        d.collector_number = 85;
        d.artist = R"RB(Will Gist)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ornn)RB", R"RB(Freljord)RB"};
        d.energy_cost = 6;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.deflect_value = 2;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or ability.)
[Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
I have +1 [M] for each friendly gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f33c4ea9010ea5f440d4470554845c0ecae2c893-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_407(CardRegistry& r) {
    r.registerCard(407, std::make_unique<OrnnForgeGod>());
}

} // namespace riftbound
