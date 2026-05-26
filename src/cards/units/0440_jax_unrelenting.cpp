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

class JaxUnrelenting : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();

        switch (ri.resume_point) {
        case 0: {
            if (!ctx.state.objectExists(ctx.source)) return;

            // ── Weaponmaster: equip one of your Equipment to me ──
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

            if (best_gear != kInvalidId) {
                auto& gear = ctx.state.getObject(best_gear);
                // Detach from current unit ("even if already attached").
                if (gear.attached_to.has_value()) {
                    auto old_unit = *gear.attached_to;
                    if (ctx.state.objectExists(old_unit)) {
                        auto& old = ctx.state.getObject(old_unit);
                        old.attachment_might_bonus -= gear.might_bonus;
                        auto it = std::find(old.attachments.begin(),
                                            old.attachments.end(), best_gear);
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
                ctx.events.logTrace("JAX, UNRELENTING: equipped " + gear.name);
            } else {
                // Nothing equipped — nothing attached, so no rider.
                return;
            }
            // Fall through to the optional rider below.
            break;
        }
        }

        // ── Rider: "you may pay [1] to draw 1" (an Equipment was attached) ──
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Jax, Unrelenting: pay [1] to draw 1?",
                                    still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / no energy
        ps.rune_pool.energy -= 1;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("JAX, UNRELENTING: paid [1] -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 440;
        d.def_id = R"RB(sfd-119-221)RB";
        d.name = R"RB(Jax, Unrelenting)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-119/221)RB";
        d.collector_number = 119;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Jax)RB", R"RB(Icathia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
When you attach an Equipment to me, you may pay [1] to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e63ab3370a6e4ce4baddfba12ab92468f6cc9541-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_440(CardRegistry& r) {
    r.registerCard(440, std::make_unique<JaxUnrelenting>());
}

} // namespace riftbound
