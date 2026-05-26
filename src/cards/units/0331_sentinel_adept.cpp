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

class SentinelAdept : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // Collect the controller's Equipment gear on board (attached or not).
        std::vector<GameObjectId> equipment;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            bool is_equipment = false;
            for (auto& tag : obj.tags) {
                if (tag == "Equipment") { is_equipment = true; break; }
            }
            if (!is_equipment) continue;
            equipment.push_back(id);
        }
        auto still_legal = [&equipment]() { return !equipment.empty(); };
        if (!still_legal()) return;

        // "you may" — optional.
        int conf = confirmOptional(ctx, "Sentinel Adept: equip an Equipment to me?",
                                    still_legal);
        if (conf < 1) return;

        GameObjectId gear_id = pickTarget(ctx, "Sentinel Adept: pick Equipment", equipment);
        if (gear_id == kInvalidId || !ctx.state.objectExists(gear_id)) return;

        auto& gear = ctx.state.getObject(gear_id);

        // "even if it's already attached" — detach from current unit first.
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(), old.attachments.end(), gear_id);
                if (it != old.attachments.end()) old.attachments.erase(it);
                old.recomputeMight();
            }
            gear.attached_to = std::nullopt;
        }

        auto& self = ctx.state.getObject(ctx.source);
        gear.attached_to = ctx.source;
        self.attachments.push_back(gear_id);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();

        ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
        ctx.events.logTrace("SENTINEL ADEPT: equipped " + gear.name +
                            " (free / [A] less)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 331;
        d.def_id = R"RB(sfd-008-221)RB";
        d.name = R"RB(Sentinel Adept)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-008/221)RB";
        d.collector_number = 8;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Sentinel)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/94e213b77131d9a67e51743a35fc5dff80fd79ec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_331(CardRegistry& r) {
    r.registerCard(331, std::make_unique<SentinelAdept>());
}

} // namespace riftbound
