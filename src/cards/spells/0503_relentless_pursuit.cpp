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

class RelentlessPursuit : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId unit = targets[0];

        // "Move a friendly unit." Best-effort: move to a battlefield where the
        // controller already has another unit, if the unit isn't already there.
        auto unit_bf = ctx.state.getObject(unit).battlefieldId();
        std::optional<BattlefieldId> dest;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == unit) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            if (unit_bf && *unit_bf == *bf) continue;   // already there
            dest = bf;
            break;
        }
        if (dest) {
            ctx.executor.moveToBattlefield(unit, *dest);
            ctx.events.logTrace("RELENTLESS PURSUIT: moved a friendly unit");
        }

        // "You may attach up to one Equipment with the same controller to it."
        GameObjectId gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller) continue;
            if (!isEquipment(obj) || !obj.location.has_value()) continue;
            if (obj.attached_to.has_value()) continue;   // unattached
            gear = id;
            break;
        }
        if (gear != kInvalidId && ctx.state.objectExists(gear)) {
            auto& g = ctx.state.getObject(gear);
            auto& u = ctx.state.getObject(unit);
            g.attached_to = unit;
            u.attachments.push_back(gear);
            g.location = u.location;
            g.zone = u.zone;
            u.attachment_might_bonus += g.might_bonus;
            u.recomputeMight();
            ctx.events.emit(ObjectStateChangedEvent{gear, "attached"});
            ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
            ctx.events.logTrace("RELENTLESS PURSUIT: attached " + g.name);
        }
        // The delayed "When I conquer, you may move me to my base" grant is a
        // documented no-op (no per-object delayed-ability injection API).
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 503;
        d.def_id = R"RB(sfd-184-221)RB";
        d.name = R"RB(Relentless Pursuit)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-184/221)RB";
        d.collector_number = 184;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Lucian)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit. You may attach up to one Equipment with the same controller to it. This turn, that unit has "When I conquer, you may move me to my base.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/48754730486abefdb56221d0760b9c963aae7c09-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_503(CardRegistry& r) {
    r.registerCard(503, std::make_unique<RelentlessPursuit>());
}

} // namespace riftbound
