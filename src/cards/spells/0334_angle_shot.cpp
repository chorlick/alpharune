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

class AngleShot : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isReactionAbility() const override { return true; }
    bool needsPlayTimeTargetPair() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2};
    }

    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // First pick: any unit on board.
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            units.push_back(id);
        }
        std::sort(units.begin(), units.end());
        // Second pick: an Equipment gear with the SAME controller as the unit.
        auto second_fn = [&](GameObjectId picked_unit) {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(picked_unit)) return out;
            PlayerId uctrl = ctx.state.getObject(picked_unit).controller;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear() || !obj.location.has_value()) continue;
                if (obj.controller != uctrl) continue;
                bool is_equipment = false;
                for (auto& tag : obj.tags)
                    if (tag == "Equipment") { is_equipment = true; break; }
                if (!is_equipment) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        auto [unit, equip] = pickTargetPair(ctx, "Angle Shot: unit + Equipment",
                                            units, second_fn);
        bool suspending = (unit == kInvalidId || equip == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        // Attach/detach if both picked. Draw 1 unconditionally either way.
        if (unit != kInvalidId && equip != kInvalidId &&
            ctx.state.objectExists(unit) && ctx.state.objectExists(equip)) {
            auto& gear = ctx.state.getObject(equip);
            if (gear.attached_to.has_value() && *gear.attached_to == unit) {
                // Detach.
                ctx.executor.unattachGear(equip);
                ctx.events.logTrace("ANGLE SHOT: detached " + gear.name);
            } else {
                // Attach (free). Detach from prior bearer first.
                if (gear.attached_to.has_value() &&
                    ctx.state.objectExists(*gear.attached_to)) {
                    ctx.executor.unattachGear(equip);
                }
                auto& g = ctx.state.getObject(equip);
                auto& u = ctx.state.getObject(unit);
                g.attached_to = unit;
                u.attachments.push_back(equip);
                g.location = u.location;
                g.zone = u.zone;
                u.attachment_might_bonus += g.might_bonus;
                u.recomputeMight();
                ctx.events.emit(ObjectStateChangedEvent{equip, "attached"});
                ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
                ctx.events.logTrace("ANGLE SHOT: attached " + g.name + " -> " +
                                    u.name);
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("ANGLE SHOT: draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 334;
        d.def_id = R"RB(sfd-011-221)RB";
        d.name = R"RB(Angle Shot)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-011/221)RB";
        d.collector_number = 11;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a unit and an Equipment with the same controller. Attach that Equipment to that unit or detach that Equipment from that unit. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/757d507b684e55e333bac0ecdf2ec9ff3ad6045c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_334(CardRegistry& r) {
    r.registerCard(334, std::make_unique<AngleShot>());
}

} // namespace riftbound
